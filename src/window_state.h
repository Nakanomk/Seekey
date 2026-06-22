#ifndef SEEKEY_WINDOW_STATE_H
#define SEEKEY_WINDOW_STATE_H

#include <glib.h>
#include <gtk/gtk.h>

/* Persisted per-user window state (which monitor to show on across
 * sessions). Located under $XDG_STATE_HOME. */
typedef struct {
    char monitor[128];   /* gdk_monitor_get_connector() output, e.g. "HDMI-A-1" */
} SeekeyWindowState;

/* Return a newly-allocated path to the state file (caller frees with
 * g_free). Falls back to ~/.local/state/seekey/window.ini if
 * XDG_STATE_HOME is unset. */
char *seekey_window_state_path(void);

/* Load state. Missing or malformed files are not errors: the function
 * returns TRUE and zeroes `out`. `error` may be NULL. */
gboolean seekey_window_state_load(SeekeyWindowState *out, GError **error);

/* Persist state. Returns FALSE on I/O failure (caller may ignore). */
gboolean seekey_window_state_save(const SeekeyWindowState *state,
                                   GError **error);

/* Remove the state file. No-op if it does not exist. */
void seekey_window_state_clear(void);

/* Look up a GdkMonitor by connector name. Returns NULL if no match. */
GdkMonitor *seekey_find_monitor_by_name(GdkDisplay *display, const char *name);

#endif
