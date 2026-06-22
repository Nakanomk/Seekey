#include "window_state.h"

#include <glib.h>
#include <glib/gstdio.h>

#define STATE_DIR  "seekey"
#define STATE_FILE "window.ini"

char *seekey_window_state_path(void)
{
    const char *base = g_getenv("XDG_STATE_HOME");
    if (base == NULL || base[0] == '\0') {
        base = g_build_filename(g_get_home_dir(), ".local", "state", NULL);
    } else {
        base = g_strdup(base);
    }
    char *path = g_build_filename(base, STATE_DIR, STATE_FILE, NULL);
    g_free((gpointer)base);
    return path;
}

static char *state_dir_path(void)
{
    const char *base = g_getenv("XDG_STATE_HOME");
    if (base == NULL || base[0] == '\0') {
        base = g_build_filename(g_get_home_dir(), ".local", "state", NULL);
    } else {
        base = g_strdup(base);
    }
    char *path = g_build_filename(base, STATE_DIR, NULL);
    g_free((gpointer)base);
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

    GKeyFile *kf = g_key_file_new();
    if (state->monitor[0] != '\0') {
        g_key_file_set_string(kf, "window", "monitor", state->monitor);
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
