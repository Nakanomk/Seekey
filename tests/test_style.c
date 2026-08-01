#include "unity.h"
#include "config.h"
#include "style.h"

#include <glib.h>

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

int run_style_tests(void)
{
    UnityBegin("test_style.c");
    RUN_TEST(test_css_contains_font_and_minimum_width);
    RUN_TEST(test_css_inherits_desktop_font_by_default);
    RUN_TEST(test_css_escapes_font_quotes);
    return UnityEnd();
}
