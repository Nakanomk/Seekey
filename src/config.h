#ifndef SEEKEY_CONFIG_H
#define SEEKEY_CONFIG_H

#include "seekey.h"
#include <glib.h>

/* Built-in color theme presets. */
typedef struct {
    const char *name;
    const char *foreground;
    const char *background;
    const char *border_color;
    const char *shadow;
    const char *placeholder_foreground;
    const char *placeholder_background;
    const char *placeholder_border_color;
} SeekeyThemePreset;

/* Returns the number of built-in theme presets. */
gsize seekey_config_theme_count(void);

/* Returns a pointer to the i-th preset. */
const SeekeyThemePreset *seekey_config_theme_at(gsize i);

/* Look up a theme by name; returns NULL if unknown. */
const SeekeyThemePreset *seekey_config_theme_lookup(const char *name);

/* Fill `config` with hard-coded defaults. */
void seekey_config_set_defaults(SeekeyConfig *config);

/* Apply a theme preset by name to `config`. Unknown names keep current
 * colors. Returns TRUE if a preset was applied. */
gboolean seekey_config_apply_theme(SeekeyConfig *config, const char *name);

/* Resolve the effective config path. Order:
 *   1. --config <path>   (explicit, highest priority)
 *   2. <cwd>/seekey.ini  (project directory)
 *   3. ""                (no file: pure defaults)
 * Already parses --config from argv. */
gboolean seekey_config_resolve_path(SeekeyConfig *config, int argc, char **argv,
                                    GError **error);

/* Load configuration from `config->config_path`. If the file doesn't
 * exist, returns TRUE with defaults preserved. */
gboolean seekey_config_load(SeekeyConfig *config, GError **error);

/* Save the current config to `config->config_path`. Creates parent
 * directories as needed. */
gboolean seekey_config_save(const SeekeyConfig *config, GError **error);

/* Write the effective configuration to `config->config_path` if it
 * doesn't already exist. If `force` is TRUE, overwrite. */
gboolean seekey_config_init(SeekeyConfig *config, gboolean force, GError **error);

/* Print effective configuration to stdout, prefixed with source path. */
gboolean seekey_config_print(const SeekeyConfig *config, GError **error);

/* Validate the configuration; returns TRUE if everything is consistent. */
gboolean seekey_config_validate(const SeekeyConfig *config, GError **error);

/* Parse command-line arguments. Mutates `config` and `*argc`/`*argv`
 * for GTK. Returns FALSE on parse error. */
gboolean seekey_parse_args(SeekeyConfig *config, int *argc, char ***argv,
                           GError **error);

/* Returns TRUE if `flag` is present in argv. */
gboolean seekey_cli_has_flag(int argc, char **argv, const char *flag);

/* Build the default save path: <cwd>/seekeyini. Caller frees with g_free.
 * Returns NULL if cwd cannot be determined (in which case "." is used). */
char *seekey_default_save_path(void);

/* Determine the path to a matugen-generated colors.json file.
 * Priority:
 *   1. CLI flag --matugen <path> (parsed out of argv here)
 *   2. Environment variable MATUGEN_COLORS
 *   3. $XDG_CACHE_HOME/matugen/colors.json
 *   4. ~/.cache/matugen/colors.json
 * Returns a newly allocated string (caller frees). NULL if neither the
 * environment override nor a default path can be formed. */
char *seekey_matugen_resolve_path(int argc, char **argv);

/* Load a matugen colors.json file into a new GHashTable mapping role
 * names (e.g. "surface", "on_surface") to color strings (e.g. "#1a1a1a").
 * Returns NULL and sets `error` on failure (missing keys structure,
 * malformed JSON, missing file). Caller frees with
 * g_hash_table_destroy() + the table's free functions. */
GHashTable *seekey_matugen_load(const char *path, GError **error);

/* Resolve a value of the form "@matugen:<role>" or
 * "@matugen:<role>@<alpha>" against the colors table. If the value
 * doesn't start with "@matugen:" it is duplicated verbatim. If the role
 * isn't in the table, the original value (without the prefix) is
 * returned. The caller frees the result with g_free. */
char *seekey_matugen_resolve_value(const char *value, GHashTable *colors);

/* Locate --config <path> in argv and copy to `config->config_path` if
 * present. Does not return an error; missing flag is non-fatal. */
void seekey_cli_extract_config_path(SeekeyConfig *config, int argc, char **argv);

#endif
