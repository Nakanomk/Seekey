#include "unity.h"
#include "test_helpers.h"
#include "tui.h"
#include "config.h"
#include "seekey.h"

#include <glib.h>
#include <string.h>

/* ------------------------------------------------------------------ */

static void test_field_count_matches(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    TuiField fields[TUI_FIELD_COUNT];
    size_t count = 0;
    tui_build_fields(fields, &count, &c);
    TEST_ASSERT_EQUAL_size_t(TUI_FIELD_COUNT, count);
}

static void test_field_value_uint(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.duration_ms = 1234;
    TuiField f = {0};
    f.type = TUI_UINT;
    f.uint_target = &c.duration_ms;
    char buf[64];
    tui_field_value(&f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1234", buf);
}

static void test_field_value_bool(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.merge_repeats = TRUE;
    c.show_mouse = FALSE;
    TuiField ft = {0};
    ft.type = TUI_BOOL;
    ft.bool_target = &c.merge_repeats;
    TuiField ff = {0};
    ff.type = TUI_BOOL;
    ff.bool_target = &c.show_mouse;
    char buf[16];
    tui_field_value(&ft, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("yes", buf);
    tui_field_value(&ff, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("no", buf);
}

static void test_field_value_string_color_choice(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.foreground, "#abcdef", sizeof(c.foreground));
    TuiField f = {0};
    f.type = TUI_COLOR;
    f.string_target = c.foreground;
    char buf[64];
    tui_field_value(&f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#abcdef", buf);
}

static void test_adjust_uint_clamps_min(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.duration_ms = 200;
    TuiField f = {0};
    f.type = TUI_UINT;
    f.uint_target = &c.duration_ms;
    f.min = 100;
    f.max = 10000;
    f.step = 100;
    tui_adjust_field(&f, -1);
    tui_adjust_field(&f, -1);
    TEST_ASSERT_EQUAL_UINT(100, c.duration_ms);
}

static void test_adjust_uint_clamps_max(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.duration_ms = 9900;
    TuiField f = {0};
    f.type = TUI_UINT;
    f.uint_target = &c.duration_ms;
    f.min = 100;
    f.max = 10000;
    f.step = 100;
    tui_adjust_field(&f, 1);
    tui_adjust_field(&f, 1);
    TEST_ASSERT_EQUAL_UINT(10000, c.duration_ms);
}

static void test_adjust_uint_step(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.duration_ms = 1000;
    TuiField f = {0};
    f.type = TUI_UINT;
    f.uint_target = &c.duration_ms;
    f.min = 100;
    f.max = 10000;
    f.step = 50;
    tui_adjust_field(&f, 1);
    TEST_ASSERT_EQUAL_UINT(1050, c.duration_ms);
    tui_adjust_field(&f, -1);
    TEST_ASSERT_EQUAL_UINT(1000, c.duration_ms);
}

static void test_adjust_bool_toggles(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    gboolean orig = c.merge_repeats;
    TuiField f = {0};
    f.type = TUI_BOOL;
    f.bool_target = &c.merge_repeats;
    tui_adjust_field(&f, 1);
    TEST_ASSERT_NOT_EQUAL(orig, c.merge_repeats);
    tui_adjust_field(&f, -1);
    TEST_ASSERT_EQUAL(orig, c.merge_repeats);
}

static void test_adjust_choice_cycles(void)
{
    static const char *opts[] = {"a", "b", "c"};
    char buf[8] = "b";
    TuiField f = {0};
    f.type = TUI_CHOICE;
    f.string_target = buf;
    f.string_size = sizeof(buf);
    f.choices = opts;
    f.choice_count = 3;

    tui_adjust_field(&f, 1);
    TEST_ASSERT_EQUAL_STRING("c", buf);
    tui_adjust_field(&f, 1);
    TEST_ASSERT_EQUAL_STRING("a", buf);
    tui_adjust_field(&f, -1);
    TEST_ASSERT_EQUAL_STRING("c", buf);
}

static void test_current_choice_index_unknown(void)
{
    static const char *opts[] = {"x", "y"};
    char buf[8] = "z";
    TuiField f = {0};
    f.string_target = buf;
    f.choices = opts;
    f.choice_count = 2;
    TEST_ASSERT_EQUAL_UINT(0, tui_current_choice_index(&f));
}

static void test_adjust_color_cycles(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.foreground, "#ffffff", sizeof(c.foreground));
    int first = tui_nearest_color_index(c.foreground);
    TuiField f = {0};
    f.type = TUI_COLOR;
    f.string_target = c.foreground;
    f.string_size = sizeof(c.foreground);
    tui_adjust_field(&f, 1);
    int second = tui_nearest_color_index(c.foreground);
    TEST_ASSERT_NOT_EQUAL(first, second);
    tui_adjust_field(&f, -1);
    int back = tui_nearest_color_index(c.foreground);
    TEST_ASSERT_EQUAL(first, back);
}

static void test_nearest_color_exact_match(void)
{
    TEST_ASSERT_EQUAL_INT(0, tui_nearest_color_index("#ffffff"));
    TEST_ASSERT_EQUAL_INT(5, tui_nearest_color_index("#111318"));
}

static void test_nearest_color_closest_pick(void)
{
    /* Black is not in palette; should pick the closest dark entry. */
    int idx = tui_nearest_color_index("#000000");
    /* The closest should be either #111318 (idx 5) or #333333 (idx 4). */
    TEST_ASSERT_TRUE(idx == 4 || idx == 5);
}

static void test_nearest_color_handles_null(void)
{
    TEST_ASSERT_EQUAL_INT(0, tui_nearest_color_index(NULL));
}

static void test_reset_field_uint(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.duration_ms = 5000;
    TuiField fields[TUI_FIELD_COUNT];
    size_t count = 0;
    tui_build_fields(fields, &count, &c);
    TuiField *f = NULL;
    for (size_t i = 0; i < count; i++) {
        if (g_strcmp0(fields[i].label, "duration-ms") == 0) {
            f = &fields[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(f);
    f->default_uint = 7777;
    tui_reset_field(f);
    TEST_ASSERT_EQUAL_UINT(7777, c.duration_ms);
}

static void test_reset_field_bool(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    TuiField fields[TUI_FIELD_COUNT];
    size_t count = 0;
    tui_build_fields(fields, &count, &c);
    TuiField *f = NULL;
    for (size_t i = 0; i < count; i++) {
        if (g_strcmp0(fields[i].label, "show-mouse") == 0) {
            f = &fields[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(f);
    *f->bool_target = !f->default_bool;
    tui_reset_field(f);
    TEST_ASSERT_EQUAL(f->default_bool, *f->bool_target);
}

static void test_reset_field_string(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    TuiField fields[TUI_FIELD_COUNT];
    size_t count = 0;
    tui_build_fields(fields, &count, &c);
    TuiField *f = NULL;
    for (size_t i = 0; i < count; i++) {
        if (g_strcmp0(fields[i].label, "placeholder-text") == 0) {
            f = &fields[i];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(f);
    g_strlcpy(f->string_target, "changed", f->string_size);
    tui_reset_field(f);
    TEST_ASSERT_EQUAL_STRING(f->default_string, f->string_target);
    TEST_ASSERT_EQUAL_STRING("Seekey", c.placeholder_text);
}

/* Regression: in the original code, TUI_BOOL with Enter on the field
 * could fall through to a string write into a NULL string_target. The
 * fix is that TUI_BOOL is no longer a text-input field. Verify by
 * ensuring a fresh TuiField for a BOOL has no string_target. */
static void test_bool_field_has_no_string_target(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    TuiField fields[TUI_FIELD_COUNT];
    size_t count = 0;
    tui_build_fields(fields, &count, &c);
    for (size_t i = 0; i < count; i++) {
        if (fields[i].type == TUI_BOOL) {
            TEST_ASSERT_NULL(fields[i].string_target);
        }
    }
}

/* ------------------------------------------------------------------ */

int run_tui_tests(void)
{
    UnityBegin("test_tui.c");
    RUN_TEST(test_field_count_matches);
    RUN_TEST(test_field_value_uint);
    RUN_TEST(test_field_value_bool);
    RUN_TEST(test_field_value_string_color_choice);
    RUN_TEST(test_adjust_uint_clamps_min);
    RUN_TEST(test_adjust_uint_clamps_max);
    RUN_TEST(test_adjust_uint_step);
    RUN_TEST(test_adjust_bool_toggles);
    RUN_TEST(test_adjust_choice_cycles);
    RUN_TEST(test_current_choice_index_unknown);
    RUN_TEST(test_adjust_color_cycles);
    RUN_TEST(test_nearest_color_exact_match);
    RUN_TEST(test_nearest_color_closest_pick);
    RUN_TEST(test_nearest_color_handles_null);
    RUN_TEST(test_reset_field_uint);
    RUN_TEST(test_reset_field_bool);
    RUN_TEST(test_reset_field_string);
    RUN_TEST(test_bool_field_has_no_string_target);
    return UnityEnd();
}
