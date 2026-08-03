#include "runtime_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

struct SeekeyRuntimeLock {
  int fd;
  char *path;
};

static char *build_tmp_lock_path(const char *name) {
  char *filename = g_strdup_printf("seekey-%u-%s", (guint)geteuid(), name);
  char *path = g_build_filename(g_get_tmp_dir(), filename, NULL);
  g_free(filename);
  return path;
}

static char *build_lock_path(const char *name, gboolean *can_fallback) {
  const char *runtime_dir = g_get_user_runtime_dir();
  if (runtime_dir != NULL && runtime_dir[0] != '\0') {
    *can_fallback = g_strcmp0(runtime_dir, g_get_tmp_dir()) != 0;
    return g_build_filename(runtime_dir, name, NULL);
  }

  *can_fallback = FALSE;
  return build_tmp_lock_path(name);
}

SeekeyRuntimeLock *seekey_runtime_lock_acquire(const char *name,
                                               GError **error) {
  g_return_val_if_fail(name != NULL && name[0] != '\0', NULL);
  g_return_val_if_fail(strchr(name, G_DIR_SEPARATOR) == NULL, NULL);

  SeekeyRuntimeLock *lock = g_new0(SeekeyRuntimeLock, 1);
  lock->fd = -1;
  gboolean can_fallback = FALSE;
  lock->path = build_lock_path(name, &can_fallback);

  int flags = O_CREAT | O_RDWR | O_CLOEXEC;
#ifdef O_NOFOLLOW
  flags |= O_NOFOLLOW;
#endif
  lock->fd = open(lock->path, flags, 0600);
  if (lock->fd < 0 && can_fallback) {
    g_free(lock->path);
    lock->path = build_tmp_lock_path(name);
    lock->fd = open(lock->path, flags, 0600);
  }
  if (lock->fd < 0) {
    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                "cannot open runtime lock %s: %s", lock->path,
                g_strerror(errno));
    seekey_runtime_lock_free(lock);
    return NULL;
  }

  struct stat stat_buffer;
  if (fstat(lock->fd, &stat_buffer) < 0 || !S_ISREG(stat_buffer.st_mode) ||
      stat_buffer.st_uid != geteuid()) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                "runtime lock %s is not a regular file owned by this user",
                lock->path);
    seekey_runtime_lock_free(lock);
    return NULL;
  }

  if (flock(lock->fd, LOCK_EX | LOCK_NB) < 0) {
    int saved_errno = errno;
    if (saved_errno == EWOULDBLOCK || saved_errno == EAGAIN) {
      g_set_error(error, G_IO_ERROR, G_IO_ERROR_BUSY,
                  "another Seekey process already owns %s", lock->path);
    } else {
      g_set_error(error, G_IO_ERROR, g_io_error_from_errno(saved_errno),
                  "cannot lock %s: %s", lock->path, g_strerror(saved_errno));
    }
    seekey_runtime_lock_free(lock);
    return NULL;
  }

  return lock;
}

gboolean seekey_runtime_lock_query(const char *name, gboolean *locked,
                                   GError **error) {
  g_return_val_if_fail(locked != NULL, FALSE);

  *locked = FALSE;
  GError *local_error = NULL;
  SeekeyRuntimeLock *lock = seekey_runtime_lock_acquire(name, &local_error);
  if (lock != NULL) {
    seekey_runtime_lock_free(lock);
    return TRUE;
  }

  if (g_error_matches(local_error, G_IO_ERROR, G_IO_ERROR_BUSY)) {
    *locked = TRUE;
    g_clear_error(&local_error);
    return TRUE;
  }

  g_propagate_error(error, local_error);
  return FALSE;
}

const char *seekey_runtime_lock_path(const SeekeyRuntimeLock *lock) {
  return lock != NULL ? lock->path : NULL;
}

void seekey_runtime_lock_free(SeekeyRuntimeLock *lock) {
  if (lock == NULL)
    return;
  if (lock->fd >= 0)
    close(lock->fd);
  g_free(lock->path);
  g_free(lock);
}
