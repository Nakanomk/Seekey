#include "unity.h"

static const char *MATUGEN_SAMPLE;
#include "test_helpers.h"
#include "config.h"
#include "seekey.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

/* ------------------------------------------------------------------ */

static void test_set_defaults_all_fields(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    TEST_ASSERT_EQUAL_UINT(1200, c.duration_ms);
    TEST_ASSERT_EQUAL_UINT(650, c.typing_idle_ms);
    TEST_ASSERT_EQUAL_UINT(180, c.fade_ms);
    TEST_ASSERT_EQUAL_UINT(0, c.margin_px);
    TEST_ASSERT_EQUAL_UINT(5, c.max_items);
    TEST_ASSERT_TRUE(c.merge_repeats);
    TEST_ASSERT_TRUE(c.merge_modifiers);
    TEST_ASSERT_FALSE(c.show_mouse);
    TEST_ASSERT_EQUAL_STRING("right", c.align);
    TEST_ASSERT_EQUAL_STRING("fade", c.disappear);
    TEST_ASSERT_EQUAL_STRING("auto", c.layer_shell);
    TEST_ASSERT_EQUAL_STRING("default", c.theme);
    TEST_ASSERT_EQUAL_STRING("inherit", c.key_font_family);
    TEST_ASSERT_EQUAL_UINT(0, c.icon_override_count);
    TEST_ASSERT_EQUAL_STRING("", c.config_path);
}

static void test_resolve_path_cli_wins(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    char *argv[] = {"seekey", "--config", "/tmp/explicit.ini"};
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_resolve_path(&c, 3, argv, &err));
    TEST_ASSERT_EQUAL_STRING("/tmp/explicit.ini", c.config_path);
    TEST_ASSERT_NULL(err);
}

static void test_resolve_path_project_ini(void)
{
    /* Create a temp dir with seekey.ini inside and chdir there. */
    char *tmp = test_tmp_dir();
    char *old = test_cwd();
    test_chdir(tmp);
    char *p = g_build_filename(tmp, "seekey.ini", NULL);
    g_file_set_contents(p, "", 0, NULL);
    g_free(p);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    char *argv[] = {"seekey"};
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_resolve_path(&c, 1, argv, &err));
    TEST_ASSERT_TRUE(g_str_has_suffix(c.config_path, "seekey.ini"));

    test_chdir(old);
    g_free(old);
}

static void test_resolve_path_default_when_no_file(void)
{
    char *tmp = test_tmp_dir();
    char *old = test_cwd();
    test_chdir(tmp);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    char *argv[] = {"seekey"};
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_resolve_path(&c, 1, argv, &err));
    TEST_ASSERT_EQUAL_STRING("", c.config_path);

    test_chdir(old);
    g_free(old);
}

static void test_resolve_path_xdg_flag(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.xdg_config = TRUE;
    char *argv[] = {"seekey", "--xdg"};
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_resolve_path(&c, 2, argv, &err));
    TEST_ASSERT_TRUE(g_str_has_suffix(c.config_path, "config.ini"));
    TEST_ASSERT_TRUE(g_strstr_len(c.config_path, -1, "seekey") != NULL);
}

static void test_default_save_path_is_cwd_seekey_ini(void)
{
    char *old = test_cwd();
    char *tmp = test_tmp_dir();
    test_chdir(tmp);

    char *p = seekey_default_save_path();
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(g_str_has_suffix(p, "/seekey.ini"));
    /* Should be an absolute path inside the test temp dir. */
    TEST_ASSERT_TRUE(g_str_has_prefix(p, tmp));
    g_free(p);

    test_chdir(old);
    g_free(old);
}

/* ------------------------------------------------------------------ */

static void test_load_missing_returns_ok_with_defaults(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, "/nonexistent/path/seekey.ini",
              sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_EQUAL_UINT(1200, c.duration_ms);
    TEST_ASSERT_NULL(err);
}

static void test_load_empty_path_returns_ok(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_NULL(err);
}

static void test_parse_args_mouse_flags(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);

    int argc = 2;
    char *args[] = {"seekey", "--show-mouse"};
    char **argv = args;
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_parse_args(&c, &argc, &argv, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_TRUE(c.show_mouse);

    argc = 2;
    char *no_mouse_args[] = {"seekey", "--no-mouse"};
    argv = no_mouse_args;
    TEST_ASSERT_TRUE(seekey_parse_args(&c, &argc, &argv, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_FALSE(c.show_mouse);
}

static void test_parse_args_gui_modes(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    int argc = 4;
    char *args[] = {"seekey", "--config-gui", "--desktop-launch",
                    "--preview-child"};
    char **argv = args;
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_parse_args(&c, &argc, &argv, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_TRUE(c.config_gui);
    TEST_ASSERT_TRUE(c.desktop_launch);
    TEST_ASSERT_TRUE(c.preview_child);
}

static void test_extract_matugen_path_before_load(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    char *args[] = {"seekey", "--matugen", "/tmp/colors.json"};
    seekey_cli_extract_matugen_path(&c, 3, args);
    TEST_ASSERT_EQUAL_STRING("/tmp/colors.json", c.matugen_path);
}

static void test_load_typing_max_width_zero(void)
{
    char *path = test_write_file(
        "typing-zero.ini", "[style]\ntyping-max-width=0\n");

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_UINT(0, c.typing_max_width);

    g_free(path);
}

static void test_load_legacy_style_window_size(void)
{
    const char *data =
        "[general]\n"
        "window-width=800\n"
        "[style]\n"
        "window-width=900\n"
        "window-height=240\n";
    char *path = test_write_file("legacy-window.ini", data);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_UINT(800, c.window_width);
    TEST_ASSERT_EQUAL_UINT(240, c.window_height);

    g_free(path);
}

static void test_reload_preserves_paths_and_is_transactional(void)
{
    char *path = test_write_file(
        "reload.ini", "[general]\nduration-ms=800\n");

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    c.duration_ms = 5000;

    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_reload(&c, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_UINT(800, c.duration_ms);
    TEST_ASSERT_EQUAL_STRING(path, c.config_path);
    TEST_ASSERT_EQUAL_STRING("", c.matugen_path);

    TEST_ASSERT_TRUE(g_file_set_contents(
        path, "[general]\nduration-ms=99999\n", -1, &err));
    TEST_ASSERT_NULL(err);
    c.duration_ms = 1700;
    TEST_ASSERT_FALSE(seekey_config_reload(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_UINT(1700, c.duration_ms);
    TEST_ASSERT_EQUAL_STRING(path, c.config_path);
    g_clear_error(&err);

    g_free(path);
}

static void test_load_save_roundtrip(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    c.duration_ms = 2500;
    c.typing_idle_ms = 900;
    c.theme[0] = '\0';
    g_strlcpy(c.theme, "nord", sizeof(c.theme));
    seekey_config_apply_theme(&c, "nord");
    c.merge_repeats = FALSE;
    c.key_min_width = 42;
    g_strlcpy(c.key_font_family, "Iosevka", sizeof(c.key_font_family));
    c.icon_override_count = 1;
    g_strlcpy(c.icon_overrides[0].name, "Backspace", sizeof(c.icon_overrides[0].name));
    g_strlcpy(c.icon_overrides[0].icon, "⌫!", sizeof(c.icon_overrides[0].icon));

    char *path = test_write_file("seekey.ini", "");
    g_strlcpy(c.config_path, path, sizeof(c.config_path));

    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_save(&c, &err));
    TEST_ASSERT_NULL(err);

    SeekeyConfig c2;
    seekey_config_set_defaults(&c2);
    g_strlcpy(c2.config_path, path, sizeof(c2.config_path));
    TEST_ASSERT_TRUE(seekey_config_load(&c2, &err));
    TEST_ASSERT_NULL(err);

    TEST_ASSERT_EQUAL_UINT(2500, c2.duration_ms);
    TEST_ASSERT_EQUAL_UINT(900, c2.typing_idle_ms);
    TEST_ASSERT_EQUAL_STRING("nord", c2.theme);
    TEST_ASSERT_FALSE(c2.merge_repeats);
    TEST_ASSERT_EQUAL_UINT(42, c2.key_min_width);
    TEST_ASSERT_EQUAL_STRING("Iosevka", c2.key_font_family);
    TEST_ASSERT_EQUAL_UINT(1, c2.icon_override_count);
    TEST_ASSERT_EQUAL_STRING("Backspace", c2.icon_overrides[0].name);
    TEST_ASSERT_EQUAL_STRING("⌫!", c2.icon_overrides[0].icon);

    g_free(path);
}

static void test_load_invalid_uint_range_fails(void)
{
    const char *bad = "[general]\nduration-ms=99999\n";
    char *path = test_write_file("seekey.ini", bad);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_error_free(err);
    g_free(path);
}

static void test_load_invalid_choice_fails(void)
{
    const char *bad = "[style]\nalign=diagonal\n";
    char *path = test_write_file("seekey.ini", bad);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_error_free(err);
    g_free(path);
}

static void test_load_invalid_boolean_fails(void)
{
    char *path = test_write_file(
        "invalid-bool.ini", "[general]\nmerge-repeats=perhaps\n");
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_clear_error(&err);
    g_free(path);
}

static void test_load_unknown_theme_fails(void)
{
    char *path = test_write_file(
        "unknown-theme.ini", "[general]\ntheme=not-a-theme\n");
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_clear_error(&err);
    g_free(path);
}

static void test_load_too_long_string_fails(void)
{
    GString *data = g_string_new("[style]\nfont-family=");
    for (int i = 0; i < 160; i++) g_string_append_c(data, 'x');
    g_string_append_c(data, '\n');
    char *path = test_write_file("long-font.ini", data->str);
    g_string_free(data, TRUE);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_clear_error(&err);
    g_free(path);
}

static void test_load_accepts_null_error(void)
{
    char *path = test_write_file(
        "null-error.ini", "[general]\nduration-ms=invalid\n");
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    TEST_ASSERT_FALSE(seekey_config_load(&c, NULL));
    g_free(path);
}

static void test_load_empty_font_family_fails(void)
{
    char *path = test_write_file("empty-font.ini",
                                 "[style]\nfont-family=\n");
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_clear_error(&err);
    g_free(path);
}

static void test_load_unknown_key_ignored(void)
{
    const char *data = "[general]\nduration-ms=800\nmade-up-key=hello\n";
    char *path = test_write_file("seekey.ini", data);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_UINT(800, c.duration_ms);
    g_free(path);
}

static void test_load_icons_section(void)
{
    const char *data = "[icons]\nBackspace=\"X\"\nEnter=\"Y\"\n";
    char *path = test_write_file("seekey.ini", data);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_UINT(2, c.icon_override_count);
    TEST_ASSERT_EQUAL_STRING("X", c.icon_overrides[0].icon);
    TEST_ASSERT_EQUAL_STRING("Y", c.icon_overrides[1].icon);
    g_free(path);
}

static void test_load_rejects_too_many_icons(void)
{
    GString *data = g_string_new("[icons]\n");
    for (int i = 0; i <= SEEKEY_MAX_ICON_OVERRIDES; i++) {
        g_string_append_printf(data, "Key%d=X\n", i);
    }
    char *path = test_write_file("too-many-icons.ini", data->str);
    g_string_free(data, TRUE);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_clear_error(&err);
    g_free(path);
}

static void test_parse_args_rejects_unknown_theme(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    int argc = 3;
    char *args[] = {"seekey", "--theme", "unknown"};
    char **argv = args;
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_parse_args(&c, &argc, &argv, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_clear_error(&err);
}

static void test_save_preserves_comments_unknown_keys_and_matugen(void)
{
    char *mpath = test_write_file("save-colors.json", MATUGEN_SAMPLE);
    const char *data =
        "# keep this comment\n"
        "[general]\n"
        "duration-ms=800\n"
        "future-option=keep-me\n"
        "[style]\n"
        "foreground=@matugen:on_surface\n";
    char *path = test_write_file("preserve.ini", data);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    g_strlcpy(c.matugen_path, mpath, sizeof(c.matugen_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_EQUAL_STRING("#eeeeee", c.foreground);
    c.duration_ms = 900;
    TEST_ASSERT_TRUE(seekey_config_save(&c, &err));
    TEST_ASSERT_NULL(err);

    char *saved = NULL;
    TEST_ASSERT_TRUE(g_file_get_contents(path, &saved, NULL, &err));
    TEST_ASSERT_NOT_NULL(g_strstr_len(saved, -1, "# keep this comment"));
    TEST_ASSERT_NOT_NULL(g_strstr_len(saved, -1, "future-option=keep-me"));
    TEST_ASSERT_NOT_NULL(g_strstr_len(
        saved, -1, "foreground=@matugen:on_surface"));
    TEST_ASSERT_NOT_NULL(g_strstr_len(saved, -1, "duration-ms=900"));
    g_free(saved);
    g_free(mpath);
    g_free(path);
}

/* ------------------------------------------------------------------ */

static void test_theme_apply_known(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    TEST_ASSERT_TRUE(seekey_config_apply_theme(&c, "dracula"));
    TEST_ASSERT_EQUAL_STRING("#f8f8f2", c.foreground);
    TEST_ASSERT_EQUAL_STRING("alpha(#6272a4, 0.92)", c.background);
    TEST_ASSERT_EQUAL_STRING("alpha(#bd93f9, 0.72)", c.border_color);
}

static void test_theme_unknown_keeps_current(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    const char *orig_fg = c.foreground;
    seekey_config_apply_theme(&c, "not-a-real-theme");
    TEST_ASSERT_EQUAL_STRING(orig_fg, c.foreground);
}

static void test_theme_count_and_lookup(void)
{
    TEST_ASSERT_TRUE(seekey_config_theme_count() >= 6);
    TEST_ASSERT_NOT_NULL(seekey_config_theme_lookup("nord"));
    TEST_ASSERT_NULL(seekey_config_theme_lookup("nope"));
}

/* ------------------------------------------------------------------ */

static void test_init_creates_file(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    char *path = g_build_filename(test_tmp_dir(), "new.ini", NULL);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_init(&c, FALSE, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_TRUE(g_file_test(path, G_FILE_TEST_EXISTS));
    g_free(path);
}

static void test_init_existing_without_force_fails(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    char *path = test_write_file("existing.ini", "[general]\n");
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_init(&c, FALSE, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_error_free(err);
    g_free(path);
}

static void test_init_force_overwrites(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    char *path = test_write_file("existing.ini", "[general]\n");
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_init(&c, TRUE, &err));
    TEST_ASSERT_NULL(err);
    g_free(path);
}

/* ------------------------------------------------------------------ */

static void test_validate_basic(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_validate(&c, &err));
    TEST_ASSERT_NULL(err);
}

static void test_validate_rejects_bad_align(void)
{
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.align, "middle", sizeof(c.align));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_validate(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_error_free(err);
}

/* ------------------------------------------------------------------ */
/* Matugen integration                                                 */
/* ------------------------------------------------------------------ */

static void test_matugen_path_default_xdg_cache(void)
{
    /* Drop MATUGEN_COLORS to keep this deterministic. */
    g_unsetenv("MATUGEN_COLORS");
    g_unsetenv("XDG_CACHE_HOME");

    char *old = test_cwd();
    char *argv[] = {"seekey"};
    char *p = seekey_matugen_resolve_path(1, argv);
    /* No XDG_CACHE_HOME → falls back to ~/.cache/matugen/colors.json */
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(g_str_has_suffix(p, "matugen/colors.json"));
    g_free(p);
    test_chdir(old);
    g_free(old);
}

static void test_matugen_path_env_override(void)
{
    g_setenv("MATUGEN_COLORS", "/tmp/custom/matugen.json", TRUE);
    char *argv[] = {"seekey"};
    char *p = seekey_matugen_resolve_path(1, argv);
    TEST_ASSERT_EQUAL_STRING("/tmp/custom/matugen.json", p);
    g_free(p);
    g_unsetenv("MATUGEN_COLORS");
}

static void test_matugen_path_cli_override(void)
{
    g_setenv("MATUGEN_COLORS", "/tmp/should-be-ignored.json", TRUE);
    char *argv[] = {"seekey", "--matugen", "/tmp/from-cli.json"};
    char *p = seekey_matugen_resolve_path(3, argv);
    TEST_ASSERT_EQUAL_STRING("/tmp/from-cli.json", p);
    g_free(p);
    g_unsetenv("MATUGEN_COLORS");
}

static const char *MATUGEN_SAMPLE =
    "{\n"
    "  \"colors\": {\n"
    "    \"primary\": \"#ff00aa\",\n"
    "    \"on_surface\": \"#eeeeee\",\n"
    "    \"surface\": \"#1a1a1a\"\n"
    "  }\n"
    "}\n";

static void test_matugen_load_roundtrip(void)
{
    char *path = test_write_file("matugen.json", MATUGEN_SAMPLE);
    GError *err = NULL;
    GHashTable *t = seekey_matugen_load(path, &err);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("#1a1a1a", g_hash_table_lookup(t, "surface"));
    TEST_ASSERT_EQUAL_STRING("#eeeeee", g_hash_table_lookup(t, "on_surface"));
    TEST_ASSERT_EQUAL_STRING("#ff00aa", g_hash_table_lookup(t, "primary"));
    g_hash_table_destroy(t);
    g_free(path);
}

static void test_matugen_load_missing_file(void)
{
    GError *err = NULL;
    GHashTable *t = seekey_matugen_load("/no/such/path.json", &err);
    TEST_ASSERT_NULL(t);
    TEST_ASSERT_NOT_NULL(err);
    g_error_free(err);
}

static void test_matugen_load_malformed_json(void)
{
    char *path = test_write_file("bad.json", "not json {");
    GError *err = NULL;
    GHashTable *t = seekey_matugen_load(path, &err);
    TEST_ASSERT_NULL(t);
    TEST_ASSERT_NOT_NULL(err);
    g_error_free(err);
    g_free(path);
}

static GHashTable *make_colors(void)
{
    GHashTable *t = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);
    g_hash_table_insert(t, g_strdup("surface"), g_strdup("#1a1a1a"));
    g_hash_table_insert(t, g_strdup("on_surface"), g_strdup("#eeeeee"));
    return t;
}

static void test_matugen_resolve_exact_match(void)
{
    GHashTable *c = make_colors();
    char *r = seekey_matugen_resolve_value("@matugen:surface", c);
    TEST_ASSERT_EQUAL_STRING("#1a1a1a", r);
    g_free(r);
    r = seekey_matugen_resolve_value("@matugen:on_surface", c);
    TEST_ASSERT_EQUAL_STRING("#eeeeee", r);
    g_free(r);
    g_hash_table_destroy(c);
}

static void test_matugen_resolve_alpha(void)
{
    GHashTable *c = make_colors();
    char *r = seekey_matugen_resolve_value("@matugen:surface@0.86", c);
    TEST_ASSERT_EQUAL_STRING("alpha(#1a1a1a, 0.86)", r);
    g_free(r);
    r = seekey_matugen_resolve_value("@matugen:on_surface@0.74", c);
    TEST_ASSERT_EQUAL_STRING("alpha(#eeeeee, 0.74)", r);
    g_free(r);
    g_hash_table_destroy(c);
}

static void test_matugen_resolve_missing_key_keeps_raw(void)
{
    GHashTable *c = make_colors();
    char *r = seekey_matugen_resolve_value("@matugen:no_such_role", c);
    TEST_ASSERT_EQUAL_STRING("@matugen:no_such_role", r);
    g_free(r);
    g_hash_table_destroy(c);
}

static void test_matugen_resolve_passthrough(void)
{
    GHashTable *c = make_colors();
    char *r = seekey_matugen_resolve_value("#ff00aa", c);
    TEST_ASSERT_EQUAL_STRING("#ff00aa", r);
    g_free(r);
    r = seekey_matugen_resolve_value("alpha(#000, 0.5)", c);
    TEST_ASSERT_EQUAL_STRING("alpha(#000, 0.5)", r);
    g_free(r);
    r = seekey_matugen_resolve_value("0 7px 22px rgba(0,0,0,0.3)", c);
    TEST_ASSERT_EQUAL_STRING("0 7px 22px rgba(0,0,0,0.3)", r);
    g_free(r);
    r = seekey_matugen_resolve_value("", c);
    TEST_ASSERT_EQUAL_STRING("", r);
    g_free(r);
    r = seekey_matugen_resolve_value(NULL, c);
    TEST_ASSERT_EQUAL_STRING("", r);
    g_free(r);
    g_hash_table_destroy(c);
}

static void test_matugen_resolve_invalid_alpha(void)
{
    GHashTable *c = make_colors();
    /* Non-numeric alpha → ignore the @alpha suffix, return the raw color. */
    char *r = seekey_matugen_resolve_value("@matugen:surface@bogus", c);
    TEST_ASSERT_EQUAL_STRING("#1a1a1a", r);
    g_free(r);
    /* Out-of-range alpha → also fall back to raw color. */
    r = seekey_matugen_resolve_value("@matugen:surface@1.5", c);
    TEST_ASSERT_EQUAL_STRING("#1a1a1a", r);
    g_free(r);
    g_hash_table_destroy(c);
}

static void test_matugen_load_applies_to_config_fields(void)
{
    char *mpath = test_write_file("colors.json", MATUGEN_SAMPLE);
    const char *ini_data =
        "[style]\n"
        "foreground=@matugen:on_surface\n"
        "background=@matugen:surface@0.86\n";
    char *ipath = test_write_file("seekey.ini", ini_data);

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, ipath, sizeof(c.config_path));
    g_strlcpy(c.matugen_path, mpath, sizeof(c.matugen_path));

    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("#eeeeee", c.foreground);
    TEST_ASSERT_EQUAL_STRING("alpha(#1a1a1a, 0.86)", c.background);

    g_free(mpath);
    g_free(ipath);
}

static void test_missing_default_matugen_uses_theme_fallback(void)
{
    const char *old = g_getenv("MATUGEN_COLORS");
    char *old_copy = old != NULL ? g_strdup(old) : NULL;
    g_setenv("MATUGEN_COLORS", "/no/such/default-colors.json", TRUE);
    char *path = test_write_file(
        "missing-default-matugen.ini",
        "[style]\nforeground=@matugen:on_surface\n");

    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    GError *err = NULL;
    TEST_ASSERT_TRUE(seekey_config_load(&c, &err));
    TEST_ASSERT_NULL(err);
    TEST_ASSERT_EQUAL_STRING("#f7f7f2", c.foreground);
    TEST_ASSERT_TRUE(seekey_config_save(&c, &err));
    char *saved = NULL;
    TEST_ASSERT_TRUE(g_file_get_contents(path, &saved, NULL, &err));
    TEST_ASSERT_NOT_NULL(g_strstr_len(
        saved, -1, "foreground=@matugen:on_surface"));

    if (old_copy != NULL)
        g_setenv("MATUGEN_COLORS", old_copy, TRUE);
    else
        g_unsetenv("MATUGEN_COLORS");
    g_free(old_copy);
    g_free(saved);
    g_free(path);
}

static void test_explicit_missing_matugen_fails(void)
{
    char *path = test_write_file(
        "explicit-missing-matugen.ini",
        "[style]\nforeground=@matugen:on_surface\n");
    SeekeyConfig c;
    seekey_config_set_defaults(&c);
    g_strlcpy(c.config_path, path, sizeof(c.config_path));
    g_strlcpy(c.matugen_path, "/no/such/explicit-colors.json",
              sizeof(c.matugen_path));
    GError *err = NULL;
    TEST_ASSERT_FALSE(seekey_config_load(&c, &err));
    TEST_ASSERT_NOT_NULL(err);
    g_clear_error(&err);
    g_free(path);
}

static void test_cli_version_is_handled_before_config(void)
{
    char *argv[] = {"seekey", "--version"};
    FILE *capture = tmpfile();
    TEST_ASSERT_NOT_NULL(capture);

    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    TEST_ASSERT_GREATER_OR_EQUAL(0, saved_stdout);
    TEST_ASSERT_GREATER_OR_EQUAL(0, dup2(fileno(capture), STDOUT_FILENO));

    TEST_ASSERT_TRUE(seekey_cli_handle_info(2, argv));
    fflush(stdout);
    TEST_ASSERT_GREATER_OR_EQUAL(0, dup2(saved_stdout, STDOUT_FILENO));
    close(saved_stdout);

    rewind(capture);
    char output[64] = {0};
    TEST_ASSERT_NOT_NULL(fgets(output, sizeof(output), capture));
    TEST_ASSERT_EQUAL_STRING("seekey " SEEKEY_VERSION "\n", output);
    fclose(capture);
}

/* ------------------------------------------------------------------ */

int run_config_tests(void)
{
    UnityBegin("test_config.c");
    RUN_TEST(test_set_defaults_all_fields);
    RUN_TEST(test_resolve_path_cli_wins);
    RUN_TEST(test_resolve_path_project_ini);
    RUN_TEST(test_resolve_path_default_when_no_file);
    RUN_TEST(test_resolve_path_xdg_flag);
    RUN_TEST(test_default_save_path_is_cwd_seekey_ini);
    RUN_TEST(test_load_missing_returns_ok_with_defaults);
    RUN_TEST(test_load_empty_path_returns_ok);
    RUN_TEST(test_parse_args_mouse_flags);
    RUN_TEST(test_parse_args_gui_modes);
    RUN_TEST(test_extract_matugen_path_before_load);
    RUN_TEST(test_load_typing_max_width_zero);
    RUN_TEST(test_load_legacy_style_window_size);
    RUN_TEST(test_reload_preserves_paths_and_is_transactional);
    RUN_TEST(test_load_save_roundtrip);
    RUN_TEST(test_load_invalid_uint_range_fails);
    RUN_TEST(test_load_invalid_choice_fails);
    RUN_TEST(test_load_invalid_boolean_fails);
    RUN_TEST(test_load_unknown_theme_fails);
    RUN_TEST(test_load_too_long_string_fails);
    RUN_TEST(test_load_accepts_null_error);
    RUN_TEST(test_load_empty_font_family_fails);
    RUN_TEST(test_load_unknown_key_ignored);
    RUN_TEST(test_load_icons_section);
    RUN_TEST(test_load_rejects_too_many_icons);
    RUN_TEST(test_parse_args_rejects_unknown_theme);
    RUN_TEST(test_save_preserves_comments_unknown_keys_and_matugen);
    RUN_TEST(test_theme_apply_known);
    RUN_TEST(test_theme_unknown_keeps_current);
    RUN_TEST(test_theme_count_and_lookup);
    RUN_TEST(test_init_creates_file);
    RUN_TEST(test_init_existing_without_force_fails);
    RUN_TEST(test_init_force_overwrites);
    RUN_TEST(test_validate_basic);
    RUN_TEST(test_validate_rejects_bad_align);
    RUN_TEST(test_matugen_path_default_xdg_cache);
    RUN_TEST(test_matugen_path_env_override);
    RUN_TEST(test_matugen_path_cli_override);
    RUN_TEST(test_matugen_load_roundtrip);
    RUN_TEST(test_matugen_load_missing_file);
    RUN_TEST(test_matugen_load_malformed_json);
    RUN_TEST(test_matugen_resolve_exact_match);
    RUN_TEST(test_matugen_resolve_alpha);
    RUN_TEST(test_matugen_resolve_missing_key_keeps_raw);
    RUN_TEST(test_matugen_resolve_passthrough);
    RUN_TEST(test_matugen_resolve_invalid_alpha);
    RUN_TEST(test_matugen_load_applies_to_config_fields);
    RUN_TEST(test_missing_default_matugen_uses_theme_fallback);
    RUN_TEST(test_explicit_missing_matugen_fails);
    RUN_TEST(test_cli_version_is_handled_before_config);
    return UnityEnd();
}
