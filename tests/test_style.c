#include "unity.h"
#include "config.h"
#include "style.h"

#include <glib.h>
#include <linux/input-event-codes.h>

static void test_css_contains_font_and_minimum_width(void)
{
    SeekeyConfig config;
    seekey_config_set_defaults(&config);
    config.key_min_width = 48;
    g_strlcpy(config.key_font_family, "JetBrains Mono",
              sizeof(config.key_font_family));

    char *css = seekey_style_build_css(&config);
    TEST_ASSERT_NOT_NULL(css);
    TEST_ASSERT_NOT_NULL(strstr(css, "min-width: 48px"));
    TEST_ASSERT_NOT_NULL(strstr(css, "font-family: \"JetBrains Mono\""));
    g_free(css);
}

static void test_css_inherits_desktop_font_by_default(void)
{
    SeekeyConfig config;
    seekey_config_set_defaults(&config);

    char *css = seekey_style_build_css(&config);
    TEST_ASSERT_NULL(strstr(css, "font-family:"));
    g_free(css);
}

static void test_css_escapes_font_quotes(void)
{
    SeekeyConfig config;
    seekey_config_set_defaults(&config);
    g_strlcpy(config.key_font_family, "Test \"Mono\"",
              sizeof(config.key_font_family));

    char *css = seekey_style_build_css(&config);
    TEST_ASSERT_NOT_NULL(strstr(css, "Test \\\"Mono\\\""));
    g_free(css);
}

static void test_mouse_clicks_and_scrolling_do_not_raise_the_row(void)
{
    if (!gtk_init_check()) TEST_IGNORE_MESSAGE("needs a GTK display (use xvfb-run)");
    SeekeyConfig config;
    seekey_config_set_defaults(&config);
    config.key_font_px = 14;
    config.key_min_width = 0;

    GtkWidget *window = gtk_window_new();
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), row);
    GtkCssProvider *provider = gtk_css_provider_new();
    char *css = seekey_style_build_css(&config);
    gtk_css_provider_load_from_string(provider, css);
    g_free(css);
    GdkDisplay *display = gtk_widget_get_display(row);
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_box_append(GTK_BOX(row), seekey_bubble_label_new("F1"));
    int before, after, icon_width, text_width;
    gtk_widget_measure(row, GTK_ORIENTATION_VERTICAL, -1, NULL, &before, NULL, NULL);
    const guint codes[] = {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, SEEKEY_SCROLL_UP,
                           SEEKEY_SCROLL_DOWN, SEEKEY_SCROLL_LEFT, SEEKEY_SCROLL_RIGHT};
    for (guint i = 0; i < G_N_ELEMENTS(codes); i++)
        gtk_box_append(GTK_BOX(row), seekey_mouse_bubble_new(&config, codes[i]));
    gtk_widget_measure(gtk_widget_get_last_child(row), GTK_ORIENTATION_HORIZONTAL,
                       -1, NULL, &icon_width, NULL, NULL);

    /* A custom legend can widen its bubble without making the row taller. */
    config.icon_override_count = 1;
    g_strlcpy(config.icon_overrides[0].name, "LMB", sizeof(config.icon_overrides[0].name));
    g_strlcpy(config.icon_overrides[0].icon, "Left click", sizeof(config.icon_overrides[0].icon));
    GtkWidget *custom = seekey_mouse_bubble_new(&config, BTN_LEFT);
    gtk_box_append(GTK_BOX(row), custom);
    gtk_widget_measure(custom, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &text_width, NULL, NULL);
    gtk_widget_measure(row, GTK_ORIENTATION_VERTICAL, -1, NULL, &after, NULL, NULL);

    gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(provider));
    g_object_unref(provider);
    gtk_window_destroy(GTK_WINDOW(window));
    TEST_ASSERT_GREATER_THAN_INT(0, before);
    TEST_ASSERT_EQUAL_INT(before, after);
    TEST_ASSERT_GREATER_THAN_INT(icon_width, text_width);
}

int run_style_tests(void)
{
    UnityBegin("test_style.c");
    RUN_TEST(test_css_contains_font_and_minimum_width);
    RUN_TEST(test_css_inherits_desktop_font_by_default);
    RUN_TEST(test_css_escapes_font_quotes);
    RUN_TEST(test_mouse_clicks_and_scrolling_do_not_raise_the_row);
    return UnityEnd();
}
