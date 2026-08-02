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
    pid_t parent_pid;
};

gboolean seekey_overlay_query_running(gboolean *running, GError **error)
{
    g_return_val_if_fail(running != NULL, FALSE);

    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, error);
    if (bus == NULL) return FALSE;

    GVariant *reply = g_dbus_connection_call_sync(
        bus,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameHasOwner",
        g_variant_new("(s)", "dev.seekey"),
        G_VARIANT_TYPE("(b)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        error);
    g_object_unref(bus);
    if (reply == NULL) return FALSE;

    g_variant_get(reply, "(b)", running);
    g_variant_unref(reply);
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

static void stop_child(SeekeyPreviewSession *session)
{
    if (session->child_pid == 0) return;

    if (kill(session->child_pid, SIGTERM) < 0 && errno != ESRCH) {
        g_warning("failed to stop Seekey preview process %d: %s",
                  (int)session->child_pid, g_strerror(errno));
    }
    while (waitpid(session->child_pid, NULL, 0) < 0 && errno == EINTR) {
    }
    g_spawn_close_pid(session->child_pid);
    session->child_pid = 0;
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

    if (session->has_snapshot &&
        memcmp(&session->snapshot, config, sizeof(*config)) == 0) {
        return TRUE;
    }
    g_strlcpy(session->matugen_path, config->matugen_path,
              sizeof(session->matugen_path));
    if (!write_preview_config(session, config, error)) return FALSE;

    stop_child(session);
    if (!spawn_child(session, error)) return FALSE;
    session->snapshot = *config;
    session->has_snapshot = TRUE;
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
