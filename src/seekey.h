#pragma once

#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
    gboolean no_layer_shell;
    gboolean debug_input;
    guint duration_ms;
    guint typing_idle_ms;
    guint fade_ms;
    guint margin_px;
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
    char foreground[64];
    char background[64];
    char border_color[64];
    char shadow[128];
    char placeholder_text[64];
    char placeholder_foreground[64];
    char placeholder_background[64];
    char placeholder_border_color[64];
    char config_path[512];
} SeekeyConfig;

typedef struct {
    guint code;
    gint value;
    gboolean shifted;
    gboolean has_non_shift_modifier;
    char name[64];
    char combo[256];
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
gboolean seekey_is_modifier(guint code);
int seekey_modifier_order(guint code);

gboolean seekey_layer_shell_try_init(GtkWindow *window,
                                     const SeekeyConfig *config,
                                     GError **error);
