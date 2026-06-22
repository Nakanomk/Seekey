#include "unity.h"
#include "test_helpers.h"
#include "config.h"
#include "seekey.h"

#include <glib.h>
#include <linux/input-event-codes.h>

static void test_key_name_known_codes(void)
{
    TEST_ASSERT_EQUAL_STRING("Esc", seekey_key_name(KEY_ESC));
    TEST_ASSERT_EQUAL_STRING("A", seekey_key_name(KEY_A));
    TEST_ASSERT_EQUAL_STRING("Z", seekey_key_name(KEY_Z));
    TEST_ASSERT_EQUAL_STRING("1", seekey_key_name(KEY_1));
    TEST_ASSERT_EQUAL_STRING("0", seekey_key_name(KEY_0));
    TEST_ASSERT_EQUAL_STRING("Enter", seekey_key_name(KEY_ENTER));
    TEST_ASSERT_EQUAL_STRING("Space", seekey_key_name(KEY_SPACE));
    TEST_ASSERT_EQUAL_STRING("Backspace", seekey_key_name(KEY_BACKSPACE));
}

static void test_key_name_unknown_format(void)
{
    const char *name = seekey_key_name(500);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_INT(0, strncmp(name, "Key ", 4));
}

static void test_key_text_shifted_pairs(void)
{
    TEST_ASSERT_EQUAL_STRING("a", seekey_key_text(KEY_A, FALSE));
    TEST_ASSERT_EQUAL_STRING("A", seekey_key_text(KEY_A, TRUE));
    TEST_ASSERT_EQUAL_STRING("1", seekey_key_text(KEY_1, FALSE));
    TEST_ASSERT_EQUAL_STRING("!", seekey_key_text(KEY_1, TRUE));
    TEST_ASSERT_EQUAL_STRING(";", seekey_key_text(KEY_SEMICOLON, FALSE));
    TEST_ASSERT_EQUAL_STRING(":", seekey_key_text(KEY_SEMICOLON, TRUE));
    TEST_ASSERT_EQUAL_STRING(",", seekey_key_text(KEY_COMMA, FALSE));
    TEST_ASSERT_EQUAL_STRING("<", seekey_key_text(KEY_COMMA, TRUE));
}

static void test_key_text_space_unaffected_by_shift(void)
{
    TEST_ASSERT_EQUAL_STRING(" ", seekey_key_text(KEY_SPACE, FALSE));
    TEST_ASSERT_EQUAL_STRING(" ", seekey_key_text(KEY_SPACE, TRUE));
}

static void test_key_text_unknown_returns_null(void)
{
    TEST_ASSERT_NULL(seekey_key_text(KEY_F1, FALSE));
    TEST_ASSERT_NULL(seekey_key_text(KEY_LEFTCTRL, FALSE));
    TEST_ASSERT_NULL(seekey_key_text(500, FALSE));
}

static void test_key_icon_default_unicode(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    const char *bs = seekey_key_icon(KEY_BACKSPACE, &c);
    TEST_ASSERT_NOT_NULL(bs);
    TEST_ASSERT_EQUAL_STRING("⌫", bs);

    const char *en = seekey_key_icon(KEY_ENTER, &c);
    TEST_ASSERT_NOT_NULL(en);
    TEST_ASSERT_EQUAL_STRING("↵", en);

    const char *sp = seekey_key_icon(KEY_SPACE, &c);
    TEST_ASSERT_NOT_NULL(sp);
    TEST_ASSERT_EQUAL_STRING("␣", sp);
}

static void test_key_icon_override_wins(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.icon_override_count = 1;
    g_strlcpy(c.icon_overrides[0].name, "Backspace",
              sizeof(c.icon_overrides[0].name));
    g_strlcpy(c.icon_overrides[0].icon, "X",
              sizeof(c.icon_overrides[0].icon));

    const char *bs = seekey_key_icon(KEY_BACKSPACE, &c);
    TEST_ASSERT_NOT_NULL(bs);
    TEST_ASSERT_EQUAL_STRING("X", bs);
}

static void test_is_modifier_table(void)
{
    TEST_ASSERT_TRUE(seekey_is_modifier(KEY_LEFTCTRL));
    TEST_ASSERT_TRUE(seekey_is_modifier(KEY_RIGHTCTRL));
    TEST_ASSERT_TRUE(seekey_is_modifier(KEY_LEFTSHIFT));
    TEST_ASSERT_TRUE(seekey_is_modifier(KEY_RIGHTSHIFT));
    TEST_ASSERT_TRUE(seekey_is_modifier(KEY_LEFTALT));
    TEST_ASSERT_TRUE(seekey_is_modifier(KEY_LEFTMETA));

    TEST_ASSERT_FALSE(seekey_is_modifier(KEY_A));
    TEST_ASSERT_FALSE(seekey_is_modifier(KEY_ENTER));
    TEST_ASSERT_FALSE(seekey_is_modifier(KEY_SPACE));
    TEST_ASSERT_FALSE(seekey_is_modifier(500));
}

static void test_modifier_order(void)
{
    TEST_ASSERT_TRUE(seekey_modifier_order(KEY_LEFTCTRL) <
                     seekey_modifier_order(KEY_LEFTMETA));
    TEST_ASSERT_TRUE(seekey_modifier_order(KEY_LEFTMETA) <
                     seekey_modifier_order(KEY_LEFTALT));
    TEST_ASSERT_TRUE(seekey_modifier_order(KEY_LEFTALT) <
                     seekey_modifier_order(KEY_LEFTSHIFT));
    /* Non-modifier gets a high order. */
    TEST_ASSERT_TRUE(seekey_modifier_order(KEY_LEFTCTRL) <
                     seekey_modifier_order(KEY_A));
}

static void test_mouse_button_names(void)
{
    TEST_ASSERT_EQUAL_STRING("LMB", seekey_mouse_button_name(BTN_LEFT));
    TEST_ASSERT_EQUAL_STRING("RMB", seekey_mouse_button_name(BTN_RIGHT));
    TEST_ASSERT_EQUAL_STRING("MMB", seekey_mouse_button_name(BTN_MIDDLE));
    TEST_ASSERT_NULL(seekey_mouse_button_name(BTN_0));
}

int run_keynames_tests(void)
{
    UnityBegin("test_keynames.c");
    RUN_TEST(test_key_name_known_codes);
    RUN_TEST(test_key_name_unknown_format);
    RUN_TEST(test_key_text_shifted_pairs);
    RUN_TEST(test_key_text_space_unaffected_by_shift);
    RUN_TEST(test_key_text_unknown_returns_null);
    RUN_TEST(test_key_icon_default_unicode);
    RUN_TEST(test_key_icon_override_wins);
    RUN_TEST(test_is_modifier_table);
    RUN_TEST(test_modifier_order);
    RUN_TEST(test_mouse_button_names);
    return UnityEnd();
}
