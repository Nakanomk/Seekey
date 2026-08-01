#include "gui.h"

#include "config.h"
#include "preview_session.h"
#include "tui.h"
#include "window_state.h"

#include <errno.h>
#include <string.h>

typedef enum {
    MENU_PAGE_ROOT,
    MENU_PAGE_GROUP,
    MENU_PAGE_BOOL,
    MENU_PAGE_CHOICE,
    MENU_PAGE_INPUT,
    MENU_PAGE_FONT,
    MENU_PAGE_DESKTOP,
    MENU_PAGE_FIRST_RUN,
} MenuPage;

typedef enum {
    MENU_ACTION_BACK,
    MENU_ACTION_START,
    MENU_ACTION_GROUP,
    MENU_ACTION_FIELD,
    MENU_ACTION_SET_BOOL,
    MENU_ACTION_SET_CHOICE,
    MENU_ACTION_SET_FONT,
    MENU_ACTION_APPLY_INPUT,
    MENU_ACTION_OPEN_DESKTOP,
    MENU_ACTION_DESKTOP_MODE,
    MENU_ACTION_SAVE,
    MENU_ACTION_RELOAD,
} MenuActionType;

typedef struct {
    char font_family[128];
    guint font_size;
    guint row_height;
    guint lines;
    guint width_chars;
    guint horizontal_pad;
    guint vertical_pad;
    guint inner_pad;
    guint border_width;
    guint border_radius;
    guint selection_radius;
    char background[16];
    char text[16];
    char prompt[16];
    char placeholder[16];
    char input[16];
    char match[16];
    char selection[16];
    char selection_text[16];
    char selection_match[16];
    char counter[16];
    char border[16];
} MenuTheme;

typedef struct {
    MenuActionType type;
    guint index;
    gboolean bool_value;
    char *string_value;
    char *display_text;
    char *search_text;
    GtkWidget *row;
} MenuAction;

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *prompt;
    GtkWidget *search;
    GtkWidget *list;
    GtkWidget *counter;
    GtkWidget *input_value;
    GPtrArray *actions;
    GPtrArray *font_names;
    SeekeyConfig *config;
    TuiField fields[TUI_FIELD_COUNT];
    size_t field_count;
    MenuTheme theme;
    MenuPage page;
    TuiGroup active_group;
    guint active_field;
    gboolean first_desktop_launch;
    gboolean syncing;
    gboolean dirty;
    SeekeyPreviewSession *preview;
    guint preview_sync_id;
} MenuState;

static void menu_show_root(MenuState *state);
static void menu_show_group(MenuState *state, TuiGroup group);
static void menu_show_desktop(MenuState *state);

static gboolean menu_sync_preview(gpointer user_data)
{
    MenuState *state = user_data;
    GError *error = NULL;
    if (!seekey_preview_session_sync(state->preview, state->config, &error)) {
        g_printerr("seekey: preview update failed: %s\n", error->message);
        g_clear_error(&error);
    }
    return G_SOURCE_CONTINUE;
}

static void theme_set_color(char target[16], const char *value)
{
    if (value == NULL) return;
    while (*value == '#') value++;
    if (strlen(value) == 6 || strlen(value) == 8) {
        g_strlcpy(target, value, 16);
        if (strlen(target) == 6) g_strlcat(target, "ff", 16);
    }
}

static void menu_theme_defaults(MenuTheme *theme)
{
    *theme = (MenuTheme){
        .font_size = 12,
        .lines = 15,
        .width_chars = 30,
        .horizontal_pad = 40,
        .vertical_pad = 8,
        .inner_pad = 0,
        .border_width = 1,
        .border_radius = 10,
        .selection_radius = 0,
    };
    g_strlcpy(theme->font_family, "monospace", sizeof(theme->font_family));
    theme_set_color(theme->background, "fdf6e3ff");
    theme_set_color(theme->text, "657b83ff");
    theme_set_color(theme->prompt, "586e75ff");
    theme_set_color(theme->placeholder, "93a1a1ff");
    theme_set_color(theme->input, "657b83ff");
    theme_set_color(theme->match, "cb4b16ff");
    theme_set_color(theme->selection, "eee8d5ff");
    theme_set_color(theme->selection_text, "586e75ff");
    theme_set_color(theme->selection_match, "cb4b16ff");
    theme_set_color(theme->counter, "93a1a1ff");
    theme_set_color(theme->border, "002b36ff");
}

static guint key_file_uint(GKeyFile *key_file, const char *group,
                           const char *key, guint minimum, guint maximum,
                           guint fallback)
{
    GError *error = NULL;
    guint64 value = g_key_file_get_uint64(key_file, group, key, &error);
    if (error != NULL || value < minimum || value > maximum) value = fallback;
    g_clear_error(&error);
    return (guint)value;
}

static void theme_load_font(MenuTheme *theme, const char *font)
{
    if (font == NULL || font[0] == '\0') return;
    char **parts = g_strsplit(font, ":", -1);
    if (parts[0] != NULL && parts[0][0] != '\0' &&
        g_utf8_validate(parts[0], -1, NULL) && strlen(parts[0]) < 128) {
        g_strlcpy(theme->font_family, parts[0], sizeof(theme->font_family));
    }
    for (guint i = 1; parts[i] != NULL; i++) {
        if (g_str_has_prefix(parts[i], "size=")) {
            char *end = NULL;
            guint64 size = g_ascii_strtoull(parts[i] + 5, &end, 10);
            if (end != parts[i] + 5 && *end == '\0' && size >= 6 &&
                size <= 72) {
                theme->font_size = (guint)size;
            }
        }
    }
    g_strfreev(parts);
}

static void menu_theme_apply_key_file(MenuTheme *theme, GKeyFile *key_file,
                                      gboolean include_layout)
{
    if (include_layout) {
        char *font = g_key_file_get_string(key_file, "main", "font", NULL);
        theme_load_font(theme, font);
        g_free(font);
        theme->lines = key_file_uint(key_file, "main", "lines", 1, 100,
                                     theme->lines);
        theme->width_chars = key_file_uint(key_file, "main", "width", 10,
                                           300, theme->width_chars);
        theme->horizontal_pad = key_file_uint(
            key_file, "main", "horizontal-pad", 0, 500,
            theme->horizontal_pad);
        theme->vertical_pad = key_file_uint(
            key_file, "main", "vertical-pad", 0, 500,
            theme->vertical_pad);
        theme->inner_pad = key_file_uint(key_file, "main", "inner-pad", 0,
                                         500, theme->inner_pad);
        theme->border_width = key_file_uint(
            key_file, "border", "width", 0, 50, theme->border_width);
        theme->border_radius = key_file_uint(
            key_file, "border", "radius", 0, 200, theme->border_radius);
        theme->selection_radius = key_file_uint(
            key_file, "border", "selection-radius", 0, 200,
            theme->selection_radius);
    }

    struct {
        const char *key;
        char *target;
    } colors[] = {
        {"background", theme->background},
        {"text", theme->text},
        {"prompt", theme->prompt},
        {"placeholder", theme->placeholder},
        {"input", theme->input},
        {"match", theme->match},
        {"selection", theme->selection},
        {"selection-text", theme->selection_text},
        {"selection-match", theme->selection_match},
        {"counter", theme->counter},
        {"border", theme->border},
    };
    for (guint i = 0; i < G_N_ELEMENTS(colors); i++) {
        char *value = g_key_file_get_string(key_file, "colors",
                                            colors[i].key, NULL);
        theme_set_color(colors[i].target, value);
        g_free(value);
    }
}

static void menu_theme_load_file(MenuTheme *theme, const char *path,
                                 gboolean include_layout)
{
    char *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(path, &contents, &length, NULL)) return;
    char *with_main = g_strconcat("[main]\n", contents, NULL);
    g_free(contents);
    GKeyFile *key_file = g_key_file_new();
    if (g_key_file_load_from_data(key_file, with_main, -1,
                                  G_KEY_FILE_NONE, NULL)) {
        menu_theme_apply_key_file(theme, key_file, include_layout);
    }
    g_key_file_unref(key_file);
    g_free(with_main);
}

static void menu_theme_load(MenuTheme *theme)
{
    menu_theme_defaults(theme);
    char *dir = g_build_filename(g_get_user_config_dir(), "fuzzel", NULL);
    char *config = g_build_filename(dir, "fuzzel.ini", NULL);
    menu_theme_load_file(theme, config, TRUE);
    g_free(config);
    g_free(dir);
}

static char *css_color(const char value[16])
{
    return g_strdup_printf("#%s", value);
}

static void install_menu_css(const MenuTheme *theme)
{
    char *background = css_color(theme->background);
    char *text_color = css_color(theme->text);
    char *prompt = css_color(theme->prompt);
    char *placeholder = css_color(theme->placeholder);
    char *input = css_color(theme->input);
    char *selection = css_color(theme->selection);
    char *selection_text = css_color(theme->selection_text);
    char *counter = css_color(theme->counter);
    char *border = css_color(theme->border);
    char *font = g_strescape(theme->font_family, NULL);
    guint selection_overhang = theme->horizontal_pad / 3;
    guint row_height = theme->row_height > 0
                           ? theme->row_height
                           : MAX(theme->font_size + 10, 24u);
    char *css = g_strdup_printf(
        "window.seekey-menu { background: transparent; }"
        ".fuzzel-frame {"
        " background: %s; color: %s;"
        " border: %upx solid %s; border-radius: %upx;"
        " padding: %upx %upx;"
        " font-family: \"%s\"; font-size: %upt;"
        "}"
        ".fuzzel-prompt-row { min-height: %upx; margin-bottom: %upx; }"
        ".fuzzel-prompt { color: %s; }"
        "entry.fuzzel-input, entry.fuzzel-input:focus {"
        " min-height: %upx; padding: 0; margin: 0;"
        " color: %s; background: transparent;"
        " border: none; border-radius: 0; box-shadow: none; outline: none;"
        "}"
        "entry.fuzzel-input > text {"
        " color: %s; background: transparent;"
        "}"
        "entry.fuzzel-input > text > placeholder { color: %s; }"
        ".fuzzel-counter { color: %s; margin-left: 10px; }"
        ".fuzzel-list {"
        " background: transparent;"
        " margin-left: -%upx; margin-right: -%upx;"
        "}"
        ".fuzzel-list row {"
        " min-height: %upx; padding: 0 %upx;"
        " color: %s; background: transparent; border-radius: 0;"
        "}"
        ".fuzzel-list row label { color: %s; }"
        ".fuzzel-list row:hover { background: %s; }"
        ".fuzzel-list row:selected, .fuzzel-list row:selected:hover {"
        " background: %s; border-radius: %upx;"
        "}"
        ".fuzzel-list row:selected label {"
        " color: %s; font-weight: 700;"
        "}"
        ".fuzzel-invalid { color: #ff6b6b; }",
        background, text_color, theme->border_width, border,
        theme->border_radius, theme->vertical_pad, theme->horizontal_pad,
        font != NULL ? font : "monospace", theme->font_size,
        row_height, theme->inner_pad, prompt, row_height, input, input,
        placeholder, counter, selection_overhang, selection_overhang,
        row_height, selection_overhang, text_color, text_color,
        selection, selection, theme->selection_radius, selection_text);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    g_free(css);
    g_free(font);
    g_free(border);
    g_free(counter);
    g_free(selection_text);
    g_free(selection);
    g_free(input);
    g_free(placeholder);
    g_free(prompt);
    g_free(text_color);
    g_free(background);
}

static void menu_action_free(gpointer data)
{
    MenuAction *action = data;
    g_free(action->string_value);
    g_free(action->display_text);
    g_free(action->search_text);
    g_free(action);
}

static void menu_clear(MenuState *state)
{
    state->syncing = TRUE;
    g_ptr_array_set_size(state->actions, 0);
    GtkWidget *child = gtk_widget_get_first_child(state->list);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(state->list), child);
        child = next;
    }
    state->input_value = NULL;
    state->syncing = FALSE;
}

static void menu_set_prompt(MenuState *state, const char *text,
                            const char *placeholder)
{
    char *prompt = g_strdup_printf("%s%s > ", text,
                                   state->dirty ? " *" : "");
    gtk_label_set_text(GTK_LABEL(state->prompt), prompt);
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->search), placeholder);
    g_free(prompt);
}

static GtkWidget *menu_add_action(MenuState *state, MenuActionType type,
                                  const char *label, const char *value)
{
    MenuAction *action = g_new0(MenuAction, 1);
    action->type = type;
    char *display = value != NULL
                        ? g_strdup_printf("%-24s %s", label, value)
                        : g_strdup(label);
    action->display_text = g_strdup(display);
    action->search_text = g_utf8_strdown(display, -1);

    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *name = gtk_label_new(display);
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), name);
    gtk_list_box_append(GTK_LIST_BOX(state->list), row);
    g_free(display);

    action->row = row;
    g_object_set_data(G_OBJECT(row), "seekey-menu-action", action);
    g_ptr_array_add(state->actions, action);
    return row;
}

static void menu_update_counter(MenuState *state)
{
    guint visible = 0;
    for (guint i = 0; i < state->actions->len; i++) {
        MenuAction *action = g_ptr_array_index(state->actions, i);
        if (gtk_widget_get_visible(action->row)) visible++;
    }
    char *text = g_strdup_printf("%u/%u", visible, state->actions->len);
    gtk_label_set_text(GTK_LABEL(state->counter), text);
    g_free(text);
}

static void menu_select_first_visible(MenuState *state)
{
    MenuAction *back = NULL;
    for (guint i = 0; i < state->actions->len; i++) {
        MenuAction *action = g_ptr_array_index(state->actions, i);
        if (!gtk_widget_get_visible(action->row) ||
            !gtk_widget_get_sensitive(action->row)) {
            continue;
        }
        if (action->type == MENU_ACTION_BACK) {
            back = action;
            continue;
        }
        gtk_list_box_select_row(GTK_LIST_BOX(state->list),
                                GTK_LIST_BOX_ROW(action->row));
        return;
    }
    gtk_list_box_select_row(GTK_LIST_BOX(state->list),
                            back != NULL ? GTK_LIST_BOX_ROW(back->row) : NULL);
}

static void menu_reset_search(MenuState *state)
{
    state->syncing = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(state->search), "");
    state->syncing = FALSE;
}

static void menu_update_match_markup(MenuState *state, MenuAction *action,
                                     const char *needle)
{
    GtkWidget *label = gtk_list_box_row_get_child(
        GTK_LIST_BOX_ROW(action->row));
    if (needle[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(label), action->display_text);
        return;
    }
    const char *match = strstr(action->search_text, needle);
    if (match == NULL) return;
    gsize offset = (gsize)(match - action->search_text);
    gsize match_length = strlen(needle);
    if (strlen(action->search_text) != strlen(action->display_text) ||
        !g_utf8_validate(action->display_text, (gssize)offset, NULL) ||
        !g_utf8_validate(action->display_text + offset,
                         (gssize)match_length, NULL)) {
        gtk_label_set_text(GTK_LABEL(label), action->display_text);
        return;
    }
    char *before = g_markup_escape_text(action->display_text, (gssize)offset);
    char *matched = g_markup_escape_text(action->display_text + offset,
                                         (gssize)match_length);
    char *after = g_markup_escape_text(action->display_text + offset +
                                           match_length,
                                       -1);
    GtkListBoxRow *selected = gtk_list_box_get_selected_row(
        GTK_LIST_BOX(state->list));
    const char *color = selected == GTK_LIST_BOX_ROW(action->row)
                            ? state->theme.selection_match
                            : state->theme.match;
    char *markup = g_strdup_printf(
        "%s<span foreground=\"#%s\">%s</span>%s", before, color,
        matched, after);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
    g_free(after);
    g_free(matched);
    g_free(before);
}

static void menu_refresh_match_markup(MenuState *state)
{
    if (state->page == MENU_PAGE_INPUT) return;
    const char *query = gtk_editable_get_text(GTK_EDITABLE(state->search));
    char *needle = g_utf8_strdown(query, -1);
    for (guint i = 0; i < state->actions->len; i++) {
        MenuAction *action = g_ptr_array_index(state->actions, i);
        menu_update_match_markup(state, action, needle);
    }
    g_free(needle);
}

static void menu_filter(MenuState *state)
{
    const char *query = gtk_editable_get_text(GTK_EDITABLE(state->search));
    char *needle = g_utf8_strdown(query, -1);
    for (guint i = 0; i < state->actions->len; i++) {
        MenuAction *action = g_ptr_array_index(state->actions, i);
        gboolean visible = needle[0] == '\0' ||
                           strstr(action->search_text, needle) != NULL;
        gtk_widget_set_visible(action->row, visible);
    }
    g_free(needle);
    menu_select_first_visible(state);
    menu_refresh_match_markup(state);
    menu_update_counter(state);
}

static void on_selection_changed(GtkListBox *box, gpointer user_data)
{
    MenuState *state = user_data;
    if (!state->syncing) menu_refresh_match_markup(state);
}

static gboolean input_value_valid(MenuState *state, const char *value)
{
    TuiField *field = &state->fields[state->active_field];
    if (field->type == TUI_UINT) {
        if (value == NULL || value[0] == '\0') return FALSE;
        errno = 0;
        char *end = NULL;
        guint64 parsed = g_ascii_strtoull(value, &end, 10);
        return errno == 0 && end != value && *end == '\0' &&
               parsed >= field->min && parsed <= field->max;
    }
    if (field->type == TUI_COLOR) return tui_color_value_valid(value);
    return value != NULL;
}

static void menu_update_input_action(MenuState *state)
{
    if (state->page != MENU_PAGE_INPUT || state->input_value == NULL) return;
    const char *value = gtk_editable_get_text(GTK_EDITABLE(state->search));
    gboolean valid = input_value_valid(state, value);
    GtkWidget *label = gtk_list_box_row_get_child(
        GTK_LIST_BOX_ROW(state->input_value));
    MenuAction *action = g_object_get_data(G_OBJECT(state->input_value),
                                           "seekey-menu-action");
    g_free(action->display_text);
    action->display_text = g_strdup(valid ? _("Apply value")
                                          : _("Invalid value"));
    gtk_label_set_text(GTK_LABEL(label), action->display_text);
    gtk_widget_set_sensitive(state->input_value, valid);
    if (valid) {
        gtk_widget_remove_css_class(state->search, "fuzzel-invalid");
        gtk_list_box_select_row(GTK_LIST_BOX(state->list),
                                GTK_LIST_BOX_ROW(state->input_value));
    } else {
        gtk_widget_add_css_class(state->search, "fuzzel-invalid");
    }
    menu_update_counter(state);
}

static void on_search_changed(GtkEditable *editable, gpointer user_data)
{
    MenuState *state = user_data;
    if (state->syncing) return;
    if (state->page == MENU_PAGE_INPUT)
        menu_update_input_action(state);
    else
        menu_filter(state);
}

static gboolean menu_save(MenuState *state)
{
    if (state->config->config_path[0] == '\0') {
        char *path = g_build_filename(g_get_user_config_dir(), "seekey",
                                      "config.ini", NULL);
        g_strlcpy(state->config->config_path, path,
                  sizeof(state->config->config_path));
        g_free(path);
    }
    GError *error = NULL;
    if (!seekey_config_save(state->config, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return FALSE;
    }
    state->dirty = FALSE;
    return TRUE;
}

static gboolean menu_launch_overlay(MenuState *state)
{
    char *executable = g_file_read_link("/proc/self/exe", NULL);
    if (executable == NULL) executable = g_find_program_in_path("seekey");
    if (executable == NULL) executable = g_strdup("seekey");
    char *argv[4] = {executable, NULL, NULL, NULL};
    if (state->config->config_path[0] != '\0') {
        argv[1] = "--config";
        argv[2] = state->config->config_path;
    }
    GError *error = NULL;
    gboolean ok = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                NULL, NULL, NULL, &error);
    if (!ok) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
    }
    g_free(executable);
    return ok;
}

static void save_desktop_preference(gboolean show_menu)
{
    SeekeyWindowState saved;
    seekey_window_state_load(&saved, NULL);
    saved.desktop_preference_set = TRUE;
    saved.desktop_show_menu = show_menu;
    seekey_window_state_save(&saved, NULL);
}

static void menu_show_first_run(MenuState *state)
{
    state->page = MENU_PAGE_FIRST_RUN;
    menu_clear(state);
    menu_reset_search(state);
    menu_set_prompt(state, _("First launch"), _("Choose launcher behavior"));
    GtkWidget *row = menu_add_action(state, MENU_ACTION_DESKTOP_MODE,
                                     _("Open the Seekey menu"),
                                     _("Recommended"));
    MenuAction *action = g_object_get_data(G_OBJECT(row),
                                           "seekey-menu-action");
    action->bool_value = TRUE;
    row = menu_add_action(state, MENU_ACTION_DESKTOP_MODE,
                          _("Start the key overlay directly"), NULL);
    action = g_object_get_data(G_OBJECT(row), "seekey-menu-action");
    action->bool_value = FALSE;
    menu_select_first_visible(state);
    menu_update_counter(state);
}

static void menu_show_root(MenuState *state)
{
    state->page = MENU_PAGE_ROOT;
    menu_clear(state);
    menu_reset_search(state);
    menu_set_prompt(state, "Seekey", _("Search actions and settings"));
    menu_add_action(state, MENU_ACTION_START, _("Start key overlay"),
                    state->dirty ? _("Save first") : NULL);
    for (int group = 0; group < TUI_GROUP_COUNT; group++) {
        char *count = g_strdup_printf(
            "[%zu]", tui_count_in_group(state->fields, state->field_count,
                                         (TuiGroup)group));
        GtkWidget *row = menu_add_action(state, MENU_ACTION_GROUP,
                                         tui_group_name((TuiGroup)group),
                                         count);
        g_free(count);
        MenuAction *action = g_object_get_data(G_OBJECT(row),
                                               "seekey-menu-action");
        action->index = (guint)group;
    }
    SeekeyWindowState desktop = {0};
    seekey_window_state_load(&desktop, NULL);
    const char *desktop_value = !desktop.desktop_preference_set
                                    ? _("Not chosen")
                                    : desktop.desktop_show_menu
                                          ? _("Menu")
                                          : _("Overlay");
    menu_add_action(state, MENU_ACTION_OPEN_DESKTOP, _("Desktop launcher"),
                    desktop_value);
    menu_add_action(state, MENU_ACTION_SAVE, _("Save configuration"),
                    state->dirty ? _("Unsaved") : _("Saved"));
    menu_add_action(state, MENU_ACTION_RELOAD, _("Reload configuration"),
                    NULL);
    menu_select_first_visible(state);
    menu_update_counter(state);
}

static void menu_show_group(MenuState *state, TuiGroup group)
{
    state->page = MENU_PAGE_GROUP;
    state->active_group = group;
    menu_clear(state);
    menu_reset_search(state);
    menu_set_prompt(state, tui_group_name(group), _("Search settings"));
    for (guint i = 0; i < state->field_count; i++) {
        if (state->fields[i].group != group) continue;
        char value[256];
        tui_field_value(&state->fields[i], value, sizeof(value));
        GtkWidget *row = menu_add_action(state, MENU_ACTION_FIELD,
                                         state->fields[i].label, value);
        MenuAction *action = g_object_get_data(G_OBJECT(row),
                                               "seekey-menu-action");
        action->index = i;
    }
    menu_add_action(state, MENU_ACTION_BACK, _("← Back"), NULL);
    menu_select_first_visible(state);
    menu_update_counter(state);
}

static void menu_show_bool(MenuState *state)
{
    TuiField *field = &state->fields[state->active_field];
    state->page = MENU_PAGE_BOOL;
    menu_clear(state);
    menu_reset_search(state);
    menu_set_prompt(state, field->label, _("Choose a value"));
    char *enabled = g_strdup_printf("%s%s",
                                    *field->bool_target ? "* " : "  ",
                                    _("Enabled"));
    GtkWidget *row = menu_add_action(state, MENU_ACTION_SET_BOOL,
                                     enabled, NULL);
    g_free(enabled);
    MenuAction *action = g_object_get_data(G_OBJECT(row),
                                           "seekey-menu-action");
    action->bool_value = TRUE;
    char *disabled = g_strdup_printf("%s%s",
                                     !*field->bool_target ? "* " : "  ",
                                     _("Disabled"));
    row = menu_add_action(state, MENU_ACTION_SET_BOOL, disabled, NULL);
    g_free(disabled);
    action = g_object_get_data(G_OBJECT(row), "seekey-menu-action");
    action->bool_value = FALSE;
    menu_add_action(state, MENU_ACTION_BACK, _("← Back"), NULL);
    menu_select_first_visible(state);
    menu_update_counter(state);
}

static void menu_show_choice(MenuState *state)
{
    TuiField *field = &state->fields[state->active_field];
    state->page = MENU_PAGE_CHOICE;
    menu_clear(state);
    menu_reset_search(state);
    menu_set_prompt(state, field->label, _("Search values"));
    for (guint i = 0; i < field->choice_count; i++) {
        gboolean current = g_strcmp0(field->string_target,
                                     field->choices[i]) == 0;
        char *label = g_strdup_printf("%s%s", current ? "* " : "  ",
                                      field->choices[i]);
        GtkWidget *row = menu_add_action(state, MENU_ACTION_SET_CHOICE,
                                         label, NULL);
        g_free(label);
        MenuAction *action = g_object_get_data(G_OBJECT(row),
                                               "seekey-menu-action");
        action->string_value = g_strdup(field->choices[i]);
    }
    menu_add_action(state, MENU_ACTION_BACK, _("← Back"), NULL);
    menu_select_first_visible(state);
    menu_update_counter(state);
}

static gint compare_font_names(gconstpointer a, gconstpointer b)
{
    return g_utf8_collate(*(char *const *)a, *(char *const *)b);
}

static void load_font_names(MenuState *state)
{
    if (state->font_names != NULL) return;
    state->font_names = g_ptr_array_new_with_free_func(g_free);
    PangoContext *context = gtk_widget_get_pango_context(state->window);
    PangoFontFamily **families = NULL;
    int count = 0;
    pango_context_list_families(context, &families, &count);
    for (int i = 0; i < count; i++) {
        g_ptr_array_add(state->font_names,
                        g_strdup(pango_font_family_get_name(families[i])));
    }
    g_free(families);
    g_ptr_array_sort(state->font_names, compare_font_names);
    g_ptr_array_insert(state->font_names, 0, g_strdup("inherit"));
}

static void menu_show_fonts(MenuState *state)
{
    TuiField *field = &state->fields[state->active_field];
    state->page = MENU_PAGE_FONT;
    menu_clear(state);
    menu_reset_search(state);
    menu_set_prompt(state, field->label, _("Search installed fonts"));
    load_font_names(state);
    for (guint i = 0; i < state->font_names->len; i++) {
        const char *name = g_ptr_array_index(state->font_names, i);
        gboolean current = g_strcmp0(field->string_target, name) == 0;
        const char *display = g_strcmp0(name, "inherit") == 0
                                  ? _("System default")
                                  : name;
        char *label = g_strdup_printf("%s%s", current ? "* " : "  ",
                                      display);
        GtkWidget *row = menu_add_action(state, MENU_ACTION_SET_FONT,
                                         label, NULL);
        g_free(label);
        MenuAction *action = g_object_get_data(G_OBJECT(row),
                                               "seekey-menu-action");
        action->string_value = g_strdup(name);
    }
    menu_add_action(state, MENU_ACTION_BACK, _("← Back"), NULL);
    menu_select_first_visible(state);
    menu_update_counter(state);
}

static void menu_show_input(MenuState *state)
{
    TuiField *field = &state->fields[state->active_field];
    state->page = MENU_PAGE_INPUT;
    menu_clear(state);
    menu_set_prompt(state, field->label, field->input_hint);
    state->input_value = menu_add_action(
        state, MENU_ACTION_APPLY_INPUT, _("Apply value"), NULL);
    menu_add_action(state, MENU_ACTION_BACK, _("← Back"), NULL);
    char current[256];
    tui_field_value(field, current, sizeof(current));
    state->syncing = TRUE;
    gtk_editable_set_text(GTK_EDITABLE(state->search), current);
    gtk_editable_select_region(GTK_EDITABLE(state->search), 0, -1);
    state->syncing = FALSE;
    menu_update_input_action(state);
}

static void menu_show_desktop(MenuState *state)
{
    SeekeyWindowState saved = {0};
    seekey_window_state_load(&saved, NULL);
    state->page = MENU_PAGE_DESKTOP;
    menu_clear(state);
    menu_reset_search(state);
    menu_set_prompt(state, _("Desktop launcher"), _("Choose a value"));
    char *menu_label = g_strdup_printf(
        "%s%s",
        saved.desktop_preference_set && saved.desktop_show_menu ? "* " : "  ",
        _("Open the Seekey menu"));
    GtkWidget *row = menu_add_action(state, MENU_ACTION_DESKTOP_MODE,
                                     menu_label, NULL);
    g_free(menu_label);
    MenuAction *action = g_object_get_data(G_OBJECT(row),
                                           "seekey-menu-action");
    action->bool_value = TRUE;
    char *overlay_label = g_strdup_printf(
        "%s%s",
        saved.desktop_preference_set && !saved.desktop_show_menu ? "* " : "  ",
        _("Start the key overlay directly"));
    row = menu_add_action(state, MENU_ACTION_DESKTOP_MODE,
                          overlay_label, NULL);
    g_free(overlay_label);
    action = g_object_get_data(G_OBJECT(row), "seekey-menu-action");
    action->bool_value = FALSE;
    menu_add_action(state, MENU_ACTION_BACK, _("← Back"), NULL);
    menu_select_first_visible(state);
    menu_update_counter(state);
}

static void menu_open_field(MenuState *state, guint index)
{
    state->active_field = index;
    TuiField *field = &state->fields[index];
    if (field->type == TUI_BOOL)
        menu_show_bool(state);
    else if (field->type == TUI_CHOICE)
        menu_show_choice(state);
    else if (g_strcmp0(field->label, "font-family") == 0)
        menu_show_fonts(state);
    else
        menu_show_input(state);
}

static void menu_go_back(MenuState *state)
{
    switch (state->page) {
    case MENU_PAGE_ROOT:
    case MENU_PAGE_FIRST_RUN:
        g_application_quit(G_APPLICATION(state->app));
        break;
    case MENU_PAGE_GROUP:
    case MENU_PAGE_DESKTOP:
        menu_show_root(state);
        break;
    case MENU_PAGE_BOOL:
    case MENU_PAGE_CHOICE:
    case MENU_PAGE_INPUT:
    case MENU_PAGE_FONT:
        menu_show_group(state, state->active_group);
        break;
    }
}

static void menu_apply_input(MenuState *state)
{
    const char *value = gtk_editable_get_text(GTK_EDITABLE(state->search));
    if (!input_value_valid(state, value)) return;
    TuiField *field = &state->fields[state->active_field];
    if (field->type == TUI_UINT)
        *field->uint_target = (guint)g_ascii_strtoull(value, NULL, 10);
    else
        g_strlcpy(field->string_target, value, field->string_size);
    state->dirty = TRUE;
    gtk_widget_remove_css_class(state->search, "fuzzel-invalid");
    menu_show_group(state, state->active_group);
}

static void menu_activate_action(MenuState *state, MenuAction *action)
{
    if (action == NULL) return;
    switch (action->type) {
    case MENU_ACTION_BACK:
        menu_go_back(state);
        break;
    case MENU_ACTION_START:
        if ((!state->dirty || menu_save(state)) && menu_launch_overlay(state))
            g_application_quit(G_APPLICATION(state->app));
        break;
    case MENU_ACTION_GROUP:
        menu_show_group(state, (TuiGroup)action->index);
        break;
    case MENU_ACTION_FIELD:
        menu_open_field(state, action->index);
        break;
    case MENU_ACTION_SET_BOOL:
        *state->fields[state->active_field].bool_target = action->bool_value;
        state->dirty = TRUE;
        menu_show_group(state, state->active_group);
        break;
    case MENU_ACTION_SET_CHOICE: {
        TuiField *field = &state->fields[state->active_field];
        g_strlcpy(field->string_target, action->string_value,
                  field->string_size);
        if (g_strcmp0(field->label, "theme") == 0)
            seekey_config_apply_theme(state->config, action->string_value);
        state->dirty = TRUE;
        menu_show_group(state, state->active_group);
        break;
    }
    case MENU_ACTION_SET_FONT:
        g_strlcpy(state->fields[state->active_field].string_target,
                  action->string_value,
                  state->fields[state->active_field].string_size);
        state->dirty = TRUE;
        menu_show_group(state, state->active_group);
        break;
    case MENU_ACTION_APPLY_INPUT:
        menu_apply_input(state);
        break;
    case MENU_ACTION_OPEN_DESKTOP:
        menu_show_desktop(state);
        break;
    case MENU_ACTION_DESKTOP_MODE:
        save_desktop_preference(action->bool_value);
        if (state->page == MENU_PAGE_FIRST_RUN) {
            if (action->bool_value)
                menu_show_root(state);
            else if (menu_launch_overlay(state))
                g_application_quit(G_APPLICATION(state->app));
        } else {
            menu_show_root(state);
        }
        break;
    case MENU_ACTION_SAVE:
        if (menu_save(state)) menu_show_root(state);
        break;
    case MENU_ACTION_RELOAD: {
        GError *error = NULL;
        if (seekey_config_reload(state->config, &error)) {
            tui_build_fields(state->fields, &state->field_count,
                             state->config);
            state->dirty = FALSE;
            menu_show_root(state);
        } else {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
        }
        break;
    }
    }
}

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row,
                             gpointer user_data)
{
    MenuState *state = user_data;
    menu_activate_action(state, g_object_get_data(G_OBJECT(row),
                                                  "seekey-menu-action"));
}

static void menu_move_selection(MenuState *state, int direction)
{
    GtkListBoxRow *selected = gtk_list_box_get_selected_row(
        GTK_LIST_BOX(state->list));
    int current = selected != NULL ? gtk_list_box_row_get_index(selected) : -1;
    int total = (int)state->actions->len;
    if (total == 0) return;
    for (int step = 1; step <= total; step++) {
        int next = (current + direction * step + total) % total;
        MenuAction *action = g_ptr_array_index(state->actions, (guint)next);
        if (gtk_widget_get_visible(action->row) &&
            gtk_widget_get_sensitive(action->row)) {
            gtk_list_box_select_row(GTK_LIST_BOX(state->list),
                                    GTK_LIST_BOX_ROW(action->row));
            gtk_widget_grab_focus(state->search);
            return;
        }
    }
}

static gboolean on_key_pressed(GtkEventControllerKey *controller,
                               guint keyval, guint keycode,
                               GdkModifierType modifiers, gpointer user_data)
{
    MenuState *state = user_data;
    if (keyval == GDK_KEY_Escape) {
        menu_go_back(state);
        return TRUE;
    }
    if (keyval == GDK_KEY_Down ||
        (keyval == GDK_KEY_n && (modifiers & GDK_CONTROL_MASK))) {
        menu_move_selection(state, 1);
        return TRUE;
    }
    if (keyval == GDK_KEY_Up ||
        (keyval == GDK_KEY_p && (modifiers & GDK_CONTROL_MASK))) {
        menu_move_selection(state, -1);
        return TRUE;
    }
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (state->page == MENU_PAGE_INPUT) {
            menu_apply_input(state);
        } else {
            GtkListBoxRow *row = gtk_list_box_get_selected_row(
                GTK_LIST_BOX(state->list));
            if (row != NULL)
                menu_activate_action(
                    state, g_object_get_data(G_OBJECT(row),
                                             "seekey-menu-action"));
        }
        return TRUE;
    }
    return FALSE;
}

static guint menu_measure_width(MenuState *state)
{
    PangoContext *context = gtk_widget_get_pango_context(state->window);
    PangoFontDescription *font = pango_font_description_new();
    pango_font_description_set_family(font, state->theme.font_family);
    pango_font_description_set_size(font,
                                    (int)state->theme.font_size * PANGO_SCALE);

    PangoFontMetrics *metrics = pango_context_get_metrics(
        context, font, pango_context_get_language(context));
    state->theme.row_height = (guint)MAX(
        1, PANGO_PIXELS_CEIL(pango_font_metrics_get_ascent(metrics) +
                             pango_font_metrics_get_descent(metrics)));
    pango_font_metrics_unref(metrics);

    GString *sample = g_string_sized_new(state->theme.width_chars);
    for (guint i = 0; i < state->theme.width_chars; i++)
        g_string_append_c(sample, 'o');
    PangoLayout *layout = pango_layout_new(context);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, sample->str, -1);
    int width = 0;
    pango_layout_get_pixel_size(layout, &width, NULL);
    g_object_unref(layout);
    g_string_free(sample, TRUE);
    pango_font_description_free(font);
    return (guint)MAX(width, 1);
}

static void menu_activate(GtkApplication *app, gpointer user_data)
{
    MenuState *state = user_data;
    if (state->window != NULL) {
        gtk_window_present(GTK_WINDOW(state->window));
        return;
    }
    state->window = gtk_application_window_new(app);
    g_object_add_weak_pointer(G_OBJECT(state->window),
                              (gpointer *)&state->window);
    gtk_window_set_title(GTK_WINDOW(state->window), "Seekey");
    guint text_width = menu_measure_width(state);
    guint window_width = text_width +
                         2 * state->theme.horizontal_pad +
                         2 * state->theme.border_width;
    guint window_height = 2 * state->theme.border_width +
                          2 * state->theme.vertical_pad +
                          state->theme.row_height +
                          state->theme.inner_pad +
                          state->theme.lines * state->theme.row_height;
    gtk_window_set_default_size(GTK_WINDOW(state->window),
                                MAX(window_width, 420u), window_height);
    gtk_window_set_resizable(GTK_WINDOW(state->window), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(state->window), FALSE);
    gtk_widget_add_css_class(state->window, "seekey-menu");
    GError *layer_error = NULL;
    if (!seekey_layer_shell_try_init_menu(GTK_WINDOW(state->window),
                                          &layer_error)) {
        g_clear_error(&layer_error);
    }
    install_menu_css(&state->theme);

    GtkWidget *frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(frame, "fuzzel-frame");
    gtk_window_set_child(GTK_WINDOW(state->window), frame);

    GtkWidget *prompt_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(prompt_row, "fuzzel-prompt-row");
    state->prompt = gtk_label_new("Seekey > ");
    gtk_label_set_xalign(GTK_LABEL(state->prompt), 0.0f);
    gtk_widget_add_css_class(state->prompt, "fuzzel-prompt");
    state->search = gtk_entry_new();
    gtk_widget_set_hexpand(state->search, TRUE);
    gtk_widget_add_css_class(state->search, "fuzzel-input");
    g_signal_connect(state->search, "changed",
                     G_CALLBACK(on_search_changed), state);
    state->counter = gtk_label_new("");
    gtk_widget_add_css_class(state->counter, "fuzzel-counter");
    gtk_box_append(GTK_BOX(prompt_row), state->prompt);
    gtk_box_append(GTK_BOX(prompt_row), state->search);
    gtk_box_append(GTK_BOX(prompt_row), state->counter);
    gtk_box_append(GTK_BOX(frame), prompt_row);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(
        GTK_SCROLLED_WINDOW(scroll),
        state->theme.lines * state->theme.row_height);
    gtk_scrolled_window_set_max_content_height(
        GTK_SCROLLED_WINDOW(scroll),
        state->theme.lines * state->theme.row_height);
    state->list = gtk_list_box_new();
    gtk_widget_add_css_class(state->list, "fuzzel-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state->list),
                                    GTK_SELECTION_SINGLE);
    g_signal_connect(state->list, "row-activated",
                     G_CALLBACK(on_row_activated), state);
    g_signal_connect(state->list, "selected-rows-changed",
                     G_CALLBACK(on_selection_changed), state);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->list);
    gtk_box_append(GTK_BOX(frame), scroll);

    GtkEventController *keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), state);
    gtk_widget_add_controller(state->window, keys);

    tui_build_fields(state->fields, &state->field_count, state->config);
    if (state->first_desktop_launch)
        menu_show_first_run(state);
    else
        menu_show_root(state);
    gtk_window_present(GTK_WINDOW(state->window));
    gtk_widget_grab_focus(state->search);
}

gboolean seekey_config_gui_run(SeekeyConfig *config,
                               gboolean first_desktop_launch,
                               GError **error)
{
    MenuState state = {
        .config = config,
        .first_desktop_launch = first_desktop_launch,
    };
    GError *preview_error = NULL;
    state.preview = seekey_preview_session_start(config, &preview_error);
    if (state.preview == NULL) {
        g_printerr("seekey: preview unavailable: %s\n",
                   preview_error->message);
        g_clear_error(&preview_error);
    }
    menu_theme_load(&state.theme);
    state.actions = g_ptr_array_new_with_free_func(menu_action_free);
    state.app = gtk_application_new("dev.seekey.Config",
                                    G_APPLICATION_NON_UNIQUE);
    g_signal_connect(state.app, "activate", G_CALLBACK(menu_activate), &state);
    if (state.preview != NULL)
        state.preview_sync_id = g_timeout_add(100, menu_sync_preview, &state);
    char *argv[] = {"seekey-config", NULL};
    int status = g_application_run(G_APPLICATION(state.app), 1, argv);
    if (state.preview_sync_id != 0) g_source_remove(state.preview_sync_id);
    seekey_preview_session_free(state.preview);
    g_clear_pointer(&state.font_names, g_ptr_array_unref);
    g_ptr_array_unref(state.actions);
    g_object_unref(state.app);
    if (status != 0) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "GUI exited with status %d", status);
        return FALSE;
    }
    return TRUE;
}
