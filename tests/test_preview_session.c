#include "unity.h"

#include "preview_session.h"

#include <gio/gio.h>

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
    RUN_TEST(test_overlay_query_tracks_application_owner);
    return UnityEnd();
}
