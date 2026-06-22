#include "seekey.h"

#include <errno.h>
#include <locale.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *placeholder;
    GtkWidget *typing_label;
    GString *typing_text;
    guint typing_remove_timeout_id;
    guint typing_idle_timeout_id;
    GtkWidget *last_bubble;
    guint last_bubble_id;
    guint last_bubble_count;
    guint last_bubble_timeout_id;
    gboolean last_bubble_fading;
    char last_combo[256];
    guint last_key_code;
    guint64 last_modifier_mask;
    SeekeyInput *input;
    SeekeyConfig config;
    guint next_id;
    gboolean has_seen_input;
} AppState;

typedef struct {
    AppState *state;
    GtkWidget *widget;
    guint id;
    gboolean fade_phase;
} Bubble;

typedef struct {
    AppState *state;
    GtkWidget *widget;
    gboolean fade_phase;
} TypingTimeout;

static void cancel_active_typing_remove_timeout(AppState *state);
static void cancel_active_typing_idle_timeout(AppState *state);
static gboolean remove_bubble(gpointer data);
static gboolean remove_typing_bubble(gpointer data);
static void bubble_free(gpointer data);
static void typing_timeout_free(gpointer data);

static void detach_last_bubble(AppState *state)
{
    state->last_bubble = NULL;
    state->last_bubble_id = 0;
    state->last_bubble_count = 0;
    state->last_bubble_timeout_id = 0;
    state->last_bubble_fading = FALSE;
    state->last_combo[0] = '\0';
    state->last_key_code = 0;
    state->last_modifier_mask = 0;
}

static void cancel_last_bubble_timeout(AppState *state)
{
    if (state->last_bubble_timeout_id != 0) {
        g_source_remove(state->last_bubble_timeout_id);
        state->last_bubble_timeout_id = 0;
    }
}

static gboolean should_fade(const SeekeyConfig *config)
{
    return g_strcmp0(config->disappear, "fade") == 0 && config->fade_ms > 0;
}

static gboolean remove_bubble(gpointer data)
{
    Bubble *bubble = data;
    if (should_fade(&bubble->state->config) && !bubble->fade_phase) {
        Bubble *next = g_new0(Bubble, 1);
        next->state = bubble->state;
        next->widget = g_object_ref(bubble->widget);
        next->id = bubble->id;
        next->fade_phase = TRUE;
        gtk_widget_add_css_class(bubble->widget, "fading");
        if (bubble->state->last_bubble == bubble->widget &&
            bubble->state->last_bubble_id == bubble->id) {
            bubble->state->last_bubble_timeout_id = 0;
            bubble->state->last_bubble_fading = TRUE;
        }
        g_timeout_add_full(G_PRIORITY_DEFAULT,
                           bubble->state->config.fade_ms,
                           remove_bubble,
                           next,
                           bubble_free);
        return G_SOURCE_REMOVE;
    }

    GtkWidget *parent = gtk_widget_get_parent(bubble->widget);
    if (parent != NULL) {
        gtk_box_remove(GTK_BOX(parent), bubble->widget);
    }
    if (bubble->state->last_bubble == bubble->widget &&
        bubble->state->last_bubble_id == bubble->id) {
        detach_last_bubble(bubble->state);
    }
    return G_SOURCE_REMOVE;
}

static void bubble_free(gpointer data)
{
    Bubble *bubble = data;
    g_clear_object(&bubble->widget);
    g_free(bubble);
}

static void clear_typing_group(AppState *state)
{
    if (state->typing_text != NULL) {
        g_string_free(state->typing_text, TRUE);
        state->typing_text = NULL;
    }
    state->typing_label = NULL;
}

static gboolean remove_typing_bubble(gpointer data)
{
    TypingTimeout *timeout = data;
    AppState *state = timeout->state;

    if (should_fade(&state->config) && !timeout->fade_phase) {
        TypingTimeout *next = g_new0(TypingTimeout, 1);
        next->state = state;
        next->widget = g_object_ref(timeout->widget);
        next->fade_phase = TRUE;
        gtk_widget_add_css_class(timeout->widget, "fading");
        g_timeout_add_full(G_PRIORITY_DEFAULT,
                           state->config.fade_ms,
                           remove_typing_bubble,
                           next,
                           typing_timeout_free);
        return G_SOURCE_REMOVE;
    }

    if (timeout->widget != NULL) {
        GtkWidget *parent = gtk_widget_get_parent(timeout->widget);
        if (parent != NULL) {
            gtk_box_remove(GTK_BOX(parent), timeout->widget);
        }
    }

    if (state->typing_label == timeout->widget) {
        cancel_active_typing_idle_timeout(state);
        clear_typing_group(state);
        state->typing_remove_timeout_id = 0;
    }

    return G_SOURCE_REMOVE;
}

static gboolean finish_typing_group(gpointer data)
{
    TypingTimeout *timeout = data;
    AppState *state = timeout->state;

    if (state->typing_label == timeout->widget) {
        clear_typing_group(state);
        state->typing_remove_timeout_id = 0;
        state->typing_idle_timeout_id = 0;
    }

    return G_SOURCE_REMOVE;
}

static void typing_timeout_free(gpointer data)
{
    TypingTimeout *timeout = data;
    g_clear_object(&timeout->widget);
    g_free(timeout);
}

static void cancel_active_typing_remove_timeout(AppState *state)
{
    if (state->typing_remove_timeout_id != 0) {
        g_source_remove(state->typing_remove_timeout_id);
        state->typing_remove_timeout_id = 0;
    }
}

static void cancel_active_typing_idle_timeout(AppState *state)
{
    if (state->typing_idle_timeout_id != 0) {
        g_source_remove(state->typing_idle_timeout_id);
        state->typing_idle_timeout_id = 0;
    }
}

static void schedule_typing_timeouts(AppState *state)
{
    cancel_active_typing_remove_timeout(state);
    cancel_active_typing_idle_timeout(state);

    TypingTimeout *remove_timeout = g_new0(TypingTimeout, 1);
    remove_timeout->state = state;
    remove_timeout->widget = g_object_ref(state->typing_label);
    state->typing_remove_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                         state->config.duration_ms,
                                                         remove_typing_bubble,
                                                         remove_timeout,
                                                         typing_timeout_free);

    TypingTimeout *idle_timeout = g_new0(TypingTimeout, 1);
    idle_timeout->state = state;
    idle_timeout->widget = g_object_ref(state->typing_label);
    state->typing_idle_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT,
                                                       state->config.typing_idle_ms,
                                                       finish_typing_group,
                                                       idle_timeout,
                                                       typing_timeout_free);
}

static guint count_children(GtkWidget *box)
{
    guint count = 0;
    for (GtkWidget *child = gtk_widget_get_first_child(box);
         child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        count++;
    }
    return count;
}

static void trim_bubbles(AppState *state)
{
    while (count_children(state->box) > state->config.max_items) {
        GtkWidget *child = gtk_widget_get_first_child(state->box);
        if (child == NULL) {
            break;
        }
        if (child == state->typing_label) {
            cancel_active_typing_remove_timeout(state);
            cancel_active_typing_idle_timeout(state);
            clear_typing_group(state);
        }
        if (child == state->placeholder) {
            state->placeholder = NULL;
        }
        if (child == state->last_bubble) {
            cancel_last_bubble_timeout(state);
            detach_last_bubble(state);
        }
        gtk_box_remove(GTK_BOX(state->box), child);
    }
}

static void remove_placeholder(AppState *state)
{
    if (state->placeholder == NULL) {
        return;
    }

    GtkWidget *parent = gtk_widget_get_parent(state->placeholder);
    if (parent != NULL) {
        gtk_box_remove(GTK_BOX(parent), state->placeholder);
    }
    state->placeholder = NULL;
}

static guint schedule_bubble_timeout(AppState *state, GtkWidget *widget, guint id)
{
    Bubble *bubble = g_new0(Bubble, 1);
    bubble->state = state;
    bubble->widget = g_object_ref(widget);
    bubble->id = id;
    return g_timeout_add_full(G_PRIORITY_DEFAULT,
                              state->config.duration_ms,
                              remove_bubble,
                              bubble,
                              bubble_free);
}

static gboolean try_merge_last_bubble(AppState *state, const char *combo)
{
    if (state->last_bubble == NULL ||
        state->last_bubble_fading ||
        gtk_widget_get_parent(state->last_bubble) == NULL ||
        g_strcmp0(state->last_combo, combo) != 0) {
        return FALSE;
    }

    state->last_bubble_count++;
    char label[320];
    g_snprintf(label, sizeof(label), "%s x%u", combo, state->last_bubble_count);
    gtk_label_set_text(GTK_LABEL(state->last_bubble), label);

    cancel_last_bubble_timeout(state);
    state->last_bubble_timeout_id =
        schedule_bubble_timeout(state, state->last_bubble, state->last_bubble_id);
    return TRUE;
}

static void remember_last_bubble(AppState *state, GtkWidget *widget,
                                   const KeyEventMessage *event)
{
    state->last_bubble = widget;
    state->last_bubble_id = ++state->next_id;
    state->last_bubble_count = 1;
    state->last_bubble_fading = FALSE;
    g_strlcpy(state->last_combo, event->combo, sizeof(state->last_combo));
    state->last_key_code = event->code;
    state->last_modifier_mask = event->modifier_mask;
    state->last_bubble_timeout_id =
        schedule_bubble_timeout(state, widget, state->last_bubble_id);
}

static void on_key_event(const KeyEventMessage *event, gpointer user_data)
{
    AppState *state = user_data;
    const char *typed = NULL;

    if (!state->has_seen_input) {
        state->has_seen_input = TRUE;
        remove_placeholder(state);
    }

    /* --- Mouse button events: simple one-shot bubbles --- */
    if (event->is_mouse) {
        if (!state->config.show_mouse) return;

        cancel_active_typing_idle_timeout(state);
        clear_typing_group(state);
        state->typing_remove_timeout_id = 0;
        detach_last_bubble(state);

        const char *icon = seekey_key_icon(event->code, &state->config);
        GtkWidget *label = gtk_label_new(icon ? icon : event->name);
        gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
        gtk_widget_add_css_class(label, "key-bubble");
        gtk_box_append(GTK_BOX(state->box), label);
        trim_bubbles(state);

        /* Don't merge mouse repeats — each click is its own bubble. */
        state->last_bubble = label;
        state->last_bubble_id = ++state->next_id;
        state->last_bubble_timeout_id =
            schedule_bubble_timeout(state, label, state->last_bubble_id);
        return;
    }

    /* When the last bubble was a modifier combo and the incoming event
     * extends it (more modifiers added, or a non-modifier key added to
     * a modifier-only bubble), update the label in place instead of
     * creating a new bubble.  Uses a bitmask comparison so the press
     * order (e.g. Shift then Ctrl vs Ctrl then Shift) doesn't matter. */
    if (state->config.merge_modifiers &&
        state->last_bubble != NULL &&
        !state->last_bubble_fading &&
        gtk_widget_get_parent(state->last_bubble) != NULL &&
        state->last_modifier_mask != 0 &&
        (event->modifier_mask & state->last_modifier_mask) == state->last_modifier_mask) {

        gboolean extends =
            event->modifier_mask != state->last_modifier_mask ||
            seekey_is_modifier(state->last_key_code);

        if (extends) {
            gtk_label_set_text(GTK_LABEL(state->last_bubble), event->combo);
            g_strlcpy(state->last_combo, event->combo, sizeof(state->last_combo));
            state->last_key_code = event->code;
            state->last_modifier_mask = event->modifier_mask;

            cancel_last_bubble_timeout(state);
            state->last_bubble_timeout_id =
                schedule_bubble_timeout(state, state->last_bubble, state->last_bubble_id);

            /* Cancel any active typing — we're showing this as a combo. */
            cancel_active_typing_idle_timeout(state);
            clear_typing_group(state);
            state->typing_remove_timeout_id = 0;
            return;
        }
    }

    if (!event->has_non_shift_modifier && !seekey_is_modifier(event->code)) {
        typed = seekey_key_text(event->code, event->shifted);
    }

    if (typed != NULL) {
        detach_last_bubble(state);
        if (state->typing_label == NULL ||
            gtk_widget_get_parent(state->typing_label) == NULL) {
            cancel_active_typing_idle_timeout(state);
            clear_typing_group(state);
            state->typing_remove_timeout_id = 0;
            state->typing_label = gtk_label_new("");
            state->typing_text = g_string_new(NULL);
            gtk_label_set_single_line_mode(GTK_LABEL(state->typing_label), TRUE);
            gtk_label_set_ellipsize(GTK_LABEL(state->typing_label), PANGO_ELLIPSIZE_END);
            gtk_widget_add_css_class(state->typing_label, "key-bubble");
            gtk_widget_add_css_class(state->typing_label, "typing-bubble");
            gtk_box_append(GTK_BOX(state->box), state->typing_label);
            trim_bubbles(state);
        }

        g_string_append(state->typing_text, typed);
        gtk_label_set_text(GTK_LABEL(state->typing_label), state->typing_text->str);

        schedule_typing_timeouts(state);
        return;
    }

    cancel_active_typing_idle_timeout(state);
    clear_typing_group(state);
    state->typing_remove_timeout_id = 0;

    if (state->config.merge_repeats && try_merge_last_bubble(state, event->combo)) {
        return;
    }

    GtkWidget *label = gtk_label_new(event->combo);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(label, "key-bubble");
    gtk_box_append(GTK_BOX(state->box), label);
    trim_bubbles(state);

    remember_last_bubble(state, label, event);
}

static void install_css(const SeekeyConfig *config)
{
    char *css = g_strdup_printf(
        "window { background: transparent; }"
        ".overlay-root {"
        "  padding: %upx;"
        "  background: transparent;"
        "}"
        ".key-bubble {"
        "  padding: %upx %upx;"
        "  border-radius: %upx;"
        "  color: %s;"
        "  background: %s;"
        "  border: %upx solid %s;"
        "  box-shadow: %s;"
        "  font-size: %upx;"
        "  font-weight: %u;"
        "  opacity: 1;"
        "  transition: opacity %ums ease-out;"
        "}"
        ".typing-bubble {"
        "  max-width: %upx;"
        "}"
        ".placeholder-bubble {"
        "  color: %s;"
        "  background: %s;"
        "  border-color: %s;"
        "}"
        ".fading {"
        "  opacity: 0;"
        "}",
        config->overlay_padding,
        config->key_padding_y,
        config->key_padding_x,
        config->key_radius,
        config->foreground,
        config->background,
        config->key_border_width,
        config->border_color,
        config->shadow,
        config->key_font_px,
        config->key_font_weight,
        config->fade_ms,
        config->typing_max_width,
        config->placeholder_foreground,
        config->placeholder_background,
        config->placeholder_border_color);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css);
    g_free(css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static GtkAlign configured_halign(const SeekeyConfig *config)
{
    if (g_strcmp0(config->align, "left") == 0) {
        return GTK_ALIGN_START;
    }
    if (g_strcmp0(config->align, "center") == 0) {
        return GTK_ALIGN_CENTER;
    }
    return GTK_ALIGN_END;
}

static gboolean parse_uint_arg(const char *name,
                               const char *value,
                               guint min,
                               guint max,
                               guint *out,
                               GError **error)
{
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

static void config_set_defaults(SeekeyConfig *config); /* forward decl */

/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    const char *foreground;
    const char *background;
    const char *border_color;
    const char *shadow;
    const char *placeholder_foreground;
    const char *placeholder_background;
    const char *placeholder_border_color;
} ThemePreset;

static const ThemePreset THEME_PRESETS[] = {
    {"default",
     "#f7f7f2", "alpha(#111318, 0.86)", "alpha(#ffffff, 0.18)",
     "0 7px 22px alpha(#000000, 0.30)",
     "alpha(#f7f7f2, 0.74)", "alpha(#111318, 0.56)", "alpha(#ffffff, 0.14)"},
    {"light",
     "#1a1a2e", "alpha(#fafafa, 0.90)", "alpha(#cccccc, 0.40)",
     "0 7px 22px alpha(#000000, 0.12)",
     "alpha(#1a1a2e, 0.60)", "alpha(#e8e8e8, 0.70)", "alpha(#bbbbbb, 0.35)"},
    {"nord",
     "#d8dee9", "alpha(#2e3440, 0.90)", "alpha(#88c0d0, 0.25)",
     "0 7px 22px alpha(#000000, 0.38)",
     "alpha(#d8dee9, 0.70)", "alpha(#2e3440, 0.58)", "alpha(#88c0d0, 0.16)"},
    {"dracula",
     "#f8f8f2", "alpha(#282a36, 0.90)", "alpha(#bd93f9, 0.24)",
     "0 7px 22px alpha(#000000, 0.38)",
     "alpha(#f8f8f2, 0.70)", "alpha(#282a36, 0.58)", "alpha(#bd93f9, 0.16)"},
    {"catppuccin",
     "#cdd6f4", "alpha(#1e1e2e, 0.90)", "alpha(#cba6f7, 0.22)",
     "0 7px 22px alpha(#000000, 0.38)",
     "alpha(#cdd6f4, 0.72)", "alpha(#1e1e2e, 0.58)", "alpha(#cba6f7, 0.14)"},
    {"monokai",
     "#f8f8f2", "alpha(#272822, 0.90)", "alpha(#a6e22e, 0.22)",
     "0 7px 22px alpha(#000000, 0.38)",
     "alpha(#f8f8f2, 0.72)", "alpha(#272822, 0.58)", "alpha(#a6e22e, 0.14)"},
};

static void apply_theme_preset(SeekeyConfig *config, const char *name)
{
    for (gsize i = 0; i < G_N_ELEMENTS(THEME_PRESETS); i++) {
        if (g_strcmp0(THEME_PRESETS[i].name, name) == 0) {
            g_strlcpy(config->foreground, THEME_PRESETS[i].foreground,
                      sizeof(config->foreground));
            g_strlcpy(config->background, THEME_PRESETS[i].background,
                      sizeof(config->background));
            g_strlcpy(config->border_color, THEME_PRESETS[i].border_color,
                      sizeof(config->border_color));
            g_strlcpy(config->shadow, THEME_PRESETS[i].shadow,
                      sizeof(config->shadow));
            g_strlcpy(config->placeholder_foreground,
                      THEME_PRESETS[i].placeholder_foreground,
                      sizeof(config->placeholder_foreground));
            g_strlcpy(config->placeholder_background,
                      THEME_PRESETS[i].placeholder_background,
                      sizeof(config->placeholder_background));
            g_strlcpy(config->placeholder_border_color,
                      THEME_PRESETS[i].placeholder_border_color,
                      sizeof(config->placeholder_border_color));
            return;
        }
    }
    /* Unknown theme: keep current colors, the config file can override. */
}

static void config_set_defaults(SeekeyConfig *config)
{
    config->merge_repeats = TRUE;
    config->merge_modifiers = TRUE;
    config->show_mouse = TRUE;
    config->duration_ms = 1200;
    config->typing_idle_ms = 650;
    config->fade_ms = 180;
    config->margin_px = 96;
    config->margin_horizontal_px = 0;
    config->max_items = 5;
    config->window_width = 720;
    config->window_height = 160;
    config->box_spacing = 7;
    config->overlay_padding = 12;
    config->key_min_width = 0;  /* 0 = content-based, >0 = fixed minimum */
    config->key_padding_x = 14;
    config->key_padding_y = 8;
    config->key_radius = 6;
    config->key_border_width = 1;
    config->key_font_px = 20;
    config->key_font_weight = 700;
    config->typing_max_width = 480;
    g_strlcpy(config->align, "right", sizeof(config->align));
    g_strlcpy(config->disappear, "fade", sizeof(config->disappear));
    g_strlcpy(config->layer_shell, "auto", sizeof(config->layer_shell));
    g_strlcpy(config->theme, "default", sizeof(config->theme));
    config->icon_override_count = 0;
    g_strlcpy(config->foreground, "#f7f7f2", sizeof(config->foreground));
    g_strlcpy(config->background, "alpha(#111318, 0.86)", sizeof(config->background));
    g_strlcpy(config->border_color, "alpha(#ffffff, 0.18)", sizeof(config->border_color));
    g_strlcpy(config->shadow, "0 7px 22px alpha(#000000, 0.30)", sizeof(config->shadow));
    g_strlcpy(config->placeholder_text, "Seekey", sizeof(config->placeholder_text));
    g_strlcpy(config->placeholder_foreground,
              "alpha(#f7f7f2, 0.74)",
              sizeof(config->placeholder_foreground));
    g_strlcpy(config->placeholder_background,
              "alpha(#111318, 0.56)",
              sizeof(config->placeholder_background));
    g_strlcpy(config->placeholder_border_color,
              "alpha(#ffffff, 0.14)",
              sizeof(config->placeholder_border_color));
    g_snprintf(config->config_path,
               sizeof(config->config_path),
               "%s/seekey/config.ini",
               g_get_user_config_dir());
}

static gboolean valid_choice(const char *value, const char *a, const char *b, const char *c)
{
    return g_strcmp0(value, a) == 0 ||
           g_strcmp0(value, b) == 0 ||
           (c != NULL && g_strcmp0(value, c) == 0);
}

static gboolean parse_choice_arg(const char *name,
                                 const char *value,
                                 const char *a,
                                 const char *b,
                                 const char *c,
                                 char *out,
                                 gsize out_size,
                                 GError **error)
{
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

static void find_config_path(SeekeyConfig *config, int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--config") == 0 && i + 1 < argc) {
            g_strlcpy(config->config_path, argv[i + 1], sizeof(config->config_path));
            return;
        }
    }
}

static gboolean cli_has_flag(int argc, char **argv, const char *flag)
{
    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], flag) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void keyfile_get_uint_range(GKeyFile *key_file,
                                   const char *group,
                                   const char *key,
                                   guint min,
                                   guint max,
                                   guint *target,
                                   GError **error)
{
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

static void keyfile_get_string_value(GKeyFile *key_file,
                                     const char *group,
                                     const char *key,
                                     char *target,
                                     gsize target_size,
                                     GError **error)
{
    if (*error != NULL || !g_key_file_has_key(key_file, group, key, NULL)) {
        return;
    }

    char *value = g_key_file_get_string(key_file, group, key, error);
    if (*error == NULL) {
        g_strlcpy(target, value, target_size);
    }
    g_free(value);
}

static gboolean load_config_file(SeekeyConfig *config, GError **error)
{
    if (!g_file_test(config->config_path, G_FILE_TEST_EXISTS)) {
        return TRUE;
    }

    GKeyFile *key_file = g_key_file_new();
    if (!g_key_file_load_from_file(key_file, config->config_path, G_KEY_FILE_NONE, error)) {
        g_key_file_unref(key_file);
        return FALSE;
    }

    /* Apply theme first so individual [style] keys can override it. */
    {
        char theme_name[32];
        keyfile_get_string_value(key_file, "general", "theme", theme_name,
                                 sizeof(theme_name), error);
        if (*error == NULL) {
            apply_theme_preset(config, theme_name);
            g_strlcpy(config->theme, theme_name, sizeof(config->theme));
        } else {
            g_clear_error(error);
        }
    }

    {
        gboolean val;
        if (g_key_file_has_key(key_file, "general", "merge-repeats", NULL)) {
            val = g_key_file_get_boolean(key_file, "general", "merge-repeats", NULL);
            config->merge_repeats = val;
        }
        if (g_key_file_has_key(key_file, "general", "merge-modifiers", NULL)) {
            val = g_key_file_get_boolean(key_file, "general", "merge-modifiers", NULL);
            config->merge_modifiers = val;
        }
        if (g_key_file_has_key(key_file, "general", "show-mouse", NULL)) {
            val = g_key_file_get_boolean(key_file, "general", "show-mouse", NULL);
            config->show_mouse = val;
        }
    }

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
    keyfile_get_uint_range(key_file, "general", "window-width", 240, 3000,
                           &config->window_width, error);
    keyfile_get_uint_range(key_file, "general", "window-height", 80, 1000,
                           &config->window_height, error);
    keyfile_get_string_value(key_file, "general", "layer-shell", config->layer_shell,
                             sizeof(config->layer_shell), error);

    keyfile_get_uint_range(key_file, "style", "spacing", 0, 80,
                           &config->box_spacing, error);
    keyfile_get_uint_range(key_file, "style", "overlay-padding", 0, 80,
                           &config->overlay_padding, error);
    keyfile_get_uint_range(key_file, "style", "key-min-width", 1, 300,
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
    keyfile_get_uint_range(key_file, "style", "typing-max-width", 80, 2000,
                           &config->typing_max_width, error);

    keyfile_get_string_value(key_file, "style", "align", config->align,
                             sizeof(config->align), error);
    keyfile_get_string_value(key_file, "style", "disappear", config->disappear,
                             sizeof(config->disappear), error);
    keyfile_get_string_value(key_file, "style", "foreground", config->foreground,
                             sizeof(config->foreground), error);
    keyfile_get_string_value(key_file, "style", "background", config->background,
                             sizeof(config->background), error);
    keyfile_get_string_value(key_file, "style", "border-color", config->border_color,
                             sizeof(config->border_color), error);
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
        char **icon_keys = g_key_file_get_keys(key_file, "icons", &icon_count, NULL);
        for (gsize i = 0; i < icon_count && config->icon_override_count < SEEKEY_MAX_ICON_OVERRIDES; i++) {
            char *value = g_key_file_get_string(key_file, "icons", icon_keys[i], NULL);
            if (value != NULL) {
                g_strlcpy(config->icon_overrides[config->icon_override_count].name,
                          icon_keys[i], sizeof(config->icon_overrides[0].name));
                g_strlcpy(config->icon_overrides[config->icon_override_count].icon,
                          value, sizeof(config->icon_overrides[0].icon));
                config->icon_override_count++;
                g_free(value);
            }
        }
        g_strfreev(icon_keys);
    }

    g_key_file_unref(key_file);

    if (*error != NULL) {
        return FALSE;
    }
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

    return TRUE;
}

static void keyfile_set_config(GKeyFile *key_file, const SeekeyConfig *config)
{
    g_key_file_set_integer(key_file, "general", "duration-ms", (int)config->duration_ms);
    g_key_file_set_integer(key_file, "general", "typing-idle-ms", (int)config->typing_idle_ms);
    g_key_file_set_integer(key_file, "general", "fade-ms", (int)config->fade_ms);
    g_key_file_set_integer(key_file, "general", "margin", (int)config->margin_px);
    g_key_file_set_integer(key_file, "general", "margin-horizontal", (int)config->margin_horizontal_px);
    g_key_file_set_integer(key_file, "general", "max-items", (int)config->max_items);
    g_key_file_set_integer(key_file, "general", "window-width", (int)config->window_width);
    g_key_file_set_integer(key_file, "general", "window-height", (int)config->window_height);
    g_key_file_set_string(key_file, "general", "layer-shell", config->layer_shell);
    g_key_file_set_boolean(key_file, "general", "merge-repeats", config->merge_repeats);
    g_key_file_set_boolean(key_file, "general", "merge-modifiers", config->merge_modifiers);
    g_key_file_set_boolean(key_file, "general", "show-mouse", config->show_mouse);
    g_key_file_set_string(key_file, "general", "theme", config->theme);

    g_key_file_set_string(key_file, "style", "align", config->align);
    g_key_file_set_string(key_file, "style", "disappear", config->disappear);
    g_key_file_set_integer(key_file, "style", "spacing", (int)config->box_spacing);
    g_key_file_set_integer(key_file, "style", "overlay-padding", (int)config->overlay_padding);
    g_key_file_set_integer(key_file, "style", "key-min-width", (int)config->key_min_width);
    g_key_file_set_integer(key_file, "style", "key-padding-x", (int)config->key_padding_x);
    g_key_file_set_integer(key_file, "style", "key-padding-y", (int)config->key_padding_y);
    g_key_file_set_integer(key_file, "style", "key-radius", (int)config->key_radius);
    g_key_file_set_integer(key_file, "style", "key-border-width", (int)config->key_border_width);
    g_key_file_set_integer(key_file, "style", "key-font-px", (int)config->key_font_px);
    g_key_file_set_integer(key_file, "style", "key-font-weight", (int)config->key_font_weight);
    g_key_file_set_integer(key_file, "style", "typing-max-width", (int)config->typing_max_width);
    g_key_file_set_string(key_file, "style", "foreground", config->foreground);
    g_key_file_set_string(key_file, "style", "background", config->background);
    g_key_file_set_string(key_file, "style", "border-color", config->border_color);
    g_key_file_set_string(key_file, "style", "shadow", config->shadow);
    g_key_file_set_string(key_file, "style", "placeholder-text", config->placeholder_text);
    g_key_file_set_string(key_file,
                          "style",
                          "placeholder-foreground",
                          config->placeholder_foreground);
    g_key_file_set_string(key_file,
                          "style",
                          "placeholder-background",
                          config->placeholder_background);
    g_key_file_set_string(key_file,
                          "style",
                          "placeholder-border-color",
                          config->placeholder_border_color);

    /* --- [icons] section --- */
    for (guint i = 0; i < config->icon_override_count; i++) {
        g_key_file_set_string(key_file, "icons",
                              config->icon_overrides[i].name,
                              config->icon_overrides[i].icon);
    }
}

static gboolean save_config_file(const SeekeyConfig *config, GError **error)
{
    GKeyFile *key_file = g_key_file_new();
    keyfile_set_config(key_file, config);

    gsize length = 0;
    char *data = g_key_file_to_data(key_file, &length, error);
    g_key_file_unref(key_file);
    if (data == NULL) {
        return FALSE;
    }

    char *dir = g_path_get_dirname(config->config_path);
    if (g_mkdir_with_parents(dir, 0755) != 0) {
        g_set_error(error,
                    G_FILE_ERROR,
                    g_file_error_from_errno(errno),
                    "Failed to create %s: %s",
                    dir,
                    g_strerror(errno));
        g_free(dir);
        g_free(data);
        return FALSE;
    }
    g_free(dir);

    gboolean ok = g_file_set_contents(config->config_path, data, (gssize)length, error);
    g_free(data);
    return ok;
}

static gboolean print_config_file(const SeekeyConfig *config, GError **error)
{
    GKeyFile *key_file = g_key_file_new();
    keyfile_set_config(key_file, config);

    gsize length = 0;
    char *data = g_key_file_to_data(key_file, &length, error);
    g_key_file_unref(key_file);
    if (data == NULL) {
        return FALSE;
    }

    g_print("# path: %s\n%s", config->config_path, data);
    g_free(data);
    return TRUE;
}

static gboolean init_config_file(const SeekeyConfig *config, GError **error)
{
    if (g_file_test(config->config_path, G_FILE_TEST_EXISTS) && !config->force) {
        g_set_error(error,
                    G_IO_ERROR,
                    G_IO_ERROR_EXISTS,
                    "Config already exists at %s; use --force to overwrite",
                    config->config_path);
        return FALSE;
    }

    return save_config_file(config, error);
}

/* ==================================================================
 * TUI color palette — 24 preset colours rendered with ncurses pairs.
 * ================================================================== */

typedef struct {
    const char *hex;
    short r, g, b;   /* 0–255 */
} TuiPaletteEntry;

static const TuiPaletteEntry TUI_PALETTE[] = {
    {"#ffffff", 255, 255, 255}, {"#cccccc", 204, 204, 204},
    {"#999999", 153, 153, 153}, {"#666666", 102, 102, 102},
    {"#333333",  51,  51,  51}, {"#111318",  17,  19,  24},
    {"#f7f7f2", 247, 247, 242}, {"#1a1a2e",  26,  26,  46},
    {"#ff5555", 255,  85,  85}, {"#ee5a24", 238,  90,  36},
    {"#f0932b", 240, 147,  43}, {"#f9ca24", 249, 202,  36},
    {"#a6e22e", 166, 226,  46}, {"#2ecc71",  46, 204, 113},
    {"#1abc9c",  26, 188, 156}, {"#74b9ff", 116, 185, 255},
    {"#0984e3",   9, 132, 227}, {"#5e81ac",  94, 129, 172},
    {"#a29bfe", 162, 155, 254}, {"#bd93f9", 189, 147, 249},
    {"#cba6f7", 203, 166, 247}, {"#d8dee9", 216, 222, 233},
    {"#88c0d0", 136, 192, 208}, {"#cdd6f4", 205, 214, 244},
};

#define TUI_PALETTE_COLS  6
#define TUI_PALETTE_ROWS  ((int)G_N_ELEMENTS(TUI_PALETTE) / TUI_PALETTE_COLS)
#define TUI_COLOR_PAIR_BASE  32

static void tui_init_colors(void)
{
    if (!has_colors() || !can_change_color()) return;
    start_color();
    for (gsize i = 0; i < G_N_ELEMENTS(TUI_PALETTE); i++) {
        short r = (short)(TUI_PALETTE[i].r * 1000 / 255);
        short g = (short)(TUI_PALETTE[i].g * 1000 / 255);
        short b = (short)(TUI_PALETTE[i].b * 1000 / 255);
        init_color((short)(TUI_COLOR_PAIR_BASE + i), r, g, b);
        init_pair((short)(TUI_COLOR_PAIR_BASE + i),
                  (short)(TUI_COLOR_PAIR_BASE + i),
                  (short)(TUI_COLOR_PAIR_BASE + i));
    }
}

static int tui_nearest_color_index(const char *hex)
{
    /* Quick scan for an exact palette match, then fall back to nearest. */
    for (gsize i = 0; i < G_N_ELEMENTS(TUI_PALETTE); i++) {
        if (g_ascii_strcasecmp(hex, TUI_PALETTE[i].hex) == 0) return (int)i;
    }
    /* Parse #rrggbb and find closest by Euclidean distance. */
    guint rr = 128, gg = 128, bb = 128;
    if (hex[0] == '#' && strlen(hex) == 7) {
        rr = (guint)g_ascii_strtoull(hex + 1, NULL, 16) >> 16 & 0xff;
        gg = (guint)g_ascii_strtoull(hex + 1, NULL, 16) >> 8  & 0xff;
        bb = (guint)g_ascii_strtoull(hex + 1, NULL, 16)       & 0xff;
    }
    int best = 0, best_dist = 0x7fffffff;
    for (gsize i = 0; i < G_N_ELEMENTS(TUI_PALETTE); i++) {
        int dr = (int)rr - (int)TUI_PALETTE[i].r;
        int dg = (int)gg - (int)TUI_PALETTE[i].g;
        int db = (int)bb - (int)TUI_PALETTE[i].b;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) { best_dist = dist; best = (int)i; }
    }
    return best;
}

static void tui_draw_color_swatch(int y, int x, const char *hex, int w)
{
    int idx = tui_nearest_color_index(hex);
    if (has_colors()) {
        attron(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
        for (int i = 0; i < w; i++) mvprintw(y, x + i, " ");
        attroff(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
    } else {
        mvprintw(y, x, "[#]");
    }
}

/* Returns TRUE and writes hex into out if the user picked a color. */
static gboolean tui_color_picker(const char *label, char *out, gsize out_size)
{
    int cur_idx = tui_nearest_color_index(out);
    int sel_row = cur_idx / TUI_PALETTE_COLS;
    int sel_col = cur_idx % TUI_PALETTE_COLS;
    gboolean done = FALSE, picked = FALSE;

    while (!done) {
        erase();
        mvprintw(0, 2, "Pick color for: %s", label);
        mvprintw(1, 2, "Arrows: navigate  Enter: pick  c: custom hex  Esc: cancel");

        for (int row = 0; row < TUI_PALETTE_ROWS; row++) {
            for (int col = 0; col < TUI_PALETTE_COLS; col++) {
                int idx = row * TUI_PALETTE_COLS + col;
                int x = 4 + col * 6;
                int y = 4 + row * 2;
                if (has_colors()) {
                    attron(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
                    mvprintw(y, x, "    ");
                    attroff(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
                }
                if (row == sel_row && col == sel_col) {
                    mvprintw(y, x - 2, "[");
                    mvprintw(y, x + 4, "]");
                }
                mvprintw(y + 1, x, "%-4s", TUI_PALETTE[idx].hex + 4);
            }
        }

        int info_y = 4 + TUI_PALETTE_ROWS * 2 + 1;
        int cur = sel_row * TUI_PALETTE_COLS + sel_col;
        mvprintw(info_y, 2, "Selected: ");
        if (has_colors()) {
            attron(COLOR_PAIR(TUI_COLOR_PAIR_BASE + cur));
            mvprintw(info_y, 12, "    ");
            attroff(COLOR_PAIR(TUI_COLOR_PAIR_BASE + cur));
        }
        mvprintw(info_y, 17, " %s", TUI_PALETTE[cur].hex);

        int ch = getch();
        switch (ch) {
        case KEY_UP:    case 'k': sel_row = (sel_row - 1 + TUI_PALETTE_ROWS) % TUI_PALETTE_ROWS; break;
        case KEY_DOWN:  case 'j': sel_row = (sel_row + 1) % TUI_PALETTE_ROWS; break;
        case KEY_LEFT:  case 'h': sel_col = (sel_col - 1 + TUI_PALETTE_COLS) % TUI_PALETTE_COLS; break;
        case KEY_RIGHT: case 'l': sel_col = (sel_col + 1) % TUI_PALETTE_COLS; break;
        case 'c': case 'C': {
            echo(); curs_set(1);
            char buf[16] = {0};
            move(info_y + 2, 2); clrtoeol();
            mvprintw(info_y + 2, 2, "Enter hex (#rrggbb): ");
            getnstr(buf, (int)sizeof(buf) - 1);
            noecho(); curs_set(0);
            if (buf[0] == '#' && strlen(buf) == 7) {
                g_strlcpy(out, buf, out_size);
                picked = TRUE; done = TRUE;
            }
            break;
        }
        case '\n': case '\r': case KEY_ENTER:
            g_strlcpy(out, TUI_PALETTE[cur].hex, out_size);
            picked = TRUE; done = TRUE;
            break;
        case 27: case 'q':
            done = TRUE; break;
        }
    }
    return picked;
}

/* Returns TRUE if the user selected a new theme. */
static gboolean tui_theme_picker(const char *current, char *out, gsize out_size)
{
    static const char *THEME_NAMES[] = {"default", "nord", "dracula", "catppuccin", "monokai", "light"};
    static const char *THEME_FG[]   = {"#f7f7f2","#d8dee9","#f8f8f2","#cdd6f4","#f8f8f2","#1a1a2e"};
    static const char *THEME_BG[]   = {"#111318","#2e3440","#282a36","#1e1e2e","#272822","#fafafa"};
    static const char *THEME_BD[]   = {"#ffffff","#88c0d0","#bd93f9","#cba6f7","#a6e22e","#cccccc"};

    int n = (int)G_N_ELEMENTS(THEME_NAMES);
    int sel = 0;
    for (int i = 0; i < n; i++) {
        if (g_strcmp0(current, THEME_NAMES[i]) == 0) { sel = i; break; }
    }
    gboolean done = FALSE, picked = FALSE;

    while (!done) {
        erase();
        mvprintw(0, 2, "Pick a theme  (arrows, enter, esc)");
        for (int i = 0; i < n; i++) {
            int y = 3 + i * 3;
            if (i == sel) { attron(A_REVERSE); }
            mvprintw(y, 2, " %c %-16s", (i == sel) ? '>' : ' ', THEME_NAMES[i]);
            if (i == sel) { attroff(A_REVERSE); }
            tui_draw_color_swatch(y + 1,  5, THEME_FG[i], 4);
            tui_draw_color_swatch(y + 1, 10, THEME_BG[i], 4);
            tui_draw_color_swatch(y + 1, 15, THEME_BD[i], 4);
            mvprintw(y + 1, 18, "fg / bg / border");
        }
        int ch = getch();
        switch (ch) {
        case KEY_UP:   case 'k': sel = (sel - 1 + n) % n; break;
        case KEY_DOWN: case 'j': sel = (sel + 1) % n; break;
        case '\n': case '\r': case KEY_ENTER:
            g_strlcpy(out, THEME_NAMES[sel], out_size);
            picked = TRUE; done = TRUE;
            break;
        case 27: case 'q': done = TRUE; break;
        }
    }
    return picked;
}

/* ------------------------------------------------------------------ */

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
    TuiFieldType type;
    guint *uint_target;
    guint min;
    guint max;
    guint step;
    char *string_target;
    gsize string_size;
    const char **choices;
    guint choice_count;
    gboolean *bool_target;
} TuiField;

static const char *ALIGN_CHOICES[] = {"left", "center", "right"};
static const char *DISAPPEAR_CHOICES[] = {"instant", "fade"};

static guint current_choice_index(const TuiField *field)
{
    for (guint i = 0; i < field->choice_count; i++) {
        if (g_strcmp0(field->string_target, field->choices[i]) == 0) {
            return i;
        }
    }
    return 0;
}

static void tui_field_value(const TuiField *field, char *buffer, gsize size)
{
    switch (field->type) {
    case TUI_UINT:
        g_snprintf(buffer, size, "%u", *field->uint_target);
        break;
    case TUI_BOOL:
        g_strlcpy(buffer, *field->bool_target ? "yes" : "no", size);
        break;
    case TUI_COLOR:
    case TUI_STRING:
    case TUI_CHOICE:
        g_strlcpy(buffer, field->string_target, size);
        break;
    }
}

static void tui_adjust_field(TuiField *field, int direction)
{
    if (field->type == TUI_UINT) {
        guint value = *field->uint_target;
        guint step = field->step > 0 ? field->step : 1;
        if (direction < 0) {
            value = value > field->min + step ? value - step : field->min;
        } else {
            value = value + step < field->max ? value + step : field->max;
        }
        *field->uint_target = value;
        return;
    }

    if (field->type == TUI_BOOL) {
        *field->bool_target = !*field->bool_target;
        return;
    }

    if (field->type == TUI_CHOICE) {
        guint index = current_choice_index(field);
        if (direction < 0) {
            index = index == 0 ? field->choice_count - 1 : index - 1;
        } else {
            index = (index + 1) % field->choice_count;
        }
        g_strlcpy(field->string_target, field->choices[index], field->string_size);
    }

    if (field->type == TUI_COLOR) {
        int idx = tui_nearest_color_index(field->string_target);
        int count = (int)G_N_ELEMENTS(TUI_PALETTE);
        idx = (idx + direction + count) % count;
        g_strlcpy(field->string_target, TUI_PALETTE[idx].hex, field->string_size);
    }
}

static void tui_edit_field(TuiField *field)
{
    char value[512];
    tui_field_value(field, value, sizeof(value));

    echo();
    curs_set(1);
    move(LINES - 3, 0);
    clrtoeol();
    mvprintw(LINES - 3, 0, "New value for %s: ", field->label);
    getnstr(value, (int)sizeof(value) - 1);
    noecho();
    curs_set(0);

    if (field->type == TUI_UINT) {
        char *end = NULL;
        unsigned long parsed = strtoul(value, &end, 10);
        if (*value != '\0' && *end == '\0' &&
            parsed >= field->min && parsed <= field->max) {
            *field->uint_target = (guint)parsed;
        }
        return;
    }

    if (field->type == TUI_CHOICE) {
        for (guint i = 0; i < field->choice_count; i++) {
            if (g_strcmp0(value, field->choices[i]) == 0) {
                g_strlcpy(field->string_target, value, field->string_size);
                return;
            }
        }
        return;
    }

    if (field->type == TUI_COLOR) {
        if (value[0] == '#' || value[0] != '\0') {
            g_strlcpy(field->string_target, value, field->string_size);
        }
        return;
    }

    if (*value != '\0') {
        g_strlcpy(field->string_target, value, field->string_size);
    }
}

static void tui_draw_preview(const SeekeyConfig *config)
{
    int top = LINES - 8;
    if (top < 10) {
        return;
    }

    char sample[160];
    g_snprintf(sample,
               sizeof(sample),
               "[ %s ]  [ Ctrl + C x3 ]",
               config->placeholder_text);

    int x = 2;
    int width = (int)strlen(sample);
    if (g_strcmp0(config->align, "center") == 0) {
        x = MAX(2, (COLS - width) / 2);
    } else if (g_strcmp0(config->align, "right") == 0) {
        x = MAX(2, COLS - width - 3);
    }

    mvhline(top, 0, ACS_HLINE, COLS);
    mvprintw(top + 1, 2, "Preview");
    attron(A_BOLD);
    mvprintw(top + 3, x, "%s", sample);
    attroff(A_BOLD);
    mvprintw(top + 5,
             2,
             "Disappear: %s, duration %ums, fade %ums",
             config->disappear,
             config->duration_ms,
             config->fade_ms);
}

static gboolean run_config_tui(SeekeyConfig *config, GError **error)
{
    TuiField fields[] = {
        {"duration-ms", "How long key bubbles stay visible.", TUI_UINT,
         &config->duration_ms, 100, 10000, 100, NULL, 0, NULL, 0},
        {"typing-idle-ms", "Pause before typing starts a new bubble.", TUI_UINT,
         &config->typing_idle_ms, 100, 5000, 50, NULL, 0, NULL, 0},
        {"fade-ms", "Fade animation length. 0 disables fade delay.", TUI_UINT,
         &config->fade_ms, 0, 3000, 50, NULL, 0, NULL, 0},
        {"margin", "Bottom layer-shell margin.", TUI_UINT,
         &config->margin_px, 0, 1000, 8, NULL, 0, NULL, 0},
        {"margin-horizontal", "Horizontal margin for left/right anchor.", TUI_UINT,
         &config->margin_horizontal_px, 0, 1000, 8, NULL, 0, NULL, 0},
        {"max-items", "Maximum bubbles visible at once.", TUI_UINT,
         &config->max_items, 1, 20, 1, NULL, 0, NULL, 0},
        {"window-width", "Fallback window width.", TUI_UINT,
         &config->window_width, 240, 3000, 20, NULL, 0, NULL, 0},
        {"window-height", "Fallback window height.", TUI_UINT,
         &config->window_height, 80, 1000, 10, NULL, 0, NULL, 0},
        {"layer-shell", "auto falls back to a window; required stays across workspaces or exits.", TUI_CHOICE,
         NULL, 0, 0, 0, config->layer_shell, sizeof(config->layer_shell),
         (const char *[]){"auto", "required", "off"}, 3},
        {"theme", "Color preset (overridable per key in [style]).", TUI_CHOICE,
         NULL, 0, 0, 0, config->theme, sizeof(config->theme),
         (const char *[]){"default", "nord", "dracula", "catppuccin", "monokai", "light"}, 6},
        {"merge-repeats", "Show repeated keys as Key xN.", TUI_BOOL,
         NULL, 0, 0, 0, NULL, 0, NULL, 0, &config->merge_repeats},
        {"merge-modifiers", "Update modifier bubble when combo extends.", TUI_BOOL,
         NULL, 0, 0, 0, NULL, 0, NULL, 0, &config->merge_modifiers},
        {"show-mouse", "Show mouse button clicks as bubbles.", TUI_BOOL,
         NULL, 0, 0, 0, NULL, 0, NULL, 0, &config->show_mouse},
        {"align", "Bubble row alignment.", TUI_CHOICE,
         NULL, 0, 0, 0, config->align, sizeof(config->align),
         ALIGN_CHOICES, G_N_ELEMENTS(ALIGN_CHOICES)},
        {"disappear", "Bubble removal mode.", TUI_CHOICE,
         NULL, 0, 0, 0, config->disappear, sizeof(config->disappear),
         DISAPPEAR_CHOICES, G_N_ELEMENTS(DISAPPEAR_CHOICES)},
        {"spacing", "Space between bubbles.", TUI_UINT,
         &config->box_spacing, 0, 80, 1, NULL, 0, NULL, 0},
        {"overlay-padding", "Outer padding around the bubble row.", TUI_UINT,
         &config->overlay_padding, 0, 80, 1, NULL, 0, NULL, 0},
        {"key-padding-x", "Horizontal key padding.", TUI_UINT,
         &config->key_padding_x, 0, 80, 1, NULL, 0, NULL, 0},
        {"key-padding-y", "Vertical key padding.", TUI_UINT,
         &config->key_padding_y, 0, 80, 1, NULL, 0, NULL, 0},
        {"key-radius", "Bubble corner radius.", TUI_UINT,
         &config->key_radius, 0, 80, 1, NULL, 0, NULL, 0},
        {"key-border-width", "Bubble border width.", TUI_UINT,
         &config->key_border_width, 0, 20, 1, NULL, 0, NULL, 0},
        {"key-font-px", "Bubble font size.", TUI_UINT,
         &config->key_font_px, 8, 96, 1, NULL, 0, NULL, 0},
        {"key-font-weight", "Bubble font weight.", TUI_UINT,
         &config->key_font_weight, 100, 1000, 100, NULL, 0, NULL, 0},
        {"typing-max-width", "Maximum width for grouped typing bubbles.", TUI_UINT,
         &config->typing_max_width, 80, 2000, 20, NULL, 0, NULL, 0},
        {"foreground", "GTK CSS text color. Enter=picker, ←→=cycle, c=custom.", TUI_COLOR,
         NULL, 0, 0, 0, config->foreground, sizeof(config->foreground), NULL, 0},
        {"background", "GTK CSS background. Enter=picker, ←→=cycle, c=custom.", TUI_COLOR,
         NULL, 0, 0, 0, config->background, sizeof(config->background), NULL, 0},
        {"border-color", "GTK CSS border color. Enter=picker, ←→=cycle, c=custom.", TUI_COLOR,
         NULL, 0, 0, 0, config->border_color, sizeof(config->border_color), NULL, 0},
        {"shadow", "GTK CSS box-shadow.", TUI_STRING,
         NULL, 0, 0, 0, config->shadow, sizeof(config->shadow), NULL, 0},
        {"placeholder-text", "Startup placeholder text.", TUI_STRING,
         NULL, 0, 0, 0, config->placeholder_text, sizeof(config->placeholder_text), NULL, 0},
        {"placeholder-foreground", "Placeholder text color. Enter=picker, ←→=cycle.", TUI_COLOR,
         NULL, 0, 0, 0, config->placeholder_foreground,
         sizeof(config->placeholder_foreground), NULL, 0},
        {"placeholder-background", "Placeholder background. Enter=picker, ←→=cycle.", TUI_COLOR,
         NULL, 0, 0, 0, config->placeholder_background,
         sizeof(config->placeholder_background), NULL, 0},
        {"placeholder-border-color", "Placeholder border color. Enter=picker, ←→=cycle.", TUI_COLOR,
         NULL, 0, 0, 0, config->placeholder_border_color,
         sizeof(config->placeholder_border_color), NULL, 0},
    };

    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    tui_init_colors();

    int selected = 0;
    int scroll = 0;
    gboolean saved = FALSE;
    gboolean running = TRUE;

    while (running) {
        erase();
        mvprintw(0, 2, "Seekey config TUI");
        mvprintw(1, 2, "Config: %s", config->config_path);
        mvprintw(2, 2, "Keys: Up/Down select, Left/Right adjust, Enter edit, s save, q quit");

        int list_top = 4;
        int list_bottom = LINES - 10;
        int visible = MAX(1, list_bottom - list_top);
        if (selected < scroll) {
            scroll = selected;
        } else if (selected >= scroll + visible) {
            scroll = selected - visible + 1;
        }

        for (int row = 0; row < visible; row++) {
            int index = scroll + row;
            if (index >= (int)G_N_ELEMENTS(fields)) {
                break;
            }

            char value[512];
            tui_field_value(&fields[index], value, sizeof(value));
            if (index == selected) {
                attron(A_REVERSE);
                mvprintw(list_top + row, 2, "%-28s %s", fields[index].label, value);
                attroff(A_REVERSE);
            } else {
                mvprintw(list_top + row, 2, "%-28s %s", fields[index].label, value);
            }
            if (fields[index].type == TUI_COLOR) {
                tui_draw_color_swatch(list_top + row, 56, value, 4);
            }
        }

        mvprintw(LINES - 9, 2, "%s", fields[selected].help);
        if (saved) {
            mvprintw(LINES - 9, COLS - 24, "Saved");
        }
        tui_draw_preview(config);
        refresh();

        int ch = getch();
        saved = FALSE;
        switch (ch) {
        case KEY_UP:
        case 'k':
            selected = selected > 0 ? selected - 1 : (int)G_N_ELEMENTS(fields) - 1;
            break;
        case KEY_DOWN:
        case 'j':
            selected = selected + 1 < (int)G_N_ELEMENTS(fields) ? selected + 1 : 0;
            break;
        case KEY_LEFT:
        case 'h':
            tui_adjust_field(&fields[selected], -1);
            if (g_strcmp0(fields[selected].label, "theme") == 0) {
                apply_theme_preset(config, config->theme);
            }
            break;
        case KEY_RIGHT:
        case 'l':
            tui_adjust_field(&fields[selected], 1);
            if (g_strcmp0(fields[selected].label, "theme") == 0) {
                apply_theme_preset(config, config->theme);
            }
            break;
        case '\n':
        case '\r':
        case KEY_ENTER:
            if (fields[selected].type == TUI_COLOR) {
                char new_color[64];
                g_strlcpy(new_color, fields[selected].string_target, sizeof(new_color));
                if (tui_color_picker(fields[selected].label, new_color, sizeof(new_color))) {
                    g_strlcpy(fields[selected].string_target, new_color,
                              fields[selected].string_size);
                }
            } else if (g_strcmp0(fields[selected].label, "theme") == 0) {
                char new_theme[32];
                g_strlcpy(new_theme, config->theme, sizeof(new_theme));
                if (tui_theme_picker(config->theme, new_theme, sizeof(new_theme))) {
                    g_strlcpy(config->theme, new_theme, sizeof(config->theme));
                    apply_theme_preset(config, config->theme);
                }
            } else {
                tui_edit_field(&fields[selected]);
            }
            break;
        case 's':
        case 'S':
            saved = save_config_file(config, error);
            if (!saved) {
                running = FALSE;
            }
            break;
        case 'q':
        case 'Q':
        case 27:
            running = FALSE;
            break;
        default:
            break;
        }
    }

    endwin();
    return *error == NULL;
}

static gboolean parse_args(SeekeyConfig *config, int *argc, char ***argv, GError **error)
{
    for (int i = 1; i < *argc; i++) {
        const char *arg = (*argv)[i];
        if (g_strcmp0(arg, "--no-layer-shell") == 0) {
            config->no_layer_shell = TRUE;
            g_strlcpy(config->layer_shell, "off", sizeof(config->layer_shell));
        } else if (g_strcmp0(arg, "--debug-input") == 0) {
            config->debug_input = TRUE;
        } else if (g_strcmp0(arg, "--config-tui") == 0) {
            config->config_tui = TRUE;
        } else if (g_strcmp0(arg, "--init-config") == 0) {
            config->init_config = TRUE;
        } else if (g_strcmp0(arg, "--force") == 0) {
            config->force = TRUE;
        } else if (g_strcmp0(arg, "--print-config") == 0) {
            config->print_config = TRUE;
        } else if (g_strcmp0(arg, "--validate-config") == 0) {
            config->validate_config = TRUE;
        } else if (g_strcmp0(arg, "--config") == 0) {
            if (++i >= *argc) {
                g_set_error_literal(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                    "--config requires a path");
                return FALSE;
            }
            g_strlcpy(config->config_path, (*argv)[i], sizeof(config->config_path));
        } else if (g_strcmp0(arg, "--duration") == 0) {
            if (++i >= *argc ||
                !parse_uint_arg("--duration", (*argv)[i], 100, 10000,
                                &config->duration_ms, error)) {
                return FALSE;
            }
        } else if (g_strcmp0(arg, "--typing-idle") == 0) {
            if (++i >= *argc ||
                !parse_uint_arg("--typing-idle", (*argv)[i], 100, 5000,
                                &config->typing_idle_ms, error)) {
                return FALSE;
            }
        } else if (g_strcmp0(arg, "--fade-ms") == 0) {
            if (++i >= *argc ||
                !parse_uint_arg("--fade-ms", (*argv)[i], 0, 3000,
                                &config->fade_ms, error)) {
                return FALSE;
            }
        } else if (g_strcmp0(arg, "--margin") == 0) {
            if (++i >= *argc ||
                !parse_uint_arg("--margin", (*argv)[i], 0, 1000,
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
            if (++i >= *argc ||
                !parse_uint_arg("--max-items", (*argv)[i], 1, 20,
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
                                  config->disappear, sizeof(config->disappear), error)) {
                return FALSE;
            }
        } else if (g_strcmp0(arg, "--layer-shell") == 0) {
            if (++i >= *argc ||
                !parse_choice_arg("--layer-shell", (*argv)[i], "auto", "required", "off",
                                  config->layer_shell, sizeof(config->layer_shell), error)) {
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
            apply_theme_preset(config, config->theme);
        } else if (g_strcmp0(arg, "-V") == 0 || g_strcmp0(arg, "--version") == 0) {
            g_print("seekey %s\n", SEEKEY_VERSION);
            exit(0);
        } else if (g_strcmp0(arg, "-h") == 0 || g_strcmp0(arg, "--help") == 0) {
            g_print("Usage: seekey [OPTIONS]\n\n"
                    "Options:\n"
                    "  --config PATH          Config file path, default ~/.config/seekey/config.ini\n"
                    "  --config-tui           Open terminal UI to edit and save configuration\n"
                    "  --init-config          Write the current default/config/CLI settings to config\n"
                    "  --force                Allow --init-config to overwrite an existing config\n"
                    "  --print-config         Print the effective configuration and exit\n"
                    "  --validate-config      Validate configuration and exit\n"
                    "  --no-layer-shell       Disable gtk4-layer-shell even if available\n"
                    "  --layer-shell auto|required|off\n"
                    "  --theme NAME           Color preset: default, nord, dracula, catppuccin, monokai, light\n"
                    "  --merge-repeats        Stack identical key bubbles as Key xN (default on)\n"
                    "  --no-merge-repeats     Show each key press as a separate bubble\n"
                    "  --merge-modifiers      Update modifier bubble when combo extends (default on)\n"
                    "  --no-merge-modifiers   Keep each modifier press as a separate bubble\n"
                    "  --show-mouse           Show mouse clicks (default on)\n"
                    "  --no-mouse             Hide mouse clicks\n"
                    "  --duration MS          Bubble duration, default 1200\n"
                    "  --typing-idle MS       Pause before typed text starts a new bubble, default 650\n"
                    "  --fade-ms MS           Fade duration when disappear=fade, default 180\n"
                    "  --margin PX            Bottom margin in layer-shell mode, default 96\n"
                    "  --margin-horizontal PX Horizontal margin for left/right anchor, default 0\n"
                    "  --max-items N          Maximum visible bubbles, default 5\n"
                    "  --align left|center|right\n"
                    "  --disappear instant|fade\n"
                    "  --debug-input          Print input discovery and events\n"
                    "  -V, --version          Print version and exit\n"
                    "  -h, --help             Show help\n");
            exit(0);
        } else {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                        "Unknown option: %s", arg);
            return FALSE;
        }
    }

    return TRUE;
}

/* Try to identify the running Wayland compositor so we can give helpful
 * hints and (in the future) adapt behaviour. */
static void detect_compositor(void)
{
    const char *desktop = g_getenv("XDG_CURRENT_DESKTOP");
    const char *wayland = g_getenv("WAYLAND_DISPLAY");

    if (wayland == NULL) {
        g_print("seekey: compositor unknown (no WAYLAND_DISPLAY)\n");
        return;
    }

    const char *name = desktop ? desktop : "unknown";
    g_print("seekey: compositor %s\n", name);

    /* Compositor-specific hints (informational only for now). */
    struct { const char *id; const char *hint; } hints[] = {
        {"niri", "  hint: niri treats layer-shell outputs as the full workspace column."
                 "  Use a fallback window + niri window-rules for fixed positioning."},
        {"GNOME", "  hint: GNOME does not support wlr-layer-shell."
                  "  Use layer-shell=off and a GNOME extension for always-on-top."},
        {"sway",  "  hint: Sway supports wlr-layer-shell; layer-shell=auto works well."},
        {"Hyprland", "  hint: Hyprland supports wlr-layer-shell; layer-shell=auto works well."},
    };

    for (gsize i = 0; i < G_N_ELEMENTS(hints); i++) {
        if (g_strstr_len(desktop, -1, hints[i].id) != NULL) {
            g_print("%s\n", hints[i].hint);
            break;
        }
    }
}

static void activate(GtkApplication *app, gpointer user_data)
{
    AppState *state = user_data;
    state->app = app;

    detect_compositor();

    install_css(&state->config);

    GtkWidget *window = gtk_application_window_new(app);
    state->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Seekey");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(window),
                                (int)state->config.window_width,
                                (int)state->config.window_height);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,
                                 (int)state->config.box_spacing);
    state->box = box;
    gtk_widget_set_halign(box, configured_halign(&state->config));
    gtk_widget_set_valign(box, GTK_ALIGN_END);
    gtk_widget_add_css_class(box, "overlay-root");
    gtk_window_set_child(GTK_WINDOW(window), box);

    state->placeholder = gtk_label_new(state->config.placeholder_text);
    gtk_label_set_single_line_mode(GTK_LABEL(state->placeholder), TRUE);
    gtk_widget_add_css_class(state->placeholder, "key-bubble");
    gtk_widget_add_css_class(state->placeholder, "placeholder-bubble");
    gtk_box_append(GTK_BOX(state->box), state->placeholder);

    GError *layer_error = NULL;
    if (!seekey_layer_shell_try_init(GTK_WINDOW(window), &state->config, &layer_error)) {
        if (g_strcmp0(state->config.layer_shell, "required") == 0) {
            g_printerr("seekey: layer-shell required but unavailable: %s\n",
                       layer_error->message);
            g_printerr("seekey: install gtk4-layer-shell and use a compositor that supports wlr-layer-shell.\n");
            g_clear_error(&layer_error);
            exit(2);
        }
        g_printerr("seekey: using fallback window: %s\n", layer_error->message);
        g_clear_error(&layer_error);
    }

    GError *input_error = NULL;
    state->input = seekey_input_new(&state->config, on_key_event, state, &input_error);
    if (state->input == NULL) {
        g_printerr("seekey: %s\n", input_error->message);
        g_printerr("seekey: grant read access to /dev/input/event* or run a quick test as root.\n");
        g_clear_error(&input_error);
    } else {
        seekey_input_start(state->input);
    }

    gtk_window_present(GTK_WINDOW(window));
}

static void shutdown_app(GApplication *app, gpointer user_data)
{
    AppState *state = user_data;
    cancel_active_typing_remove_timeout(state);
    cancel_active_typing_idle_timeout(state);
    clear_typing_group(state);
    seekey_input_free(state->input);
    state->input = NULL;
}

int main(int argc, char **argv)
{
    AppState state = {0};
    config_set_defaults(&state.config);
    find_config_path(&state.config, argc, argv);

    GError *error = NULL;
    gboolean initializing = cli_has_flag(argc, argv, "--init-config");
    if (!initializing && !load_config_file(&state.config, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return 2;
    }

    if (!parse_args(&state.config, &argc, &argv, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return 2;
    }

    if (state.config.init_config) {
        if (!init_config_file(&state.config, &error)) {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
            return 2;
        }
        g_print("Wrote config to %s\n", state.config.config_path);
        return 0;
    }

    if (state.config.print_config) {
        if (!print_config_file(&state.config, &error)) {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
            return 2;
        }
        return 0;
    }

    if (state.config.validate_config) {
        g_print("Config OK: %s\n", state.config.config_path);
        return 0;
    }

    if (state.config.config_tui) {
        if (!run_config_tui(&state.config, &error)) {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
            return 2;
        }
        return 0;
    }

    GtkApplication *app = gtk_application_new("dev.seekey.Seekey",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
    g_signal_connect(app, "shutdown", G_CALLBACK(shutdown_app), &state);

    int app_argc = 1;
    int status = g_application_run(G_APPLICATION(app), app_argc, argv);
    g_object_unref(app);
    return status;
}
