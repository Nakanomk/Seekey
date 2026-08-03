#include "unity.h"

#include "config.h"
#include "preview_session.h"

#include <gio/gio.h>

static void test_preview_config_comparison_ignores_runtime_fields(void)
{
    SeekeyConfig a;
    SeekeyConfig b;
    seekey_config_set_defaults(&a);
    b = a;

    g_strlcpy(a.config_path, "/tmp/a.ini", sizeof(a.config_path));
    g_strlcpy(b.config_path, "/tmp/b.ini", sizeof(b.config_path));
    a.config_gui = TRUE;
    b.debug_input = TRUE;
    TEST_ASSERT_TRUE(seekey_preview_config_equal(&a, &b));

    b.no_layer_shell = TRUE;
    TEST_ASSERT_FALSE(seekey_preview_config_equal(&a, &b));
    b.no_layer_shell = a.no_layer_shell;
    b.duration_ms++;
    TEST_ASSERT_FALSE(seekey_preview_config_equal(&a, &b));
    b.duration_ms = a.duration_ms;
    g_strlcpy(b.matugen_path, "/tmp/colors.json",
              sizeof(b.matugen_path));
    TEST_ASSERT_FALSE(seekey_preview_config_equal(&a, &b));
}

static void test_overlay_query_tracks_application_owner(void)
{
    GTestDBus *bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(bus);

    GError *error = NULL;
    gboolean running = TRUE;
    TEST_ASSERT_TRUE(seekey_overlay_query_running(&running, &error));
    TEST_ASSERT_NULL(error);
    TEST_ASSERT_FALSE(running);

    GApplication *overlay = g_application_new(
        "dev.seekey", G_APPLICATION_DEFAULT_FLAGS);
    TEST_ASSERT_TRUE(g_application_register(overlay, NULL, &error));
    TEST_ASSERT_NULL(error);

    running = FALSE;
    TEST_ASSERT_TRUE(seekey_overlay_query_running(&running, &error));
    TEST_ASSERT_NULL(error);
    TEST_ASSERT_TRUE(running);

    g_object_unref(overlay);
    g_test_dbus_down(bus);
    g_object_unref(bus);
}

int run_preview_session_tests(void)
{
    UnityBegin("test_preview_session.c");
    RUN_TEST(test_preview_config_comparison_ignores_runtime_fields);
    RUN_TEST(test_overlay_query_tracks_application_owner);
    return UnityEnd();
}
