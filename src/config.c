#include "config.h"
#include "seekey.h"

#include <errno.h>
#include <json-glib/json-glib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Theme presets                                                       */
/* ------------------------------------------------------------------ */

static const SeekeyThemePreset THEME_PRESETS[] = {
    {"default", "#f7f7f2", "alpha(#111318, 0.86)", "alpha(#ffffff, 0.18)",
     "0 7px 22px alpha(#000000, 0.30)", "alpha(#f7f7f2, 0.74)",
     "alpha(#111318, 0.56)", "alpha(#ffffff, 0.14)"},
    {"light", "#1a1a2e", "alpha(#fafafa, 0.90)", "alpha(#cccccc, 0.40)",
     "0 7px 22px alpha(#000000, 0.12)", "alpha(#1a1a2e, 0.60)",
     "alpha(#e8e8e8, 0.70)", "alpha(#bbbbbb, 0.35)"},
    {"nord", "#eceff4", "alpha(#5e81ac, 0.92)", "alpha(#88c0d0, 0.72)",
     "0 7px 22px alpha(#000000, 0.38)", "alpha(#d8dee9, 0.70)",
     "alpha(#5e81ac, 0.62)", "alpha(#88c0d0, 0.52)"},
    {"dracula", "#f8f8f2", "alpha(#6272a4, 0.92)", "alpha(#bd93f9, 0.72)",
     "0 7px 22px alpha(#000000, 0.38)", "alpha(#f8f8f2, 0.70)",
     "alpha(#6272a4, 0.62)", "alpha(#bd93f9, 0.52)"},
    {"catppuccin", "#f5e0dc", "alpha(#585b70, 0.94)", "alpha(#cba6f7, 0.72)",
     "0 7px 22px alpha(#000000, 0.38)", "alpha(#cdd6f4, 0.72)",
     "alpha(#585b70, 0.64)", "alpha(#cba6f7, 0.52)"},
    {"monokai", "#f8f8f2", "alpha(#75715e, 0.94)", "alpha(#a6e22e, 0.72)",
     "0 7px 22px alpha(#000000, 0.38)", "alpha(#f8f8f2, 0.72)",
     "alpha(#75715e, 0.64)", "alpha(#a6e22e, 0.52)"},
    {"matugen", "@matugen:on_surface", "@matugen:surface@0.86",
     "@matugen:outline@0.45", "0 7px 22px alpha(#000000, 0.30)",
     "@matugen:on_surface@0.74", "@matugen:surface@0.56",
     "@matugen:outline_variant@0.65"},
};

static const char *STYLE_THEME_KEYS[] = {
    "foreground", "background", "border-color", "shadow",
    "placeholder-foreground", "placeholder-background",
    "placeholder-border-color",
};

gsize seekey_config_theme_count(void) { return G_N_ELEMENTS(THEME_PRESETS); }

const SeekeyThemePreset *seekey_config_theme_at(gsize i) {
  if (i >= G_N_ELEMENTS(THEME_PRESETS)) {
    return NULL;
  }
  return &THEME_PRESETS[i];
}

const SeekeyThemePreset *seekey_config_theme_lookup(const char *name) {
  if (name == NULL) {
    return NULL;
  }
  for (gsize i = 0; i < G_N_ELEMENTS(THEME_PRESETS); i++) {
    if (g_strcmp0(THEME_PRESETS[i].name, name) == 0) {
      return &THEME_PRESETS[i];
    }
  }
  return NULL;
}

gboolean seekey_config_apply_theme(SeekeyConfig *config, const char *name) {
  const SeekeyThemePreset *p = seekey_config_theme_lookup(name);
  if (p == NULL) {
    return FALSE;
  }
  g_strlcpy(config->foreground, p->foreground, sizeof(config->foreground));
  g_strlcpy(config->background, p->background, sizeof(config->background));
  g_strlcpy(config->border_color, p->border_color,
            sizeof(config->border_color));
  g_strlcpy(config->shadow, p->shadow, sizeof(config->shadow));
  g_strlcpy(config->placeholder_foreground, p->placeholder_foreground,
            sizeof(config->placeholder_foreground));
  g_strlcpy(config->placeholder_background, p->placeholder_background,
            sizeof(config->placeholder_background));
  g_strlcpy(config->placeholder_border_color, p->placeholder_border_color,
            sizeof(config->placeholder_border_color));
  return TRUE;
}

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void seekey_config_set_defaults(SeekeyConfig *config) {
  memset(config, 0, sizeof(*config));
  config->merge_repeats = TRUE;
  config->merge_modifiers = TRUE;
  config->show_mouse = FALSE;
  config->duration_ms = 1200;
  config->typing_idle_ms = 650;
  config->fade_ms = 180;
  config->margin_px = 0;
  config->margin_horizontal_px = 0;
  config->max_items = 5;
  config->window_width = 720;
  config->window_height = 160;
  config->box_spacing = 7;
  config->overlay_padding = 12;
  config->key_min_width = 0;
  config->key_padding_x = 14;
  config->key_padding_y = 8;
  config->key_radius = 12;
  config->key_border_width = 1;
  config->key_font_px = 20;
  config->key_font_weight = 700;
  config->typing_max_width = 480;
  g_strlcpy(config->align, "right", sizeof(config->align));
  g_strlcpy(config->disappear, "fade", sizeof(config->disappear));
  g_strlcpy(config->layer_shell, "auto", sizeof(config->layer_shell));
  g_strlcpy(config->typing_display, "full",
            sizeof(config->typing_display));
  g_strlcpy(config->theme, "default", sizeof(config->theme));
  g_strlcpy(config->key_font_family, "inherit",
            sizeof(config->key_font_family));
  config->icon_override_count = 0;
  g_strlcpy(config->foreground, "#f7f7f2", sizeof(config->foreground));
  g_strlcpy(config->background, "alpha(#111318, 0.86)",
            sizeof(config->background));
  g_strlcpy(config->border_color, "alpha(#ffffff, 0.18)",
            sizeof(config->border_color));
  g_strlcpy(config->shadow, "0 7px 22px alpha(#000000, 0.30)",
            sizeof(config->shadow));
  g_strlcpy(config->placeholder_text, "Seekey",
            sizeof(config->placeholder_text));
  g_strlcpy(config->placeholder_foreground, "alpha(#f7f7f2, 0.74)",
            sizeof(config->placeholder_foreground));
  g_strlcpy(config->placeholder_background, "alpha(#111318, 0.56)",
            sizeof(config->placeholder_background));
  g_strlcpy(config->placeholder_border_color, "alpha(#ffffff, 0.14)",
            sizeof(config->placeholder_border_color));
  config->config_path[0] = '\0';
}

/* ------------------------------------------------------------------ */
/* Argument helpers                                                    */
/* ------------------------------------------------------------------ */

gboolean seekey_cli_has_flag(int argc, char **argv, const char *flag) {
  for (int i = 1; i < argc; i++) {
    if (g_strcmp0(argv[i], flag) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

char *seekey_default_save_path(void) {
  char *cwd = g_get_current_dir();
  char *path = g_build_filename(cwd ? cwd : ".", "seekey.ini", NULL);
  g_free(cwd);
  return path;
}

/* ------------------------------------------------------------------ */
/* Matugen integration                                                 */
/* ------------------------------------------------------------------ */

char *seekey_matugen_resolve_path(int argc, char **argv) {
  /* 1. CLI --matugen <path>. */
  char *cli_path = NULL;
  for (int i = 1; i < argc; i++) {
    if (g_strcmp0(argv[i], "--matugen") == 0 && i + 1 < argc) {
      g_free(cli_path);
      cli_path = g_strdup(argv[++i]);
    }
  }
  if (cli_path != NULL) return cli_path;

  /* 2. MATUGEN_COLORS env var. */
  const char *env = g_getenv("MATUGEN_COLORS");
  if (env != NULL && env[0] != '\0') {
    return g_strdup(env);
  }

  /* 3-4. XDG cache fallback. */
  const char *cache = g_getenv("XDG_CACHE_HOME");
  if (cache != NULL && cache[0] != '\0') {
    return g_build_filename(cache, "matugen", "colors.json", NULL);
  }
  const char *home = g_get_home_dir();
  return home != NULL
             ? g_build_filename(home, ".cache", "matugen", "colors.json", NULL)
             : NULL;
}

GHashTable *seekey_matugen_load(const char *path, GError **error) {
  if (path == NULL || path[0] == '\0') {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "matugen path is empty");
    return NULL;
  }
  if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                "matugen file not found: %s", path);
    return NULL;
  }
  if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "matugen path is not a regular file: %s", path);
    return NULL;
  }

  char *data = NULL;
  gsize size = 0;
  if (!g_file_get_contents(path, &data, &size, error)) {
    return NULL;
  }

  JsonParser *parser = json_parser_new();
  gboolean ok = json_parser_load_from_data(parser, data, (gssize)size, error);
  g_free(data);
  if (!ok) {
    g_object_unref(parser);
    return NULL;
  }

  JsonNode *root = json_parser_get_root(parser);
  if (root == NULL || !JSON_NODE_HOLDS_OBJECT(root)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "matugen JSON root is not an object");
    g_object_unref(parser);
    return NULL;
  }
  JsonObject *root_obj = json_node_get_object(root);

  JsonNode *colors_node = json_object_get_member(root_obj, "colors");
  if (colors_node == NULL || !JSON_NODE_HOLDS_OBJECT(colors_node)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "matugen JSON missing 'colors' object");
    g_object_unref(parser);
    return NULL;
  }
  JsonObject *colors_obj = json_node_get_object(colors_node);

  GHashTable *table =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  JsonObjectIter iter;
  const char *role;
  JsonNode *value_node;
  json_object_iter_init(&iter, colors_obj);
  while (json_object_iter_next(&iter, &role, &value_node)) {
    if (!JSON_NODE_HOLDS_VALUE(value_node) ||
        json_node_get_value_type(value_node) != G_TYPE_STRING)
      continue;
    const char *hex = json_node_get_string(value_node);
    if (hex == NULL)
      continue;
    g_hash_table_insert(table, g_strdup(role), g_strdup(hex));
  }

  g_object_unref(parser);
  return table;
}

char *seekey_matugen_resolve_value(const char *value, GHashTable *colors) {
  if (value == NULL) {
    return g_strdup("");
  }
  /* Pass through anything that doesn't look like a matugen reference. */
  if (strncmp(value, "@matugen:", 9) != 0) {
    return g_strdup(value);
  }

  const char *spec = value + 9; /* after "@matugen:" */
  const char *at = strchr(spec, '@');

  /* Parse role. */
  char role[64];
  gsize role_len = at ? (gsize)(at - spec) : strlen(spec);
  if (role_len == 0 || role_len >= sizeof(role)) {
    return g_strdup(value);
  }
  memcpy(role, spec, role_len);
  role[role_len] = '\0';

  /* Look up the color. */
  const char *hex = colors ? g_hash_table_lookup(colors, role) : NULL;
  if (hex == NULL) {
    /* Unknown role: keep the original so the user sees what's wrong. */
    return g_strdup(value);
  }

  if (at == NULL) {
    return g_strdup(hex);
  }

  /* Trailing @<alpha> → wrap in alpha(...). */
  char *end = NULL;
  double alpha = g_ascii_strtod(at + 1, &end);
  if (end == at + 1 || *end != '\0' || !isfinite(alpha) || alpha < 0.0 ||
      alpha > 1.0) {
    return g_strdup(hex);
  }
  return g_strdup_printf("alpha(%s, %g)", hex, alpha);
}

static gboolean copy_path_option(const char *option, const char *value,
                                 char *target, gsize target_size,
                                 GError **error) {
  if (value == NULL || value[0] == '\0' || value[0] == '-') {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "%s requires a path value (prefix paths beginning with '-' "
                "with ./)", option);
    return FALSE;
  }
  if (strlen(value) >= target_size) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "%s path is too long (maximum %zu bytes)", option,
                target_size - 1);
    return FALSE;
  }
  g_strlcpy(target, value, target_size);
  return TRUE;
}

gboolean seekey_cli_extract_matugen_path(SeekeyConfig *config, int argc,
                                         char **argv, GError **error) {
  for (int i = 1; i < argc; i++) {
    if (g_strcmp0(argv[i], "--matugen") == 0) {
      if (!copy_path_option(
              "--matugen", i + 1 < argc ? argv[i + 1] : NULL,
              config->matugen_path, sizeof(config->matugen_path), error)) {
        return FALSE;
      }
      i++;
    }
  }
  return TRUE;
}

gboolean seekey_config_resolve_path(SeekeyConfig *config, int argc, char **argv,
                                    GError **error) {
  /* 1. --config wins. */
  gboolean explicit_config = FALSE;
  for (int i = 1; i < argc; i++) {
    if (g_strcmp0(argv[i], "--config") == 0) {
      if (!copy_path_option(
              "--config", i + 1 < argc ? argv[i + 1] : NULL,
              config->config_path, sizeof(config->config_path), error)) {
        return FALSE;
      }
      explicit_config = TRUE;
      i++;
    }
  }
  if (explicit_config) return TRUE;

  /* 2. --xdg flag forces XDG path even if a project file exists. */
  if (config->xdg_config) {
    char *xdg =
        g_build_filename(g_get_user_config_dir(), "seekey", "config.ini", NULL);
    gboolean copied = copy_path_option(
        "resolved config", xdg, config->config_path,
        sizeof(config->config_path), error);
    g_free(xdg);
    return copied;
  }

  /* 3. Project directory. */
  char *cwd = g_get_current_dir();
  if (cwd == NULL) {
    config->config_path[0] = '\0';
    return TRUE;
  }
  char *project = g_build_filename(cwd, "seekey.ini", NULL);
  g_free(cwd);
  if (g_file_test(project, G_FILE_TEST_EXISTS)) {
    gboolean copied = copy_path_option(
        "resolved config", project, config->config_path,
        sizeof(config->config_path), error);
    g_free(project);
    return copied;
  }
  g_free(project);

  /* 4. No file: pure defaults. */
  config->config_path[0] = '\0';
  return TRUE;
}

/* ------------------------------------------------------------------ */
/* Validation helpers                                                  */
/* ------------------------------------------------------------------ */

static gboolean valid_choice(const char *value, const char *a, const char *b,
                             const char *c) {
  return g_strcmp0(value, a) == 0 || g_strcmp0(value, b) == 0 ||
         (c != NULL && g_strcmp0(value, c) == 0);
}

static void replace_unresolved_matugen_values(SeekeyConfig *config) {
  SeekeyConfig fallback;
  seekey_config_set_defaults(&fallback);
  if (g_strcmp0(config->theme, "matugen") != 0) {
    seekey_config_apply_theme(&fallback, config->theme);
  }
  struct {
    char *target;
    gsize size;
    const char *fallback;
  } fields[] = {
      {config->foreground, sizeof(config->foreground), fallback.foreground},
      {config->background, sizeof(config->background), fallback.background},
      {config->border_color, sizeof(config->border_color),
       fallback.border_color},
      {config->shadow, sizeof(config->shadow), fallback.shadow},
      {config->placeholder_foreground,
       sizeof(config->placeholder_foreground), fallback.placeholder_foreground},
      {config->placeholder_background,
       sizeof(config->placeholder_background), fallback.placeholder_background},
      {config->placeholder_border_color,
       sizeof(config->placeholder_border_color),
       fallback.placeholder_border_color},
  };
  for (gsize i = 0; i < G_N_ELEMENTS(fields); i++) {
    if (g_str_has_prefix(fields[i].target, "@matugen:")) {
      g_strlcpy(fields[i].target, fields[i].fallback, fields[i].size);
    }
  }
}

static gboolean parse_uint_arg(const char *name, const char *value, guint min,
                               guint max, guint *out, GError **error) {
  if (value == NULL) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "%s requires a value", name);
    return FALSE;
  }

  char *end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);
  if (*value == '\0' || *end != '\0' || parsed < min || parsed > max) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "%s must be an integer from %u to %u", name, min, max);
    return FALSE;
  }

  *out = (guint)parsed;
  return TRUE;
}

static gboolean parse_choice_arg(const char *name, const char *value,
                                 const char *a, const char *b, const char *c,
                                 char *out, gsize out_size, GError **error) {
  if (value == NULL || !valid_choice(value, a, b, c)) {
    if (c != NULL) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "%s must be one of: %s, %s, %s", name, a, b, c);
    } else {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "%s must be one of: %s, %s", name, a, b);
    }
    return FALSE;
  }

  g_strlcpy(out, value, out_size);
  return TRUE;
}

/* ------------------------------------------------------------------ */
/* Keyfile load / save helpers                                         */
/* ------------------------------------------------------------------ */

static void keyfile_get_uint_range(GKeyFile *key_file, const char *group,
                                   const char *key, guint min, guint max,
                                   guint *target, GError **error) {
  if (*error != NULL || !g_key_file_has_key(key_file, group, key, NULL)) {
    return;
  }

  guint value = (guint)g_key_file_get_integer(key_file, group, key, error);
  if (*error != NULL) {
    return;
  }
  if (value < min || value > max) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "[%s] %s must be from %u to %u", group, key, min, max);
    return;
  }

  *target = value;
}

static void keyfile_get_boolean_value(GKeyFile *key_file, const char *group,
                                      const char *key, gboolean *target,
                                      GError **error) {
  if (*error != NULL || !g_key_file_has_key(key_file, group, key, NULL)) {
    return;
  }

  gboolean value = g_key_file_get_boolean(key_file, group, key, error);
  if (*error == NULL) {
    *target = value;
  }
}

static void keyfile_get_string_value(GKeyFile *key_file, const char *group,
                                     const char *key, char *target,
                                     gsize target_size, GError **error) {
  if (*error != NULL || !g_key_file_has_key(key_file, group, key, NULL)) {
    return;
  }

  char *value = g_key_file_get_string(key_file, group, key, error);
  if (*error == NULL) {
    if (strlen(value) >= target_size) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "[%s] %s is too long (maximum %zu bytes)", group, key,
                  target_size - 1);
    } else {
      g_strlcpy(target, value, target_size);
    }
  }
  g_free(value);
}

static gboolean seekey_config_load_impl(SeekeyConfig *config, GError **error) {
  if (config->config_path[0] == '\0') {
    return seekey_config_resolve_matugen(config, error);
  }
  if (!g_file_test(config->config_path, G_FILE_TEST_EXISTS)) {
    return seekey_config_resolve_matugen(config, error);
  }
  if (!g_file_test(config->config_path, G_FILE_TEST_IS_REGULAR)) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "config path is not a regular file: %s",
                config->config_path);
    return FALSE;
  }

  GKeyFile *key_file = g_key_file_new();
  if (!g_key_file_load_from_file(key_file, config->config_path, G_KEY_FILE_NONE,
                                 error)) {
    g_key_file_unref(key_file);
    return FALSE;
  }

  /* Apply theme first so individual [style] keys can override it. */
  {
    char theme_name[32] = "";
    keyfile_get_string_value(key_file, "general", "theme", theme_name,
                             sizeof(theme_name), error);
    if (*error == NULL && theme_name[0] != '\0') {
      if (!seekey_config_apply_theme(config, theme_name)) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                    "[general] unknown theme: %s", theme_name);
      }
      g_strlcpy(config->theme, theme_name, sizeof(config->theme));
    }
  }

  keyfile_get_boolean_value(key_file, "general", "merge-repeats",
                            &config->merge_repeats, error);
  keyfile_get_boolean_value(key_file, "general", "merge-modifiers",
                            &config->merge_modifiers, error);
  keyfile_get_boolean_value(key_file, "general", "show-mouse",
                            &config->show_mouse, error);

  keyfile_get_uint_range(key_file, "general", "duration-ms", 100, 10000,
                         &config->duration_ms, error);
  keyfile_get_uint_range(key_file, "general", "typing-idle-ms", 100, 5000,
                         &config->typing_idle_ms, error);
  keyfile_get_uint_range(key_file, "general", "fade-ms", 0, 3000,
                         &config->fade_ms, error);
  keyfile_get_uint_range(key_file, "general", "margin", 0, 1000,
                         &config->margin_px, error);
  keyfile_get_uint_range(key_file, "general", "margin-horizontal", 0, 1000,
                         &config->margin_horizontal_px, error);
  keyfile_get_uint_range(key_file, "general", "max-items", 1, 20,
                         &config->max_items, error);
  /* Versions through 0.2.0 documented these two keys under [style], while
   * the loader and saver used [general]. Prefer the canonical location but
   * continue to accept those existing configuration files. */
  const char *window_width_group =
      g_key_file_has_key(key_file, "general", "window-width", NULL)
          ? "general"
          : "style";
  const char *window_height_group =
      g_key_file_has_key(key_file, "general", "window-height", NULL)
          ? "general"
          : "style";
  keyfile_get_uint_range(key_file, window_width_group, "window-width", 240,
                         3000, &config->window_width, error);
  keyfile_get_uint_range(key_file, window_height_group, "window-height", 80,
                         1000, &config->window_height, error);
  keyfile_get_string_value(key_file, "general", "layer-shell",
                           config->layer_shell, sizeof(config->layer_shell),
                           error);
  keyfile_get_string_value(key_file, "general", "typing-display",
                           config->typing_display,
                           sizeof(config->typing_display), error);

  keyfile_get_uint_range(key_file, "style", "spacing", 0, 80,
                         &config->box_spacing, error);
  keyfile_get_uint_range(key_file, "style", "overlay-padding", 0, 80,
                         &config->overlay_padding, error);
  keyfile_get_uint_range(key_file, "style", "key-min-width", 0, 300,
                         &config->key_min_width, error);
  keyfile_get_uint_range(key_file, "style", "key-padding-x", 0, 80,
                         &config->key_padding_x, error);
  keyfile_get_uint_range(key_file, "style", "key-padding-y", 0, 80,
                         &config->key_padding_y, error);
  keyfile_get_uint_range(key_file, "style", "key-radius", 0, 80,
                         &config->key_radius, error);
  keyfile_get_uint_range(key_file, "style", "key-border-width", 0, 20,
                         &config->key_border_width, error);
  keyfile_get_uint_range(key_file, "style", "key-font-px", 8, 96,
                         &config->key_font_px, error);
  keyfile_get_uint_range(key_file, "style", "key-font-weight", 100, 1000,
                         &config->key_font_weight, error);
  keyfile_get_uint_range(key_file, "style", "typing-max-width", 0, 2000,
                         &config->typing_max_width, error);
  keyfile_get_string_value(key_file, "style", "font-family",
                           config->key_font_family,
                           sizeof(config->key_font_family), error);

  keyfile_get_string_value(key_file, "style", "align", config->align,
                           sizeof(config->align), error);
  keyfile_get_string_value(key_file, "style", "disappear", config->disappear,
                           sizeof(config->disappear), error);
  keyfile_get_string_value(key_file, "style", "foreground", config->foreground,
                           sizeof(config->foreground), error);
  keyfile_get_string_value(key_file, "style", "background", config->background,
                           sizeof(config->background), error);
  keyfile_get_string_value(key_file, "style", "border-color",
                           config->border_color, sizeof(config->border_color),
                           error);
  keyfile_get_string_value(key_file, "style", "shadow", config->shadow,
                           sizeof(config->shadow), error);
  keyfile_get_string_value(key_file, "style", "placeholder-text",
                           config->placeholder_text,
                           sizeof(config->placeholder_text), error);
  keyfile_get_string_value(key_file, "style", "placeholder-foreground",
                           config->placeholder_foreground,
                           sizeof(config->placeholder_foreground), error);
  keyfile_get_string_value(key_file, "style", "placeholder-background",
                           config->placeholder_background,
                           sizeof(config->placeholder_background), error);
  keyfile_get_string_value(key_file, "style", "placeholder-border-color",
                           config->placeholder_border_color,
                           sizeof(config->placeholder_border_color), error);

  /* --- [icons] section (optional) --- */
  if (*error == NULL && g_key_file_has_group(key_file, "icons")) {
    gsize icon_count = 0;
    char **icon_keys =
        g_key_file_get_keys(key_file, "icons", &icon_count, NULL);
    if (icon_count > SEEKEY_MAX_ICON_OVERRIDES) {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                  "[icons] has %zu entries; maximum is %u", icon_count,
                  SEEKEY_MAX_ICON_OVERRIDES);
    }
    for (gsize i = 0; *error == NULL && i < icon_count; i++) {
      char *value =
          g_key_file_get_string(key_file, "icons", icon_keys[i], NULL);
      if (value != NULL) {
        gsize value_len = strlen(value);
        if (value_len >= 2 &&
            ((value[0] == '"' && value[value_len - 1] == '"') ||
             (value[0] == '\'' && value[value_len - 1] == '\''))) {
          value[value_len - 1] = '\0';
          memmove(value, value + 1, value_len - 1);
        }
        if (strlen(icon_keys[i]) >= sizeof(config->icon_overrides[0].name) ||
            strlen(value) >= sizeof(config->icon_overrides[0].icon)) {
          g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                      "[icons] entry '%s' is too long", icon_keys[i]);
        } else {
          g_strlcpy(config->icon_overrides[config->icon_override_count].name,
                    icon_keys[i], sizeof(config->icon_overrides[0].name));
          g_strlcpy(config->icon_overrides[config->icon_override_count].icon,
                    value, sizeof(config->icon_overrides[0].icon));
          config->icon_override_count++;
        }
        g_free(value);
      }
    }
    g_strfreev(icon_keys);
  }

  g_key_file_unref(key_file);

  if (*error != NULL) {
    return FALSE;
  }

  if (!seekey_config_resolve_matugen(config, error)) return FALSE;
  if (!valid_choice(config->align, "left", "center", "right")) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "[style] align must be one of: left, center, right");
    return FALSE;
  }
  if (!valid_choice(config->disappear, "instant", "fade", NULL)) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "[style] disappear must be one of: instant, fade");
    return FALSE;
  }
  if (!valid_choice(config->layer_shell, "auto", "required", "off")) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "[general] layer-shell must be one of: auto, required, off");
    return FALSE;
  }
  if (!valid_choice(config->typing_display, "full", "masked", "off")) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "[general] typing-display must be one of: full, masked, off");
    return FALSE;
  }
  if (config->key_font_family[0] == '\0') {
    g_set_error_literal(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                        "[style] font-family cannot be empty");
    return FALSE;
  }

  return TRUE;
}

gboolean seekey_config_load(SeekeyConfig *config, GError **error) {
  GError *local_error = NULL;
  gboolean ok = seekey_config_load_impl(
      config, error != NULL ? error : &local_error);
  g_clear_error(&local_error);
  return ok;
}

gboolean seekey_config_reload(SeekeyConfig *config, GError **error) {
  SeekeyConfig reloaded;
  seekey_config_set_defaults(&reloaded);
  g_strlcpy(reloaded.config_path, config->config_path,
            sizeof(reloaded.config_path));
  g_strlcpy(reloaded.matugen_path, config->matugen_path,
            sizeof(reloaded.matugen_path));

  if (!seekey_config_load(&reloaded, error)) {
    return FALSE;
  }

  *config = reloaded;
  return TRUE;
}

static void keyfile_set_config(GKeyFile *key_file, const SeekeyConfig *config) {
  g_key_file_set_integer(key_file, "general", "duration-ms",
                         (int)config->duration_ms);
  g_key_file_set_integer(key_file, "general", "typing-idle-ms",
                         (int)config->typing_idle_ms);
  g_key_file_set_integer(key_file, "general", "fade-ms", (int)config->fade_ms);
  g_key_file_set_integer(key_file, "general", "margin", (int)config->margin_px);
  g_key_file_set_integer(key_file, "general", "margin-horizontal",
                         (int)config->margin_horizontal_px);
  g_key_file_set_integer(key_file, "general", "max-items",
                         (int)config->max_items);
  g_key_file_set_integer(key_file, "general", "window-width",
                         (int)config->window_width);
  g_key_file_set_integer(key_file, "general", "window-height",
                         (int)config->window_height);
  g_key_file_set_string(key_file, "general", "layer-shell",
                        config->layer_shell);
  g_key_file_set_string(key_file, "general", "typing-display",
                        config->typing_display);
  g_key_file_set_boolean(key_file, "general", "merge-repeats",
                         config->merge_repeats);
  g_key_file_set_boolean(key_file, "general", "merge-modifiers",
                         config->merge_modifiers);
  g_key_file_set_boolean(key_file, "general", "show-mouse", config->show_mouse);
  g_key_file_set_string(key_file, "general", "theme", config->theme);

  /* These keys were documented under [style] through 0.2.0. Saving migrates
   * them to their canonical [general] location without retaining duplicates. */
  g_key_file_remove_key(key_file, "style", "window-width", NULL);
  g_key_file_remove_key(key_file, "style", "window-height", NULL);

  g_key_file_set_string(key_file, "style", "align", config->align);
  g_key_file_set_string(key_file, "style", "disappear", config->disappear);
  g_key_file_set_integer(key_file, "style", "spacing",
                         (int)config->box_spacing);
  g_key_file_set_integer(key_file, "style", "overlay-padding",
                         (int)config->overlay_padding);
  g_key_file_set_integer(key_file, "style", "key-min-width",
                         (int)config->key_min_width);
  g_key_file_set_integer(key_file, "style", "key-padding-x",
                         (int)config->key_padding_x);
  g_key_file_set_integer(key_file, "style", "key-padding-y",
                         (int)config->key_padding_y);
  g_key_file_set_integer(key_file, "style", "key-radius",
                         (int)config->key_radius);
  g_key_file_set_integer(key_file, "style", "key-border-width",
                         (int)config->key_border_width);
  g_key_file_set_integer(key_file, "style", "key-font-px",
                         (int)config->key_font_px);
  g_key_file_set_integer(key_file, "style", "key-font-weight",
                         (int)config->key_font_weight);
  g_key_file_set_integer(key_file, "style", "typing-max-width",
                         (int)config->typing_max_width);
  g_key_file_set_string(key_file, "style", "font-family",
                        config->key_font_family);
  g_key_file_set_string(key_file, "style", "foreground", config->foreground);
  g_key_file_set_string(key_file, "style", "background", config->background);
  g_key_file_set_string(key_file, "style", "border-color",
                        config->border_color);
  g_key_file_set_string(key_file, "style", "shadow", config->shadow);
  g_key_file_set_string(key_file, "style", "placeholder-text",
                        config->placeholder_text);
  g_key_file_set_string(key_file, "style", "placeholder-foreground",
                        config->placeholder_foreground);
  g_key_file_set_string(key_file, "style", "placeholder-background",
                        config->placeholder_background);
  g_key_file_set_string(key_file, "style", "placeholder-border-color",
                        config->placeholder_border_color);

  /* --- [icons] section --- */
  g_key_file_remove_group(key_file, "icons", NULL);
  for (guint i = 0; i < config->icon_override_count; i++) {
    g_key_file_set_string(key_file, "icons", config->icon_overrides[i].name,
                          config->icon_overrides[i].icon);
  }
}

static void keyfile_restore_matugen_references(GKeyFile *key_file,
                                               const SeekeyConfig *config,
                                               char **raw_values,
                                               gboolean theme_unchanged) {
  const char *current[] = {
      config->foreground, config->background, config->border_color,
      config->shadow, config->placeholder_foreground,
      config->placeholder_background, config->placeholder_border_color,
  };
  SeekeyConfig fallback;
  seekey_config_set_defaults(&fallback);
  if (g_strcmp0(config->theme, "matugen") != 0) {
    seekey_config_apply_theme(&fallback, config->theme);
  }
  const char *fallback_values[] = {
      fallback.foreground, fallback.background, fallback.border_color,
      fallback.shadow, fallback.placeholder_foreground,
      fallback.placeholder_background, fallback.placeholder_border_color,
  };
  gboolean needs_colors = FALSE;
  for (gsize i = 0; i < G_N_ELEMENTS(STYLE_THEME_KEYS); i++) {
    if (raw_values[i] != NULL &&
        g_str_has_prefix(raw_values[i], "@matugen:")) {
      needs_colors = TRUE;
      break;
    }
  }
  if (!needs_colors) return;

  char *path = config->matugen_path[0] != '\0'
                   ? g_strdup(config->matugen_path)
                   : seekey_matugen_resolve_path(0, NULL);
  GHashTable *colors = NULL;
  if (path != NULL && g_file_test(path, G_FILE_TEST_EXISTS)) {
    colors = seekey_matugen_load(path, NULL);
  }

  for (gsize i = 0; i < G_N_ELEMENTS(STYLE_THEME_KEYS); i++) {
    const char *raw = raw_values[i];
    if (raw == NULL || !g_str_has_prefix(raw, "@matugen:")) continue;
    char *resolved = seekey_matugen_resolve_value(raw, colors);
    gboolean unresolved = g_strcmp0(resolved, raw) == 0;
    if (g_strcmp0(current[i], raw) == 0 ||
        g_strcmp0(current[i], resolved) == 0 ||
        (theme_unchanged && unresolved &&
         g_strcmp0(current[i], fallback_values[i]) == 0)) {
      g_key_file_set_string(key_file, "style", STYLE_THEME_KEYS[i], raw);
    }
    g_free(resolved);
  }

  g_clear_pointer(&colors, g_hash_table_destroy);
  g_free(path);
}

static void config_resolve_matugen_values(SeekeyConfig *config,
                                          GHashTable *colors) {
  struct {
    char *value;
    gsize size;
  } fields[] = {
      {config->foreground, sizeof(config->foreground)},
      {config->background, sizeof(config->background)},
      {config->border_color, sizeof(config->border_color)},
      {config->shadow, sizeof(config->shadow)},
      {config->placeholder_foreground,
       sizeof(config->placeholder_foreground)},
      {config->placeholder_background,
       sizeof(config->placeholder_background)},
      {config->placeholder_border_color,
       sizeof(config->placeholder_border_color)},
  };

  for (gsize i = 0; i < G_N_ELEMENTS(fields); i++) {
    char *resolved = seekey_matugen_resolve_value(fields[i].value, colors);
    g_strlcpy(fields[i].value, resolved, fields[i].size);
    g_free(resolved);
  }
  replace_unresolved_matugen_values(config);
}

gboolean seekey_config_resolve_matugen(SeekeyConfig *config, GError **error) {
  g_return_val_if_fail(config != NULL, FALSE);

  const char *values[] = {
      config->foreground, config->background, config->border_color,
      config->shadow, config->placeholder_foreground,
      config->placeholder_background, config->placeholder_border_color,
  };
  gboolean needs_resolution = FALSE;
  for (gsize i = 0; i < G_N_ELEMENTS(values); i++) {
    if (g_str_has_prefix(values[i], "@matugen:")) {
      needs_resolution = TRUE;
      break;
    }
  }

  gboolean explicit_path = config->matugen_path[0] != '\0';
  if (!needs_resolution && !explicit_path) return TRUE;

  char *path = explicit_path ? g_strdup(config->matugen_path)
                             : seekey_matugen_resolve_path(0, NULL);
  GHashTable *colors = NULL;
  GError *load_error = NULL;
  if (path != NULL && g_file_test(path, G_FILE_TEST_EXISTS)) {
    colors = seekey_matugen_load(path, &load_error);
  } else if (explicit_path) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                "matugen file not found: %s", path != NULL ? path : "");
    g_free(path);
    return FALSE;
  }

  if (colors == NULL && load_error != NULL) {
    if (explicit_path) {
      g_propagate_error(error, load_error);
      g_free(path);
      return FALSE;
    }
    g_printerr("seekey: matugen load failed: %s\n", load_error->message);
    g_clear_error(&load_error);
  }

  if (needs_resolution) config_resolve_matugen_values(config, colors);
  g_clear_pointer(&colors, g_hash_table_destroy);
  g_free(path);
  return TRUE;
}

static void keyfile_preserve_theme_inheritance(GKeyFile *key_file,
                                               const SeekeyConfig *config,
                                               char **raw_values,
                                               gboolean existing_file) {
  if (!existing_file) return;

  SeekeyConfig themed;
  seekey_config_set_defaults(&themed);
  g_strlcpy(themed.theme, config->theme, sizeof(themed.theme));
  if (!seekey_config_apply_theme(&themed, config->theme)) return;

  if (g_strcmp0(config->theme, "matugen") == 0) {
    char *path = config->matugen_path[0] != '\0'
                     ? g_strdup(config->matugen_path)
                     : seekey_matugen_resolve_path(0, NULL);
    GHashTable *colors = NULL;
    if (path != NULL && g_file_test(path, G_FILE_TEST_EXISTS)) {
      colors = seekey_matugen_load(path, NULL);
    }
    config_resolve_matugen_values(&themed, colors);
    g_clear_pointer(&colors, g_hash_table_destroy);
    g_free(path);
  }

  const char *current[] = {
      config->foreground, config->background, config->border_color,
      config->shadow, config->placeholder_foreground,
      config->placeholder_background, config->placeholder_border_color,
  };
  const char *inherited[] = {
      themed.foreground, themed.background, themed.border_color,
      themed.shadow, themed.placeholder_foreground,
      themed.placeholder_background, themed.placeholder_border_color,
  };
  for (gsize i = 0; i < G_N_ELEMENTS(STYLE_THEME_KEYS); i++) {
    if (raw_values[i] == NULL &&
        g_strcmp0(current[i], inherited[i]) == 0) {
      g_key_file_remove_key(key_file, "style", STYLE_THEME_KEYS[i], NULL);
    }
  }
}

gboolean seekey_config_save(const SeekeyConfig *config, GError **error) {
  if (config->config_path[0] == '\0') {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "No config path set; nothing to save to");
    return FALSE;
  }

  char *raw_style_values[G_N_ELEMENTS(STYLE_THEME_KEYS)] = {0};
  char *raw_theme = NULL;
  GKeyFile *key_file = g_key_file_new();
  gboolean existing_file =
      g_file_test(config->config_path, G_FILE_TEST_EXISTS);
  if (existing_file &&
      !g_file_test(config->config_path, G_FILE_TEST_IS_REGULAR)) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "config path is not a regular file: %s",
                config->config_path);
    g_key_file_unref(key_file);
    return FALSE;
  }
  if (existing_file &&
      !g_key_file_load_from_file(key_file, config->config_path,
                                 G_KEY_FILE_KEEP_COMMENTS |
                                     G_KEY_FILE_KEEP_TRANSLATIONS,
                                 error)) {
    g_key_file_unref(key_file);
    return FALSE;
  }
  for (gsize i = 0; i < G_N_ELEMENTS(STYLE_THEME_KEYS); i++) {
    raw_style_values[i] = g_key_file_get_string(
        key_file, "style", STYLE_THEME_KEYS[i], NULL);
  }
  raw_theme = g_key_file_get_string(key_file, "general", "theme", NULL);
  gboolean theme_unchanged =
      g_strcmp0(raw_theme != NULL ? raw_theme : "default", config->theme) == 0;
  keyfile_set_config(key_file, config);
  keyfile_restore_matugen_references(key_file, config, raw_style_values,
                                     theme_unchanged);
  keyfile_preserve_theme_inheritance(key_file, config, raw_style_values,
                                     existing_file);
  for (gsize i = 0; i < G_N_ELEMENTS(raw_style_values); i++) {
    g_free(raw_style_values[i]);
  }
  g_free(raw_theme);

  gsize length = 0;
  char *data = g_key_file_to_data(key_file, &length, error);
  g_key_file_unref(key_file);
  if (data == NULL) {
    return FALSE;
  }

  char *dir = g_path_get_dirname(config->config_path);
  if (dir != NULL && strlen(dir) > 0 && g_mkdir_with_parents(dir, 0755) != 0) {
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                "Failed to create %s: %s", dir, g_strerror(errno));
    g_free(dir);
    g_free(data);
    return FALSE;
  }
  g_free(dir);

  gboolean ok =
      g_file_set_contents(config->config_path, data, (gssize)length, error);
  g_free(data);
  return ok;
}

gboolean seekey_config_init(SeekeyConfig *config, gboolean force,
                            GError **error) {
  if (config->config_path[0] == '\0') {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "No config path resolved; pass --config or run from a "
                        "project directory");
    return FALSE;
  }
  if (g_file_test(config->config_path, G_FILE_TEST_EXISTS) && !force) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                "Config already exists at %s; use --force to overwrite",
                config->config_path);
    return FALSE;
  }

  return seekey_config_save(config, error);
}

gboolean seekey_config_print(const SeekeyConfig *config, GError **error) {
  GKeyFile *key_file = g_key_file_new();
  keyfile_set_config(key_file, config);

  gsize length = 0;
  char *data = g_key_file_to_data(key_file, &length, error);
  g_key_file_unref(key_file);
  if (data == NULL) {
    return FALSE;
  }

  const char *source =
      config->config_path[0] != '\0' &&
              g_file_test(config->config_path, G_FILE_TEST_IS_REGULAR)
          ? "file"
          : "default";
  const char *path =
      config->config_path[0] != '\0' ? config->config_path : "(none)";
  g_print("# source: %s\n# path: %s\n%s", source, path, data);
  g_free(data);
  return TRUE;
}

gboolean seekey_config_validate(const SeekeyConfig *config, GError **error) {
  if (!valid_choice(config->align, "left", "center", "right")) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "align must be one of: left, center, right");
    return FALSE;
  }
  if (!valid_choice(config->disappear, "instant", "fade", NULL)) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "disappear must be one of: instant, fade");
    return FALSE;
  }
  if (!valid_choice(config->layer_shell, "auto", "required", "off")) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "layer-shell must be one of: auto, required, off");
    return FALSE;
  }
  if (!valid_choice(config->typing_display, "full", "masked", "off")) {
    g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                "typing-display must be one of: full, masked, off");
    return FALSE;
  }
  if (config->key_font_family[0] == '\0') {
    g_set_error_literal(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                        "font-family cannot be empty");
    return FALSE;
  }
  return TRUE;
}

/* ------------------------------------------------------------------ */
/* Argument parsing                                                    */
/* ------------------------------------------------------------------ */

static void print_help(void) {
  g_print(
      "Usage: seekey [OPTIONS]\n\n"
      "Options:\n"
      "  --config PATH          Config file path (default: ./seekey.ini)\n"
      "  --config-tui           Open terminal UI to edit and save "
      "configuration\n"
      "  --config-gui           Open graphical configuration menu\n"
      "  --desktop-launch       Launch using the saved desktop-entry mode\n"
      "  --init-config          Write the current default/config/CLI "
      "settings to config\n"
      "  --init-config --xdg    Write to ~/.config/seekey/config.ini "
      "instead\n"
      "  --matugen PATH         Path to a matugen colors.json (overrides "
      "env/default)\n"
      "  --force                Allow --init-config to overwrite an "
      "existing config\n"
      "  --print-config         Print the effective configuration and exit\n"
      "  --validate-config      Validate configuration and exit\n"
      "  --no-layer-shell       Disable gtk4-layer-shell even if available\n"
      "  --layer-shell auto|required|off\n"
      "  --theme NAME           Color preset: default, nord, dracula, "
      "catppuccin, monokai, light, matugen\n"
      "  --merge-repeats        Stack identical key bubbles as Key xN "
      "(default on)\n"
      "  --no-merge-repeats     Show each key press as a separate bubble\n"
      "  --merge-modifiers      Update modifier bubble when combo extends "
      "(default on)\n"
      "  --no-merge-modifiers   Keep each modifier press as a separate "
      "bubble\n"
      "  --show-mouse           Show mouse clicks (default off)\n"
      "  --no-mouse             Hide mouse clicks (default)\n"
      "  --duration MS          Bubble duration, default 1200\n"
      "  --typing-idle MS       Pause before typed text starts a new bubble, "
      "default 650\n"
      "  --typing-display MODE  Typed text: full, masked, or off (default "
      "full)\n"
      "  --fade-ms MS           Fade duration when disappear=fade, default "
      "180\n"
      "  --margin PX            Bottom margin in layer-shell mode, default 0\n"
      "  --margin-horizontal PX Horizontal margin for left/right anchor, "
      "default 0\n"
      "  --max-items N          Maximum visible bubbles, default 5\n"
      "  --align left|center|right\n"
      "  --disappear instant|fade\n"
      "  --debug-input          Print input discovery and events\n"
      "  -V, --version          Print version and exit\n"
      "  -h, --help             Show help\n");
}

gboolean seekey_cli_handle_info(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (g_strcmp0(argv[i], "-V") == 0 ||
        g_strcmp0(argv[i], "--version") == 0) {
      g_print("seekey %s\n", SEEKEY_VERSION);
      return TRUE;
    }
    if (g_strcmp0(argv[i], "-h") == 0 ||
        g_strcmp0(argv[i], "--help") == 0) {
      print_help();
      return TRUE;
    }
  }
  return FALSE;
}

gboolean seekey_parse_args(SeekeyConfig *config, int *argc, char ***argv,
                           GError **error) {
  for (int i = 1; i < *argc; i++) {
    const char *arg = (*argv)[i];
    if (g_strcmp0(arg, "--no-layer-shell") == 0) {
      config->no_layer_shell = TRUE;
      g_strlcpy(config->layer_shell, "off", sizeof(config->layer_shell));
    } else if (g_strcmp0(arg, "--debug-input") == 0) {
      config->debug_input = TRUE;
    } else if (g_strcmp0(arg, "--config-tui") == 0) {
      config->config_tui = TRUE;
    } else if (g_strcmp0(arg, "--config-gui") == 0) {
      config->config_gui = TRUE;
    } else if (g_strcmp0(arg, "--preview-child") == 0) {
      config->preview_child = TRUE;
    } else if (g_strcmp0(arg, "--desktop-launch") == 0) {
      config->desktop_launch = TRUE;
    } else if (g_strcmp0(arg, "--init-config") == 0) {
      config->init_config = TRUE;
    } else if (g_strcmp0(arg, "--force") == 0) {
      config->force = TRUE;
    } else if (g_strcmp0(arg, "--print-config") == 0) {
      config->print_config = TRUE;
    } else if (g_strcmp0(arg, "--validate-config") == 0) {
      config->validate_config = TRUE;
    } else if (g_strcmp0(arg, "--xdg") == 0) {
      config->xdg_config = TRUE;
    } else if (g_strcmp0(arg, "--matugen") == 0) {
      i++;
      if (!copy_path_option("--matugen", i < *argc ? (*argv)[i] : NULL,
                            config->matugen_path,
                            sizeof(config->matugen_path), error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--config") == 0) {
      i++;
      if (!copy_path_option("--config", i < *argc ? (*argv)[i] : NULL,
                            config->config_path,
                            sizeof(config->config_path), error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--duration") == 0) {
      if (++i >= *argc || !parse_uint_arg("--duration", (*argv)[i], 100, 10000,
                                          &config->duration_ms, error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--typing-idle") == 0) {
      if (++i >= *argc ||
          !parse_uint_arg("--typing-idle", (*argv)[i], 100, 5000,
                          &config->typing_idle_ms, error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--typing-display") == 0) {
      if (++i >= *argc ||
          !parse_choice_arg("--typing-display", (*argv)[i], "full", "masked",
                            "off", config->typing_display,
                            sizeof(config->typing_display), error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--fade-ms") == 0) {
      if (++i >= *argc || !parse_uint_arg("--fade-ms", (*argv)[i], 0, 3000,
                                          &config->fade_ms, error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--margin") == 0) {
      if (++i >= *argc || !parse_uint_arg("--margin", (*argv)[i], 0, 1000,
                                          &config->margin_px, error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--margin-horizontal") == 0) {
      if (++i >= *argc ||
          !parse_uint_arg("--margin-horizontal", (*argv)[i], 0, 1000,
                          &config->margin_horizontal_px, error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--max-items") == 0) {
      if (++i >= *argc || !parse_uint_arg("--max-items", (*argv)[i], 1, 20,
                                          &config->max_items, error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--align") == 0) {
      if (++i >= *argc ||
          !parse_choice_arg("--align", (*argv)[i], "left", "center", "right",
                            config->align, sizeof(config->align), error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--disappear") == 0) {
      if (++i >= *argc ||
          !parse_choice_arg("--disappear", (*argv)[i], "instant", "fade", NULL,
                            config->disappear, sizeof(config->disappear),
                            error)) {
        return FALSE;
      }
    } else if (g_strcmp0(arg, "--layer-shell") == 0) {
      if (++i >= *argc ||
          !parse_choice_arg("--layer-shell", (*argv)[i], "auto", "required",
                            "off", config->layer_shell,
                            sizeof(config->layer_shell), error)) {
        return FALSE;
      }
      config->no_layer_shell = g_strcmp0(config->layer_shell, "off") == 0;
    } else if (g_strcmp0(arg, "--merge-repeats") == 0) {
      config->merge_repeats = TRUE;
    } else if (g_strcmp0(arg, "--no-merge-repeats") == 0) {
      config->merge_repeats = FALSE;
    } else if (g_strcmp0(arg, "--merge-modifiers") == 0) {
      config->merge_modifiers = TRUE;
    } else if (g_strcmp0(arg, "--no-merge-modifiers") == 0) {
      config->merge_modifiers = FALSE;
    } else if (g_strcmp0(arg, "--show-mouse") == 0) {
      config->show_mouse = TRUE;
    } else if (g_strcmp0(arg, "--no-mouse") == 0) {
      config->show_mouse = FALSE;
    } else if (g_strcmp0(arg, "--theme") == 0) {
      if (++i >= *argc) {
        g_set_error_literal(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                            "--theme requires a name");
        return FALSE;
      }
      g_strlcpy(config->theme, (*argv)[i], sizeof(config->theme));
      if (!seekey_config_apply_theme(config, config->theme)) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                    "Unknown theme: %s", config->theme);
        return FALSE;
      }
    } else if (g_strcmp0(arg, "-V") == 0 || g_strcmp0(arg, "--version") == 0) {
      g_print("seekey %s\n", SEEKEY_VERSION);
      exit(0);
    } else if (g_strcmp0(arg, "-h") == 0 || g_strcmp0(arg, "--help") == 0) {
      print_help();
      exit(0);
    } else {
      g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                  "Unknown option: %s", arg);
      return FALSE;
    }
  }

  return TRUE;
}
