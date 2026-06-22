#include "unity.h"
#include "test_helpers.h"
#include "window_state.h"

#include <glib.h>
#include <stdlib.h>

static char *saved_xdg_state = NULL;

static void env_swap_save(void)
{
    saved_xdg_state = g_strdup(g_getenv("XDG_STATE_HOME"));
}

static void env_swap_restore(void)
{
    if (saved_xdg_state != NULL) {
        g_setenv("XDG_STATE_HOME", saved_xdg_state, TRUE);
        g_free(saved_xdg_state);
        saved_xdg_state = NULL;
    } else {
        g_unsetenv("XDG_STATE_HOME");
    }
}

static void test_state_path_default(void)
{
    env_swap_save();
    g_unsetenv("XDG_STATE_HOME");
    char *p = seekey_window_state_path();
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(g_str_has_suffix(p, "seekey/window.ini"));
    g_free(p);
    env_swap_restore();
}

static void test_state_path_env_override(void)
{
    env_swap_save();
    g_setenv("XDG_STATE_HOME", "/tmp/custom/state", TRUE);
    char *p = seekey_window_state_path();
    TEST_ASSERT_EQUAL_STRING("/tmp/custom/state/seekey/window.ini", p);
    g_free(p);
    env_swap_restore();
}

static void test_state_load_missing_returns_zeroed(void)
{
    /* Make sure no leftover state file from a previous run. */
    seekey_window_state_clear();
    SeekeyWindowState s;
    memset(&s, 0xFF, sizeof(s));   /* poison */
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_window_state_load(&s, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("", s.monitor);
}

static void test_state_save_load_roundtrip(void)
{
    seekey_window_state_clear();
    SeekeyWindowState in;
    memset(&in, 0, sizeof(in));
    g_strlcpy(in.monitor, "HDMI-A-1", sizeof(in.monitor));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_window_state_save(&in, &err));
    TEST_ASSERT_NULL(err);

    SeekeyWindowState out;
    memset(&out, 0xFF, sizeof(out));
    TEST_ASSERT_TRUE(seekey_window_state_load(&out, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("HDMI-A-1", out.monitor);
}

static void test_state_load_malformed_returns_zeroed(void)
{
    seekey_window_state_clear();
    char *path = seekey_window_state_path();
    /* Write garbage to the state file. */
    g_file_set_contents(path, "this is not [ini\n\n[[", -1, NULL);
    g_free(path);

    SeekeyWindowState s;
    memset(&s, 0xFF, sizeof(s));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_window_state_load(&s, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("", s.monitor);
}

static void test_state_clear_removes_file(void)
{
    SeekeyWindowState s;
    memset(&s, 0, sizeof(s));
    g_strlcpy(s.monitor, "DP-1", sizeof(s.monitor));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_window_state_save(&s, &err));
    TEST_ASSERT_NULL(err);

    char *path = seekey_window_state_path();
    TEST_ASSERT_TRUE(g_file_test(path, G_FILE_TEST_EXISTS));

    seekey_window_state_clear();
    TEST_ASSERT_FALSE(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
}

static void test_state_clear_noop_when_missing(void)
{
    seekey_window_state_clear();
    /* Calling again should be a no-op, no crash. */
    seekey_window_state_clear();
    TEST_ASSERT_TRUE(TRUE);
}

int run_window_state_tests(void)
{
    UnityBegin("test_window_state.c");
    RUN_TEST(test_state_path_default);
    RUN_TEST(test_state_path_env_override);
    RUN_TEST(test_state_load_missing_returns_zeroed);
    RUN_TEST(test_state_save_load_roundtrip);
    RUN_TEST(test_state_load_malformed_returns_zeroed);
    RUN_TEST(test_state_clear_removes_file);
    RUN_TEST(test_state_clear_noop_when_missing);
    return UnityEnd();
}
