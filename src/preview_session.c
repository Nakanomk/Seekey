#include "preview_session.h"

#include "config.h"
#include "runtime_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

struct SeekeyPreviewSession {
    SeekeyRuntimeLock *runtime_lock;
    char *config_path;
    char *executable;
    char matugen_path[512];
    GPid child_pid;
    SeekeyConfig snapshot;
    gboolean has_snapshot;
    gint64 next_spawn_attempt;
    pid_t parent_pid;
};

gboolean seekey_overlay_query_running(gboolean *running, GError **error)
{
    g_return_val_if_fail(running != NULL, FALSE);

    gboolean lock_running = FALSE;
    GError *lock_error = NULL;
    gboolean lock_ok = seekey_runtime_lock_query(
        "seekey-overlay.lock", &lock_running, &lock_error);

    GError *bus_error = NULL;
    GDBusConnection *bus =
        g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &bus_error);
    if (bus == NULL) {
        if (lock_ok) {
            *running = lock_running;
            g_clear_error(&bus_error);
            g_clear_error(&lock_error);
            return TRUE;
        }
        g_clear_error(&lock_error);
        g_propagate_error(error, bus_error);
        return FALSE;
    }

    GVariant *reply = g_dbus_connection_call_sync(
        bus,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameHasOwner",
        g_variant_new("(s)", "dev.seekey"),
        G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        NULL,
        &bus_error);
    g_object_unref(bus);
    if (reply == NULL) {
        if (lock_ok) {
            *running = lock_running;
            g_clear_error(&bus_error);
            g_clear_error(&lock_error);
            return TRUE;
        }
        g_clear_error(&lock_error);
        g_propagate_error(error, bus_error);
        return FALSE;
    }

    gboolean bus_running = FALSE;
    g_variant_get(reply, "(b)", &bus_running);
    g_variant_unref(reply);
    *running = bus_running || (lock_ok && lock_running);
    g_clear_error(&lock_error);
    return TRUE;
}

gboolean seekey_preview_config_equal(const SeekeyConfig *a,
                                     const SeekeyConfig *b)
{
    g_return_val_if_fail(a != NULL, FALSE);
    g_return_val_if_fail(b != NULL, FALSE);

#define SAME_SCALAR(field) (a->field == b->field)
#define SAME_STRING(field) (g_strcmp0(a->field, b->field) == 0)
    if (!SAME_SCALAR(no_layer_shell) ||
        !SAME_SCALAR(merge_repeats) ||
        !SAME_SCALAR(merge_modifiers) ||
        !SAME_SCALAR(show_mouse) ||
        !SAME_SCALAR(duration_ms) ||
        !SAME_SCALAR(typing_idle_ms) ||
        !SAME_SCALAR(fade_ms) ||
        !SAME_SCALAR(margin_px) ||
        !SAME_SCALAR(margin_horizontal_px) ||
        !SAME_SCALAR(max_items) ||
        !SAME_SCALAR(window_width) ||
        !SAME_SCALAR(window_height) ||
        !SAME_SCALAR(box_spacing) ||
        !SAME_SCALAR(overlay_padding) ||
        !SAME_SCALAR(key_min_width) ||
        !SAME_SCALAR(key_padding_x) ||
        !SAME_SCALAR(key_padding_y) ||
        !SAME_SCALAR(key_radius) ||
        !SAME_SCALAR(key_border_width) ||
        !SAME_SCALAR(key_font_px) ||
        !SAME_SCALAR(key_font_weight) ||
        !SAME_SCALAR(typing_max_width) ||
        !SAME_STRING(align) ||
        !SAME_STRING(disappear) ||
        !SAME_STRING(layer_shell) ||
        !SAME_STRING(typing_display) ||
        !SAME_STRING(theme) ||
        !SAME_STRING(key_font_family) ||
        !SAME_STRING(foreground) ||
        !SAME_STRING(background) ||
        !SAME_STRING(border_color) ||
        !SAME_STRING(shadow) ||
        !SAME_STRING(placeholder_text) ||
        !SAME_STRING(placeholder_foreground) ||
        !SAME_STRING(placeholder_background) ||
        !SAME_STRING(placeholder_border_color) ||
        !SAME_STRING(matugen_path) ||
        !SAME_SCALAR(icon_override_count)) {
        return FALSE;
    }
#undef SAME_SCALAR
#undef SAME_STRING

    for (guint i = 0; i < a->icon_override_count; i++) {
        if (g_strcmp0(a->icon_overrides[i].name,
                      b->icon_overrides[i].name) != 0 ||
            g_strcmp0(a->icon_overrides[i].icon,
                      b->icon_overrides[i].icon) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

static void preview_child_setup(gpointer user_data)
{
    SeekeyPreviewSession *session = user_data;
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (getppid() != session->parent_pid) _exit(1);
    int null_fd = open("/dev/null", O_WRONLY);
    if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
        if (null_fd > STDERR_FILENO) close(null_fd);
    }
}

static void clear_child_pid(SeekeyPreviewSession *session)
{
    if (session->child_pid == 0) return;
    g_spawn_close_pid(session->child_pid);
    session->child_pid = 0;
}

static gboolean child_is_running(SeekeyPreviewSession *session)
{
    if (session->child_pid == 0) return FALSE;

    int status = 0;
    pid_t result;
    do {
        result = waitpid(session->child_pid, &status, WNOHANG);
    } while (result < 0 && errno == EINTR);

    if (result == 0) return TRUE;
    if (result == session->child_pid ||
        (result < 0 && errno == ECHILD)) {
        clear_child_pid(session);
        return FALSE;
    }

    g_warning("cannot query Seekey preview process %d: %s",
              (int)session->child_pid, g_strerror(errno));
    return TRUE;
}

static gboolean wait_for_child(GPid pid, gint64 timeout_us)
{
    gint64 deadline = g_get_monotonic_time() + timeout_us;
    while (TRUE) {
        pid_t result = waitpid(pid, NULL, WNOHANG);
        if (result == pid || (result < 0 && errno == ECHILD)) return TRUE;
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) {
            g_warning("failed to wait for Seekey preview process %d: %s",
                      (int)pid, g_strerror(errno));
            return TRUE;
        }
        if (g_get_monotonic_time() >= deadline) return FALSE;
        g_usleep(10000);
    }
}

static gpointer reap_child_in_background(gpointer data)
{
    pid_t pid = GPOINTER_TO_INT(data);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }
    return NULL;
}

static void stop_child(SeekeyPreviewSession *session)
{
    if (session->child_pid == 0) return;

    GPid pid = session->child_pid;
    if (kill(pid, SIGTERM) < 0 && errno != ESRCH) {
        g_warning("failed to stop Seekey preview process %d: %s",
                  (int)pid, g_strerror(errno));
    }

    if (wait_for_child(pid, G_TIME_SPAN_SECOND)) {
        clear_child_pid(session);
        return;
    }

    if (kill(pid, SIGKILL) < 0 && errno != ESRCH) {
        g_warning("failed to kill Seekey preview process %d: %s",
                  (int)pid, g_strerror(errno));
    }
    if (!wait_for_child(pid, G_TIME_SPAN_SECOND)) {
        g_warning("Seekey preview process %d did not exit after SIGKILL; "
                  "reaping it in the background", (int)pid);
        GThread *reaper = g_thread_new("seekey-preview-reaper",
                                       reap_child_in_background,
                                       GINT_TO_POINTER((gint)pid));
        g_thread_unref(reaper);
    }
    clear_child_pid(session);
}

static gboolean write_preview_config(SeekeyPreviewSession *session,
                                     const SeekeyConfig *config,
                                     GError **error)
{
    SeekeyConfig preview = *config;
    g_strlcpy(preview.config_path, session->config_path,
              sizeof(preview.config_path));
    return seekey_config_save(&preview, error);
}

static gboolean spawn_child(SeekeyPreviewSession *session, GError **error)
{
    char *argv[7] = {
        session->executable,
        "--config",
        session->config_path,
        "--preview-child",
        NULL,
        NULL,
        NULL,
    };
    if (session->matugen_path[0] != '\0') {
        argv[4] = "--matugen";
        argv[5] = session->matugen_path;
    }
    return g_spawn_async(NULL, argv, NULL,
                         G_SPAWN_DO_NOT_REAP_CHILD,
                         preview_child_setup, session,
                         &session->child_pid, error);
}

gboolean seekey_preview_session_sync(SeekeyPreviewSession *session,
                                     const SeekeyConfig *config,
                                     GError **error)
{
    g_return_val_if_fail(session != NULL, FALSE);
    g_return_val_if_fail(config != NULL, FALSE);

    gboolean same_config = session->has_snapshot &&
                           seekey_preview_config_equal(&session->snapshot,
                                                       config);
    if (same_config) {
        if (child_is_running(session)) return TRUE;
        if (g_get_monotonic_time() < session->next_spawn_attempt) return TRUE;
    }
    g_strlcpy(session->matugen_path, config->matugen_path,
              sizeof(session->matugen_path));
    if (!write_preview_config(session, config, error)) return FALSE;

    stop_child(session);
    if (!spawn_child(session, error)) {
        session->snapshot = *config;
        session->has_snapshot = TRUE;
        session->next_spawn_attempt =
            g_get_monotonic_time() + G_TIME_SPAN_SECOND;
        return FALSE;
    }
    session->snapshot = *config;
    session->has_snapshot = TRUE;
    session->next_spawn_attempt =
        g_get_monotonic_time() + G_TIME_SPAN_SECOND;
    return TRUE;
}

SeekeyPreviewSession *seekey_preview_session_start(const SeekeyConfig *config,
                                                   GError **error)
{
    SeekeyPreviewSession *session = g_new0(SeekeyPreviewSession, 1);
    session->parent_pid = getpid();
    session->runtime_lock =
        seekey_runtime_lock_acquire("seekey-preview.lock", error);
    if (session->runtime_lock == NULL) {
        g_free(session);
        return NULL;
    }
    int fd = g_file_open_tmp("seekey-preview-XXXXXX.ini",
                             &session->config_path, error);
    if (fd < 0) {
        seekey_preview_session_free(session);
        return NULL;
    }
    close(fd);

    session->executable = g_file_read_link("/proc/self/exe", NULL);
    if (session->executable == NULL)
        session->executable = g_find_program_in_path("seekey");
    if (session->executable == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                            "cannot locate the seekey executable");
        seekey_preview_session_free(session);
        return NULL;
    }
    if (!seekey_preview_session_sync(session, config, error)) {
        seekey_preview_session_free(session);
        return NULL;
    }
    return session;
}

void seekey_preview_session_free(SeekeyPreviewSession *session)
{
    if (session == NULL) return;
    stop_child(session);
    if (session->config_path != NULL) g_unlink(session->config_path);
    g_free(session->config_path);
    g_free(session->executable);
    seekey_runtime_lock_free(session->runtime_lock);
    g_free(session);
}
