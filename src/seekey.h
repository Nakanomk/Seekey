#pragma once

#define SEEKEY_VERSION "0.2.0"

#include <glib.h>
#include <gtk/gtk.h>

/* Bitmask flags for active modifiers, used to compare modifier sets. */
enum {
    SEEKEY_MOD_CTRL  = 1ULL << 0,
    SEEKEY_MOD_SHIFT = 1ULL << 1,
    SEEKEY_MOD_ALT   = 1ULL << 2,
    SEEKEY_MOD_SUPER = 1ULL << 3,
};

/* Pseudo key-codes for scroll events — safely above KEY_MAX. */
enum {
    SEEKEY_SCROLL_UP    = 0x10000,
    SEEKEY_SCROLL_DOWN  = 0x10001,
    SEEKEY_SCROLL_LEFT  = 0x10002,
    SEEKEY_SCROLL_RIGHT = 0x10003,
};

#define SEEKEY_MAX_ICON_OVERRIDES 64

typedef struct {
    char name[32];
    char icon[16];
} IconOverride;

typedef struct {
    gboolean no_layer_shell;
    gboolean debug_input;
    gboolean config_tui;
    gboolean init_config;
    gboolean force;
    gboolean print_config;
    gboolean validate_config;
    gboolean xdg_config;
    gboolean merge_repeats;
    gboolean merge_modifiers;
    gboolean show_mouse;
    guint duration_ms;
    guint typing_idle_ms;
    guint fade_ms;
    guint margin_px;
    guint margin_horizontal_px;
    guint max_items;
    guint window_width;
    guint window_height;
    guint box_spacing;
    guint overlay_padding;
    guint key_min_width;
    guint key_padding_x;
    guint key_padding_y;
    guint key_radius;
    guint key_border_width;
    guint key_font_px;
    guint key_font_weight;
    guint typing_max_width;
    char align[16];
    char disappear[16];
    char layer_shell[16];
    char theme[32];
    char foreground[64];
    char background[64];
    char border_color[64];
    char shadow[128];
    char placeholder_text[64];
    char placeholder_foreground[64];
    char placeholder_background[64];
    char placeholder_border_color[64];
    char config_path[512];
    char matugen_path[512];
    IconOverride icon_overrides[SEEKEY_MAX_ICON_OVERRIDES];
    guint icon_override_count;
} SeekeyConfig;

typedef struct {
    guint code;
    gint value;
    gboolean shifted;
    gboolean has_non_shift_modifier;
    char name[64];
    char combo[256];
    guint64 modifier_mask;
    gboolean is_mouse;
} KeyEventMessage;

typedef void (*SeekeyKeyCallback)(const KeyEventMessage *event, gpointer user_data);

typedef struct SeekeyInput SeekeyInput;

SeekeyInput *seekey_input_new(const SeekeyConfig *config,
                              SeekeyKeyCallback callback,
                              gpointer user_data,
                              GError **error);
void seekey_input_start(SeekeyInput *input);
void seekey_input_stop(SeekeyInput *input);
void seekey_input_free(SeekeyInput *input);

const char *seekey_key_name(guint code);
const char *seekey_key_text(guint code, gboolean shifted);
const char *seekey_key_icon(guint code, const SeekeyConfig *config);
const char *seekey_mouse_button_name(guint code);
gboolean seekey_is_modifier(guint code);
int seekey_modifier_order(guint code);

gboolean seekey_layer_shell_try_init(GtkWindow *window,
                                     const SeekeyConfig *config,
                                     GdkMonitor *monitor,
                                     GError **error);
