#include "window_state.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <unistd.h>

#define STATE_DIR  "seekey"
#define STATE_FILE "window.ini"

static char *state_base_path(void)
{
    const char *configured = g_getenv("XDG_STATE_HOME");
    if (configured != NULL && configured[0] != '\0') {
        return g_strdup(configured);
    }

    const char *home = g_get_home_dir();
    if (home != NULL && home[0] != '\0') {
        return g_build_filename(home, ".local", "state", NULL);
    }

    char *fallback = g_strdup_printf("seekey-state-%u", (guint)geteuid());
    char *path = g_build_filename(g_get_tmp_dir(), fallback, NULL);
    g_free(fallback);
    return path;
}

char *seekey_window_state_path(void)
{
    char *base = state_base_path();
    char *path = g_build_filename(base, STATE_DIR, STATE_FILE, NULL);
    g_free(base);
    return path;
}

static char *state_dir_path(void)
{
    char *base = state_base_path();
    char *path = g_build_filename(base, STATE_DIR, NULL);
    g_free(base);
    return path;
}

gboolean seekey_window_state_load(SeekeyWindowState *out, GError **error)
{
    g_return_val_if_fail(out != NULL, FALSE);
    memset(out, 0, sizeof(*out));

    char *path = seekey_window_state_path();
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        return TRUE;
    }
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        g_free(path);
        return TRUE;
    }

    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err)) {
        /* Malformed file: treat as missing. */
        g_key_file_unref(kf);
        g_clear_error(&err);
        g_free(path);
        return TRUE;
    }
    g_free(path);

    char *monitor = g_key_file_get_string(kf, "window", "monitor", NULL);
    if (monitor != NULL) {
        g_strlcpy(out->monitor, monitor, sizeof(out->monitor));
        g_free(monitor);
    }
    if (g_key_file_has_key(kf, "desktop", "show-menu", NULL)) {
        GError *pref_error = NULL;
        gboolean value =
            g_key_file_get_boolean(kf, "desktop", "show-menu", &pref_error);
        if (pref_error == NULL) {
            out->desktop_preference_set = TRUE;
            out->desktop_show_menu = value;
        }
        g_clear_error(&pref_error);
    }

    g_key_file_unref(kf);
    return TRUE;
}

gboolean seekey_window_state_save(const SeekeyWindowState *state, GError **error)
{
    g_return_val_if_fail(state != NULL, FALSE);

    char *dir = state_dir_path();
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        int e = errno;
        if (error != NULL) {
            g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(e),
                        "Failed to create state dir %s: %s",
                        dir, g_strerror(e));
        }
        g_free(dir);
        return FALSE;
    }
    g_free(dir);

    char *path = seekey_window_state_path();
    if (g_file_test(path, G_FILE_TEST_EXISTS) &&
        !g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        if (error != NULL) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "State path is not a regular file: %s", path);
        }
        g_free(path);
        return FALSE;
    }

    GKeyFile *kf = g_key_file_new();
    if (state->monitor[0] != '\0') {
        g_key_file_set_string(kf, "window", "monitor", state->monitor);
    }
    if (state->desktop_preference_set) {
        g_key_file_set_boolean(kf, "desktop", "show-menu",
                               state->desktop_show_menu);
    }
    gsize length = 0;
    char *data = g_key_file_to_data(kf, &length, NULL);
    g_key_file_unref(kf);
    if (data == NULL) {
        g_free(path);
        return FALSE;
    }

    gboolean ok = g_file_set_contents(path, data, (gssize)length, error);
    g_free(data);
    g_free(path);
    return ok;
}

void seekey_window_state_clear(void)
{
    char *path = seekey_window_state_path();
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_unlink(path);
    }
    g_free(path);
}

void seekey_window_state_clear_monitor(void)
{
    SeekeyWindowState state;
    seekey_window_state_load(&state, NULL);
    state.monitor[0] = '\0';
    if (state.desktop_preference_set) {
        seekey_window_state_save(&state, NULL);
    } else {
        seekey_window_state_clear();
    }
}

GdkMonitor *seekey_find_monitor_by_name(GdkDisplay *display, const char *name)
{
    if (display == NULL || name == NULL || name[0] == '\0') {
        return NULL;
    }
    GListModel *model = gdk_display_get_monitors(display);
    guint n = g_list_model_get_n_items(model);
    for (guint i = 0; i < n; i++) {
        gpointer item = g_list_model_get_item(model, i);
        if (item == NULL) continue;
        const char *connector = gdk_monitor_get_connector(GDK_MONITOR(item));
        GdkMonitor *match = NULL;
        if (connector != NULL && g_strcmp0(connector, name) == 0) {
            match = GDK_MONITOR(g_object_ref(item));
        }
        g_object_unref(item);
        if (match != NULL) {
            return match;
        }
    }
    return NULL;
}
