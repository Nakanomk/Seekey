#ifndef SEEKEY_TEST_HELPERS_H
#define SEEKEY_TEST_HELPERS_H

#include <glib.h>

/* Returns a freshly created temporary directory path. Caller frees with
 * g_free. The directory exists and is empty. */
char *test_tmp_dir(void);

/* Creates a file in the temp dir with given contents. Returns the full
 * path (caller frees with g_free). */
char *test_write_file(const char *name, const char *contents);

/* Cleans up files created with test_write_file and the temp dir. */
void test_cleanup(void);

/* Returns cwd at the time of the call. Caller frees with g_free. */
char *test_cwd(void);

/* Changes cwd; returns TRUE on success. */
gboolean test_chdir(const char *dir);

#endif
