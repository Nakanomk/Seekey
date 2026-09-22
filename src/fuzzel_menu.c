#include "gui.h"

#include "config.h"
#include "preview_session.h"
#include "tui.h"
#include "window_state.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

/* The fuzzel-driven configuration menu. Each page spawns one short-lived
 * `fuzzel --dmenu` session; the selected line (or the typed value on input
 * pages) is matched back to an action here. Input pages rely on --search and
 * --prompt-only from fuzzel 1.11. List pages additionally use --only-match
 * when the installed fuzzel supports it (1.14+), preventing free-typed text
 * from closing and reopening a menu page. The probe in
 * seekey_fuzzel_menu_run() gates everything and lets gui.c fall back to the
 * built-in GTK menu when fuzzel is missing, too old, or its configuration
 * cannot be parsed. */

typedef struct {
    SeekeyConfig *config;
    SeekeyPreviewSession *preview;
    gboolean dirty;
    gboolean overlay_running;
    gboolean failed;
    gboolean only_match;
    GError *error;
    TuiField fields[TUI_FIELD_COUNT];
    size_t field_count;
} FuzzelMenu;

typedef enum {
    ENTRY_BACK,
    ENTRY_QUIT,
    ENTRY_START,
    ENTRY_STOP,
    ENTRY_MATUGEN,
    ENTRY_GROUP,
    ENTRY_FIELD,
    ENTRY_SET_BOOL,
    ENTRY_SET_CHOICE,
    ENTRY_OPEN_DESKTOP,
    ENTRY_DESKTOP_MODE,
    ENTRY_SAVE,
    ENTRY_RELOAD,
} EntryType;

typedef struct {
    EntryType type;
    guint index;
    gboolean bool_value;
    const char *string_value;
    char *label; /* full line shown to the user and sent to fuzzel */
} MenuEntry;

typedef enum {
    FUZZEL_OK,     /* exit 0: `line` holds the selected/typed text */
    FUZZEL_CANCEL, /* fuzzel exited without a selection (Esc, etc.) */
    FUZZEL_ERROR,  /* spawn or communication failure; error is set */
} FuzzelResult;

/* ------------------------------------------------------------------ */
/* Menu state helpers                                                  */
/* ------------------------------------------------------------------ */

/* Record the first failure; every page checks `m->failed` and unwinds so
 * seekey_fuzzel_menu_run() can hand the error to the GTK fallback. */
static void menu_fail(FuzzelMenu *m, GError *error)
{
    if (m->error == NULL) {
        m->error = error;
    } else {
        g_error_free(error);
    }
    m->failed = TRUE;
}

static void start_preview(FuzzelMenu *m)
{
    if (m->preview != NULL) return;

    GError *error = NULL;
    m->preview = seekey_preview_session_start(m->config, &error);
    if (m->preview == NULL) {
        g_printerr("seekey: preview unavailable: %s\n",
                   error != NULL ? error->message : "unknown error");
        g_clear_error(&error);
    }
}

static void sync_preview(FuzzelMenu *m)
{
    if (m->preview == NULL) return;

    GError *error = NULL;
    if (!seekey_preview_session_sync(m->preview, m->config, &error)) {
        g_printerr("seekey: preview update failed: %s\n", error->message);
        g_clear_error(&error);
    }
}

static void stop_preview(FuzzelMenu *m)
{
    seekey_preview_session_free(m->preview);
    m->preview = NULL;
}

static void menu_entry_free(gpointer data)
{
    MenuEntry *entry = data;
    g_free(entry->label);
    g_free(entry);
}

/* Adds a borrowed label; the caller keeps ownership (static strings). */
static MenuEntry *menu_entry_add(GPtrArray *entries, EntryType type,
                                 const char *label)
{
    MenuEntry *entry = g_new0(MenuEntry, 1);
    entry->type = type;
    entry->label = g_strdup(label);
    g_ptr_array_add(entries, entry);
    return entry;
}

/* Adds a heap-allocated label; the menu takes ownership. */
static MenuEntry *menu_entry_add_take(GPtrArray *entries, EntryType type,
                                      char *label)
{
    MenuEntry *entry = g_new0(MenuEntry, 1);
    entry->type = type;
    entry->label = label;
    g_ptr_array_add(entries, entry);
    return entry;
}

static void menu_entry_set_value(MenuEntry *entry, const char *value)
{
    char *label = g_strdup_printf("%-24s %s", entry->label, value);
    g_free(entry->label);
    entry->label = label;
}

/* ------------------------------------------------------------------ */
/* fuzzel plumbing                                                     */
/* ------------------------------------------------------------------ */

static void fuzzel_child_setup(gpointer user_data)
{
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() != (pid_t)GPOINTER_TO_SIZE(user_data)) _exit(1);
}

/* Run one fuzzel session. `stdin_text` may be "" (nothing to send). On
 * FUZZEL_OK the returned line has trailing newlines stripped; NULL means
 * fuzzel exited 0 without printing anything. */
static FuzzelResult fuzzel_run(char **argv, const char *stdin_text, char **line,
                               GError **error)
{
    /* g_spawn + child_setup rather than GSubprocess: this mirrors
     * preview_session.c, whose PDEATHSIG + parent-pid guard is known to
     * run on the direct fork — GSubprocess may spawn through a helper
     * where getppid() is the helper and the guard would misfire. Pipe
     * volumes here are tiny (menu entries), so a single buffered write
     * followed by a read-to-EOF cannot deadlock. */
    GPid pid = 0;
    gint stdin_fd = -1;
    gint stdout_fd = -1;
    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                  fuzzel_child_setup,
                                  GSIZE_TO_POINTER((gsize)getpid()), &pid,
                                  &stdin_fd, &stdout_fd, NULL, error)) {
        return FUZZEL_ERROR;
    }

    gsize remaining = strlen(stdin_text);
    while (remaining > 0) {
        ssize_t written = write(stdin_fd, stdin_text, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;
            break; /* fuzzel closed stdin early; stdout still tells us why */
        }
        stdin_text += written;
        remaining -= (gsize)written;
    }
    close(stdin_fd);

    GString *out = g_string_new(NULL);
    for (;;) {
        char buffer[4096];
        ssize_t count = read(stdout_fd, buffer, sizeof(buffer));
        if (count > 0) {
            g_string_append_len(out, buffer, count);
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            break;
        }
    }
    close(stdout_fd);

    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    g_spawn_close_pid(pid);

    if (waited < 0 || !WIFEXITED(status)) {
        g_string_free(out, TRUE);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "fuzzel terminated abnormally");
        return FUZZEL_ERROR;
    }
    if (WEXITSTATUS(status) != 0) {
        /* dmenu convention (fuzzel >= 1.11): cancel exits 2. Compositor
         * closes and focus-loss exits 1; both mean "no selection". */
        g_string_free(out, TRUE);
        return FUZZEL_CANCEL;
    }

    char *stdout_text = g_string_free(out, FALSE);
    if (stdout_text != NULL) {
        gsize length = strlen(stdout_text);
        while (length > 0 && (stdout_text[length - 1] == '\n' ||
                              stdout_text[length - 1] == '\r')) {
            stdout_text[--length] = '\0';
        }
    }
    if (line != NULL) {
        *line = stdout_text;
    } else {
        g_free(stdout_text);
    }
    return FUZZEL_OK;
}

/* Show a list page. On FUZZEL_OK, `*index` is the chosen entry, or -1
 * when the returned text matches no entry (free-typed input). */
static FuzzelResult fuzzel_choose_page(const FuzzelMenu *m, const char *fuzzel,
                                       const char *prompt, GPtrArray *entries,
                                       gint *index, GError **error)
{
    *index = -1;

    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, (gpointer)fuzzel);
    g_ptr_array_add(argv, "--dmenu");
    if (m->only_match) {
        g_ptr_array_add(argv, "--only-match");
    }
    g_ptr_array_add(argv, "--log-level=none");
    g_ptr_array_add(argv, "--prompt");
    g_ptr_array_add(argv, (gpointer)prompt);
    g_ptr_array_add(argv, NULL);

    GString *stdin_text = g_string_new(NULL);
    for (guint i = 0; i < entries->len; i++) {
        MenuEntry *entry = g_ptr_array_index(entries, i);
        g_string_append(stdin_text, entry->label);
        g_string_append_c(stdin_text, '\n');
    }

    char *line = NULL;
    FuzzelResult result =
        fuzzel_run((char **)argv->pdata, stdin_text->str, &line, error);

    if (result == FUZZEL_OK && line != NULL) {
        for (guint i = 0; i < entries->len; i++) {
            MenuEntry *entry = g_ptr_array_index(entries, i);
            if (g_strcmp0(line, entry->label) == 0) {
                *index = (gint)i;
                break;
            }
        }
    }

    g_free(line);
    g_string_free(stdin_text, TRUE);
    g_ptr_array_unref(argv);
    return result;
}

/* Show a pure input page: no entry list, the input box pre-filled with
 * `prefill`. On FUZZEL_OK, `*value` holds the typed text ("" when the
 * user confirmed without typing). */
static FuzzelResult fuzzel_ask_value(const char *fuzzel, const char *prompt,
                                     const char *prefill, char **value,
                                     GError **error)
{
    *value = NULL;

    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, (gpointer)fuzzel);
    g_ptr_array_add(argv, "--dmenu");
    g_ptr_array_add(argv, "--log-level=none");
    g_ptr_array_add(argv, "--prompt-only");
    g_ptr_array_add(argv, (gpointer)prompt);
    if (prefill != NULL && prefill[0] != '\0') {
        g_ptr_array_add(argv, "--search");
        g_ptr_array_add(argv, (gpointer)prefill);
    }
    g_ptr_array_add(argv, NULL);

    char *line = NULL;
    FuzzelResult result = fuzzel_run((char **)argv->pdata, "", &line, error);
    if (result == FUZZEL_OK) {
        *value = line != NULL ? line : g_strdup("");
    }

    g_ptr_array_unref(argv);
    return result;
}

/* fuzzel >= 1.11 provides --search/--prompt-only; --check-config (1.10+)
 * validates both the flags and the user's fuzzel.ini without showing a
 * window. fuzzel 1.14 added --only-match, which is useful but optional. */
static gboolean fuzzel_check_config(const char *const *argv, GError **error)
{
    GSubprocess *sub = g_subprocess_newv(argv,
                                         G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                             G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                         error);
    if (sub == NULL) {
        return FALSE;
    }

    gboolean waited = g_subprocess_wait(sub, NULL, error);
    gboolean ok = waited && g_subprocess_get_if_exited(sub) &&
                  g_subprocess_get_exit_status(sub) == 0;
    g_object_unref(sub);
    return ok;
}

static gboolean fuzzel_menu_probe(char **fuzzel_out, gboolean *only_match_out)
{
    *only_match_out = FALSE;
    char *path = g_find_program_in_path("fuzzel");
    if (path == NULL) {
        return FALSE;
    }

    const char *base_argv[] = {
        path,
        "--check-config",
        "--dmenu",
        "--prompt-only=seekey",
        "--search=seekey",
        "--log-level=none",
        NULL,
    };
    GError *error = NULL;
    if (!fuzzel_check_config(base_argv, &error)) {
        if (error != NULL) {
            g_printerr("seekey: cannot probe fuzzel: %s\n", error->message);
        } else {
            g_printerr("seekey: fuzzel probe failed (needs fuzzel >= 1.11 with"
                       " a valid fuzzel.ini); using the built-in menu\n");
        }
        g_clear_error(&error);
        g_free(path);
        return FALSE;
    }

    const char *only_match_argv[] = {
        path,           "--check-config",   "--dmenu",
        "--only-match", "--log-level=none", NULL,
    };
    *only_match_out = fuzzel_check_config(only_match_argv, &error);
    g_clear_error(&error);

    *fuzzel_out = path;
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Pages                                                               */
/* ------------------------------------------------------------------ */

/* Returns TRUE when the menu is done (overlay started or user quit). */
static gboolean page_root(FuzzelMenu *m, const char *fuzzel);

static gboolean page_quit_confirm(FuzzelMenu *m, const char *fuzzel)
{
    for (;;) {
        if (m->failed) return FALSE;

        GError *error = NULL;
        GPtrArray *entries = g_ptr_array_new_with_free_func(menu_entry_free);
        MenuEntry *save =
            menu_entry_add(entries, ENTRY_QUIT, _("Save changes and quit"));
        save->bool_value = TRUE;
        MenuEntry *discard =
            menu_entry_add(entries, ENTRY_QUIT, _("Discard changes and quit"));
        discard->bool_value = FALSE;
        menu_entry_add(entries, ENTRY_BACK, _("← Back"));

        gint index = -1;
        char *prompt = g_strdup_printf("%s > ", _("Unsaved changes"));
        FuzzelResult result =
            fuzzel_choose_page(m, fuzzel, prompt, entries, &index, &error);
        g_free(prompt);
        MenuEntry *chosen =
            index >= 0 ? g_ptr_array_index(entries, (guint)index) : NULL;
        EntryType type = chosen != NULL ? chosen->type : ENTRY_BACK;
        gboolean save_first = chosen != NULL && chosen->bool_value;
        g_ptr_array_unref(entries);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            return FALSE;
        }
        if (result == FUZZEL_CANCEL) {
            return FALSE;
        }
        if (index < 0) {
            continue; /* free-typed text matched nothing: ask again */
        }
        if (type == ENTRY_BACK) {
            return FALSE;
        }
        if (type == ENTRY_QUIT && save_first && !seekey_menu_save(m->config)) {
            continue; /* save failed: ask again */
        }
        return TRUE;
    }
}

static void page_input(FuzzelMenu *m, const char *fuzzel, TuiField *field)
{
    char current[512];
    tui_field_value(field, current, sizeof(current));
    char *prefill = g_strdup(current);
    gboolean invalid = FALSE;

    for (;;) {
        if (m->failed) break;

        char *prompt;
        if (invalid) {
            prompt =
                g_strdup_printf("%s — %s > ", field->label, _("Invalid value"));
        } else if (field->type == TUI_UINT) {
            prompt =
                g_strdup_printf("%s (%s) > ", field->label, field->input_hint);
        } else {
            prompt = g_strdup_printf("%s > ", field->label);
        }

        GError *error = NULL;
        char *value = NULL;
        FuzzelResult result =
            fuzzel_ask_value(fuzzel, prompt, prefill, &value, &error);
        g_free(prompt);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            break;
        }
        if (result == FUZZEL_CANCEL) {
            g_free(value);
            break;
        }
        if (value != NULL && value[0] == '\0') {
            /* Empty input keeps the current value (like the TUI). */
            g_free(value);
            break;
        }
        if (value != NULL && tui_field_input_valid(field, value)) {
            tui_field_apply_input(field, value);
            m->dirty = TRUE;
            sync_preview(m);
            g_free(value);
            break;
        }

        /* Invalid: re-ask with the rejected text pre-filled. */
        g_free(prefill);
        prefill = value;
        invalid = TRUE;
    }
    g_free(prefill);
}

static void page_choice(FuzzelMenu *m, const char *fuzzel, TuiField *field)
{
    for (;;) {
        if (m->failed) return;

        GError *error = NULL;
        GPtrArray *entries = g_ptr_array_new_with_free_func(menu_entry_free);
        for (guint i = 0; i < field->choice_count; i++) {
            char *label = g_strdup_printf(
                "%s%s",
                g_strcmp0(field->string_target, field->choices[i]) == 0 ? "* "
                                                                        : "  ",
                field->choices[i]);
            MenuEntry *entry =
                menu_entry_add_take(entries, ENTRY_SET_CHOICE, label);
            entry->string_value = field->choices[i];
        }
        menu_entry_add(entries, ENTRY_BACK, _("← Back"));

        gint index = -1;
        char *prompt = g_strdup_printf("%s > ", field->label);
        FuzzelResult result =
            fuzzel_choose_page(m, fuzzel, prompt, entries, &index, &error);
        g_free(prompt);
        MenuEntry *chosen =
            index >= 0 ? g_ptr_array_index(entries, (guint)index) : NULL;
        EntryType type = chosen != NULL ? chosen->type : ENTRY_BACK;
        const char *string_value = chosen != NULL ? chosen->string_value : NULL;
        g_ptr_array_unref(entries);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            return;
        }
        if (result == FUZZEL_CANCEL || type == ENTRY_BACK) {
            return;
        }
        if (type == ENTRY_SET_CHOICE) {
            g_strlcpy(field->string_target, string_value, field->string_size);
            if (g_strcmp0(field->label, "theme") == 0) {
                seekey_config_apply_theme(m->config, string_value);
            }
            m->dirty = TRUE;
            sync_preview(m);
            return;
        }
        /* FUZZEL_OK with free-typed text: re-show the page. */
    }
}

static void page_bool(FuzzelMenu *m, const char *fuzzel, TuiField *field)
{
    for (;;) {
        if (m->failed) return;

        GError *error = NULL;
        GPtrArray *entries = g_ptr_array_new_with_free_func(menu_entry_free);
        char *enabled = g_strdup_printf(
            "%s%s", *field->bool_target ? "* " : "  ", _("Enabled"));
        MenuEntry *enabled_entry =
            menu_entry_add_take(entries, ENTRY_SET_BOOL, enabled);
        enabled_entry->bool_value = TRUE;
        char *disabled = g_strdup_printf(
            "%s%s", *field->bool_target ? "  " : "* ", _("Disabled"));
        MenuEntry *disabled_entry =
            menu_entry_add_take(entries, ENTRY_SET_BOOL, disabled);
        disabled_entry->bool_value = FALSE;
        menu_entry_add(entries, ENTRY_BACK, _("← Back"));

        gint index = -1;
        char *prompt = g_strdup_printf("%s > ", field->label);
        FuzzelResult result =
            fuzzel_choose_page(m, fuzzel, prompt, entries, &index, &error);
        g_free(prompt);
        MenuEntry *chosen =
            index >= 0 ? g_ptr_array_index(entries, (guint)index) : NULL;
        EntryType type = chosen != NULL ? chosen->type : ENTRY_BACK;
        gboolean bool_value = chosen != NULL ? chosen->bool_value : FALSE;
        g_ptr_array_unref(entries);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            return;
        }
        if (result == FUZZEL_CANCEL || type == ENTRY_BACK) {
            return;
        }
        if (type == ENTRY_SET_BOOL) {
            *field->bool_target = bool_value;
            m->dirty = TRUE;
            sync_preview(m);
            return;
        }
    }
}

static void page_field(FuzzelMenu *m, const char *fuzzel, TuiField *field)
{
    switch (field->type) {
    case TUI_BOOL:
        page_bool(m, fuzzel, field);
        break;
    case TUI_CHOICE:
        page_choice(m, fuzzel, field);
        break;
    case TUI_UINT:
    case TUI_STRING:
    case TUI_COLOR:
        page_input(m, fuzzel, field);
        break;
    }
}

static void page_group(FuzzelMenu *m, const char *fuzzel, TuiGroup group)
{
    for (;;) {
        if (m->failed) return;

        GError *error = NULL;
        GPtrArray *entries = g_ptr_array_new_with_free_func(menu_entry_free);
        for (size_t i = 0; i < m->field_count; i++) {
            if (m->fields[i].group != group) continue;
            char value[256];
            tui_field_value(&m->fields[i], value, sizeof(value));
            char *label =
                g_strdup_printf("%-24s %s", m->fields[i].label, value);
            MenuEntry *entry = menu_entry_add_take(entries, ENTRY_FIELD, label);
            entry->index = (guint)i;
        }
        menu_entry_add(entries, ENTRY_BACK, _("← Back"));

        char *prompt = g_strdup_printf("%s%s > ", tui_group_name(group),
                                       m->dirty ? " *" : "");
        gint index = -1;
        FuzzelResult result =
            fuzzel_choose_page(m, fuzzel, prompt, entries, &index, &error);
        g_free(prompt);
        MenuEntry *chosen =
            index >= 0 ? g_ptr_array_index(entries, (guint)index) : NULL;
        EntryType type = chosen != NULL ? chosen->type : ENTRY_BACK;
        guint field_index = chosen != NULL ? chosen->index : 0;
        g_ptr_array_unref(entries);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            return;
        }
        if (result == FUZZEL_CANCEL || type == ENTRY_BACK) {
            return;
        }
        if (index < 0) {
            continue; /* free-typed text matched nothing: re-show */
        }
        if (type == ENTRY_FIELD) {
            page_field(m, fuzzel, &m->fields[field_index]);
        }
    }
}

static void page_desktop(FuzzelMenu *m, const char *fuzzel)
{
    for (;;) {
        if (m->failed) return;

        GError *error = NULL;
        SeekeyWindowState saved = {0};
        seekey_window_state_load(&saved, NULL);

        GPtrArray *entries = g_ptr_array_new_with_free_func(menu_entry_free);
        char *menu_label = g_strdup_printf(
            "%s%s",
            saved.desktop_preference_set && saved.desktop_show_menu ? "* "
                                                                    : "  ",
            _("Open the Seekey menu"));
        MenuEntry *menu_choice =
            menu_entry_add_take(entries, ENTRY_DESKTOP_MODE, menu_label);
        menu_choice->bool_value = TRUE;
        char *overlay_label = g_strdup_printf(
            "%s%s",
            saved.desktop_preference_set && !saved.desktop_show_menu ? "* "
                                                                     : "  ",
            _("Start the key overlay directly"));
        MenuEntry *overlay_choice =
            menu_entry_add_take(entries, ENTRY_DESKTOP_MODE, overlay_label);
        overlay_choice->bool_value = FALSE;
        menu_entry_add(entries, ENTRY_BACK, _("← Back"));

        gint index = -1;
        char *prompt = g_strdup_printf("%s > ", _("Desktop launcher"));
        FuzzelResult result =
            fuzzel_choose_page(m, fuzzel, prompt, entries, &index, &error);
        g_free(prompt);
        MenuEntry *chosen =
            index >= 0 ? g_ptr_array_index(entries, (guint)index) : NULL;
        EntryType type = chosen != NULL ? chosen->type : ENTRY_BACK;
        gboolean bool_value = chosen != NULL ? chosen->bool_value : FALSE;
        g_ptr_array_unref(entries);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            return;
        }
        if (result == FUZZEL_CANCEL || type == ENTRY_BACK) {
            return;
        }
        if (index < 0) {
            continue; /* free-typed text matched nothing: re-show */
        }
        if (type == ENTRY_DESKTOP_MODE) {
            seekey_menu_save_desktop_preference(bool_value);
            return;
        }
    }
}

/* First desktop launch chooser. Returns TRUE when the menu is done
 * (overlay started or the user dismissed the page). */
static gboolean page_first_run(FuzzelMenu *m, const char *fuzzel)
{
    for (;;) {
        if (m->failed) return TRUE;

        GError *error = NULL;
        GPtrArray *entries = g_ptr_array_new_with_free_func(menu_entry_free);
        MenuEntry *menu_choice = menu_entry_add_take(
            entries, ENTRY_DESKTOP_MODE,
            g_strdup_printf("%-24s %s", _("Open the Seekey menu"),
                            _("Recommended")));
        menu_choice->bool_value = TRUE;
        MenuEntry *overlay_choice = menu_entry_add(
            entries, ENTRY_DESKTOP_MODE, _("Start the key overlay directly"));
        overlay_choice->bool_value = FALSE;

        gint index = -1;
        char *prompt = g_strdup_printf("%s > ", _("First launch"));
        FuzzelResult result =
            fuzzel_choose_page(m, fuzzel, prompt, entries, &index, &error);
        g_free(prompt);
        MenuEntry *chosen =
            index >= 0 ? g_ptr_array_index(entries, (guint)index) : NULL;
        EntryType type = chosen != NULL ? chosen->type : ENTRY_DESKTOP_MODE;
        gboolean bool_value = chosen != NULL ? chosen->bool_value : TRUE;
        g_ptr_array_unref(entries);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            return TRUE;
        }
        if (result == FUZZEL_CANCEL) {
            return TRUE;
        }
        if (index < 0) {
            continue; /* free-typed text matched nothing: re-show */
        }
        if (type == ENTRY_DESKTOP_MODE) {
            seekey_menu_save_desktop_preference(bool_value);
            if (bool_value) {
                return FALSE; /* continue into the regular menu */
            }
            stop_preview(m);
            if (seekey_menu_launch_overlay(m->config)) {
                return TRUE;
            }
            start_preview(m);
            return FALSE;
        }
    }
}

static gboolean page_root(FuzzelMenu *m, const char *fuzzel)
{
    for (;;) {
        if (m->failed) return TRUE;

        GError *error = NULL;
        GPtrArray *entries = g_ptr_array_new_with_free_func(menu_entry_free);

        if (m->overlay_running) {
            menu_entry_add(entries, ENTRY_STOP, _("Stop key overlay"));
        } else {
            char *label =
                m->dirty ? g_strdup_printf("%s (%s)", _("Start key overlay"),
                                           _("Save first"))
                         : g_strdup(_("Start key overlay"));
            menu_entry_add_take(entries, ENTRY_START, label);
        }

        if (seekey_menu_matugen_available(m->config)) {
            char *label =
                g_strcmp0(m->config->theme, "matugen") == 0
                    ? g_strdup_printf("%s (%s)", _("Use Matugen colors"),
                                      _("Active"))
                    : g_strdup(_("Use Matugen colors"));
            menu_entry_add_take(entries, ENTRY_MATUGEN, label);
        }

        for (int group = 0; group < TUI_GROUP_COUNT; group++) {
            char *label = g_strdup_printf(
                "%s [%zu]", tui_group_name((TuiGroup)group),
                tui_count_in_group(m->fields, m->field_count, (TuiGroup)group));
            MenuEntry *entry = menu_entry_add_take(entries, ENTRY_GROUP, label);
            entry->index = (guint)group;
        }

        SeekeyWindowState desktop = {0};
        seekey_window_state_load(&desktop, NULL);
        const char *desktop_value = !desktop.desktop_preference_set
                                        ? _("Not chosen")
                                    : desktop.desktop_show_menu ? _("Menu")
                                                                : _("Overlay");
        char *desktop_label =
            g_strdup_printf("%s: %s", _("Desktop launcher"), desktop_value);
        menu_entry_add_take(entries, ENTRY_OPEN_DESKTOP, desktop_label);

        MenuEntry *save =
            menu_entry_add(entries, ENTRY_SAVE, _("Save configuration"));
        menu_entry_set_value(save, m->dirty ? _("Unsaved") : _("Saved"));
        menu_entry_add(entries, ENTRY_RELOAD, _("Reload configuration"));
        menu_entry_add(entries, ENTRY_QUIT, _("Quit"));

        char *prompt = g_strdup_printf("Seekey%s > ", m->dirty ? " *" : "");
        gint index = -1;
        FuzzelResult result =
            fuzzel_choose_page(m, fuzzel, prompt, entries, &index, &error);
        g_free(prompt);
        MenuEntry *chosen =
            index >= 0 ? g_ptr_array_index(entries, (guint)index) : NULL;
        EntryType type = chosen != NULL ? chosen->type : ENTRY_QUIT;
        guint action_index = chosen != NULL ? chosen->index : 0;
        g_ptr_array_unref(entries);

        if (result == FUZZEL_ERROR) {
            menu_fail(m, error);
            return TRUE;
        }
        if (result == FUZZEL_OK && index < 0) {
            continue; /* free-typed text matched nothing: re-show */
        }
        if (result == FUZZEL_CANCEL) {
            type = ENTRY_QUIT; /* Esc at the root asks to quit */
        }

        switch (type) {
        case ENTRY_START:
            if (m->dirty && !seekey_menu_save(m->config)) {
                break;
            }
            stop_preview(m);
            if (seekey_menu_launch_overlay(m->config)) {
                m->overlay_running = TRUE;
                return TRUE;
            }
            start_preview(m);
            break;
        case ENTRY_STOP:
            if (seekey_menu_stop_overlay(&error)) {
                m->overlay_running = FALSE;
                start_preview(m);
            } else {
                g_printerr("seekey: cannot stop overlay: %s\n", error->message);
                g_clear_error(&error);
                m->overlay_running = seekey_menu_overlay_running();
            }
            break;
        case ENTRY_MATUGEN:
            g_strlcpy(m->config->theme, "matugen", sizeof(m->config->theme));
            seekey_config_apply_theme(m->config, "matugen");
            m->dirty = TRUE;
            sync_preview(m);
            break;
        case ENTRY_GROUP:
            page_group(m, fuzzel, (TuiGroup)action_index);
            break;
        case ENTRY_OPEN_DESKTOP:
            page_desktop(m, fuzzel);
            break;
        case ENTRY_SAVE:
            if (seekey_menu_save(m->config)) {
                m->dirty = FALSE;
            }
            break;
        case ENTRY_RELOAD:
            if (seekey_config_reload(m->config, &error)) {
                tui_build_fields(m->fields, &m->field_count, m->config);
                m->dirty = FALSE;
                sync_preview(m);
            } else {
                g_printerr("seekey: %s\n", error->message);
                g_clear_error(&error);
            }
            break;
        case ENTRY_QUIT:
            if (m->dirty && !page_quit_confirm(m, fuzzel)) {
                break;
            }
            return TRUE;
        case ENTRY_BACK:
        case ENTRY_FIELD:
        case ENTRY_SET_BOOL:
        case ENTRY_SET_CHOICE:
        case ENTRY_DESKTOP_MODE:
            break;
        }
    }
}

gboolean seekey_fuzzel_menu_run(SeekeyConfig *config,
                                gboolean first_desktop_launch, GError **error)
{
    /* Every page needs a Wayland surface; without a session the probe
     * would still pass (--check-config does not connect) and the menu
     * would silently do nothing. */
    if (g_getenv("WAYLAND_DISPLAY") == NULL &&
        g_getenv("WAYLAND_SOCKET") == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "no Wayland session for the fuzzel menu");
        return FALSE;
    }

    char *fuzzel = NULL;
    gboolean only_match = FALSE;
    if (!fuzzel_menu_probe(&fuzzel, &only_match)) {
        return FALSE;
    }

    FuzzelMenu m = {0};
    m.config = config;
    m.only_match = only_match;
    m.overlay_running = seekey_menu_overlay_running();
    tui_build_fields(m.fields, &m.field_count, config);
    if (!m.overlay_running) {
        start_preview(&m);
    }

    gboolean done = FALSE;
    if (first_desktop_launch) {
        done = page_first_run(&m, fuzzel);
    }
    if (!done) {
        page_root(&m, fuzzel);
    }

    stop_preview(&m);
    g_free(fuzzel);

    if (m.failed) {
        if (error != NULL) {
            g_propagate_prefixed_error(error, m.error, "fuzzel menu failed: ");
        } else {
            g_printerr("seekey: fuzzel menu failed: %s\n", m.error->message);
            g_error_free(m.error);
        }
        return FALSE;
    }
    return TRUE;
}
