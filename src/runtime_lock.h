#ifndef SEEKEY_RUNTIME_LOCK_H
#define SEEKEY_RUNTIME_LOCK_H

#include <gio/gio.h>

typedef struct SeekeyRuntimeLock SeekeyRuntimeLock;

SeekeyRuntimeLock *seekey_runtime_lock_acquire(const char *name,
                                               GError **error);
const char *seekey_runtime_lock_path(const SeekeyRuntimeLock *lock);
void seekey_runtime_lock_free(SeekeyRuntimeLock *lock);

#endif
