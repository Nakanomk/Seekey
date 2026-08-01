#include "unity.h"

#include "runtime_lock.h"

#include <glib/gstdio.h>
#include <unistd.h>

static char lock_name[96];

static void test_runtime_lock_is_exclusive_and_reusable(void) {
  GError *error = NULL;
  SeekeyRuntimeLock *first = seekey_runtime_lock_acquire(lock_name, &error);
  TEST_ASSERT_NOT_NULL_MESSAGE(
      first, error != NULL ? error->message : "lock acquisition failed");
  TEST_ASSERT_NULL(error);

  char *path = g_strdup(seekey_runtime_lock_path(first));
  SeekeyRuntimeLock *second = seekey_runtime_lock_acquire(lock_name, &error);
  TEST_ASSERT_NULL(second);
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_EQUAL_INT(G_IO_ERROR_BUSY, error->code);
  g_clear_error(&error);

  seekey_runtime_lock_free(first);
  second = seekey_runtime_lock_acquire(lock_name, &error);
  TEST_ASSERT_NOT_NULL(second);
  TEST_ASSERT_NULL(error);
  seekey_runtime_lock_free(second);

  g_unlink(path);
  g_free(path);
}

int run_runtime_lock_tests(void) {
  g_snprintf(lock_name, sizeof(lock_name),
             "seekey-test-%u-%" G_GINT64_FORMAT ".lock", (guint)getpid(),
             g_get_monotonic_time());
  UNITY_BEGIN();
  RUN_TEST(test_runtime_lock_is_exclusive_and_reusable);
  return UNITY_END();
}
