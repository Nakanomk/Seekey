#include "test_helpers.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <unistd.h>

static char *g_tmp_dir = NULL;

char *test_tmp_dir(void)
{
    if (g_tmp_dir != NULL) {
        return g_strdup(g_tmp_dir);
    }
    GError *err = NULL;
    g_tmp_dir = g_dir_make_tmp("seekey-test-XXXXXX", &err);
    if (g_tmp_dir == NULL) {
        g_error("test_tmp_dir failed: %s", err->message);
    }
    return g_strdup(g_tmp_dir);
}

char *test_write_file(const char *name, const char *contents)
{
    char *dir = test_tmp_dir();
    char *path = g_build_filename(dir, name, NULL);
    g_free(dir);
    GError *err = NULL;
    if (!g_file_set_contents(path, contents, -1, &err)) {
        g_error("test_write_file failed: %s", err->message);
    }
    return path;
}

void test_cleanup(void)
{
    if (g_tmp_dir == NULL) {
        return;
    }
    GError *err = NULL;
    GDir *dir = g_dir_open(g_tmp_dir, 0, &err);
    if (dir != NULL) {
        const char *name;
        while ((name = g_dir_read_name(dir)) != NULL) {
            char *p = g_build_filename(g_tmp_dir, name, NULL);
            g_unlink(p);
            g_free(p);
        }
        g_dir_close(dir);
    }
    if (g_tmp_dir != NULL) {
        g_rmdir(g_tmp_dir);
        g_free(g_tmp_dir);
        g_tmp_dir = NULL;
    }
}

char *test_cwd(void)
{
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        g_error("getcwd failed");
    }
    return g_strdup(buf);
}

gboolean test_chdir(const char *dir)
{
    return chdir(dir) == 0;
}
