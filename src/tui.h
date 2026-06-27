#ifndef SEEKEY_TUI_H
#define SEEKEY_TUI_H

#include "seekey.h"
#include <glib.h>

/* Field groups, used by the tabbed TUI. Order = tab order. */
typedef enum {
    TUI_GROUP_GENERAL = 0,   /* timing, behaviour, layer-shell, theme */
    TUI_GROUP_LAYOUT,        /* align, spacing, padding, sizing       */
    TUI_GROUP_APPEARANCE,    /* colors, shadow, fonts                  */
    TUI_GROUP_PLACEHOLDER,   /* placeholder text + colors              */
    TUI_GROUP_COUNT,
} TuiGroup;

typedef enum {
    TUI_UINT,
    TUI_STRING,
    TUI_CHOICE,
    TUI_BOOL,
    TUI_COLOR,
} TuiFieldType;

typedef struct {
    const char *label;
    const char *help;
    const char *input_hint;
    TuiFieldType type;
    TuiGroup group;
    guint *uint_target;
    guint min, max, step;
    guint default_uint;
    char *string_target;
    gsize string_size;
    char default_string[128];
    const char **choices;
    guint choice_count;
    gboolean *bool_target;
    gboolean default_bool;
} TuiField;

/* Group display names (tab labels). */
const char *tui_group_name(TuiGroup g);

/* Pure helpers (no ncurses); safe to call from tests. */
guint tui_current_choice_index(const TuiField *field);
void   tui_field_value(const TuiField *field, char *buffer, gsize size);
void   tui_adjust_field(TuiField *field, int direction);
int    tui_nearest_color_index(const char *hex);
void   tui_reset_field(TuiField *field);
void   tui_build_fields(TuiField *out, size_t *out_count, SeekeyConfig *config);

/* Count fields that belong to a given group (for the tabbed view). */
size_t tui_count_in_group(const TuiField *fields, size_t count, TuiGroup g);

/* Total number of fields built by tui_build_fields (for assertions). */
#define TUI_FIELD_COUNT 33

/* Open the interactive TUI editor. Returns TRUE if no error. On error
 * sets `error` and TUI has been torn down. */
gboolean seekey_tui_run(SeekeyConfig *config, GError **error);

#endif
