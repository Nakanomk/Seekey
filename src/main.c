#include "seekey.h"

#include <stdlib.h>

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

static void remember_last_bubble(AppState *state, GtkWidget *widget, const char *combo)
{
    state->last_bubble = widget;
    state->last_bubble_id = ++state->next_id;
    state->last_bubble_count = 1;
    state->last_bubble_fading = FALSE;
    g_strlcpy(state->last_combo, combo, sizeof(state->last_combo));
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

    if (try_merge_last_bubble(state, event->combo)) {
        return;
    }

    GtkWidget *label = gtk_label_new(event->combo);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(label, "key-bubble");
    gtk_box_append(GTK_BOX(state->box), label);
    trim_bubbles(state);

    remember_last_bubble(state, label, event->combo);
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
        "  min-width: %upx;"
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
        "  min-width: %upx;"
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
        config->key_min_width,
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
        config->key_min_width,
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

static void config_set_defaults(SeekeyConfig *config)
{
    config->duration_ms = 1200;
    config->typing_idle_ms = 650;
    config->fade_ms = 180;
    config->margin_px = 96;
    config->max_items = 5;
    config->window_width = 720;
    config->window_height = 160;
    config->box_spacing = 7;
    config->overlay_padding = 12;
    config->key_min_width = 58;
    config->key_padding_x = 14;
    config->key_padding_y = 8;
    config->key_radius = 6;
    config->key_border_width = 1;
    config->key_font_px = 20;
    config->key_font_weight = 700;
    config->typing_max_width = 480;
    g_strlcpy(config->align, "right", sizeof(config->align));
    g_strlcpy(config->disappear, "fade", sizeof(config->disappear));
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

    keyfile_get_uint_range(key_file, "general", "duration-ms", 100, 10000,
                           &config->duration_ms, error);
    keyfile_get_uint_range(key_file, "general", "typing-idle-ms", 100, 5000,
                           &config->typing_idle_ms, error);
    keyfile_get_uint_range(key_file, "general", "fade-ms", 0, 3000,
                           &config->fade_ms, error);
    keyfile_get_uint_range(key_file, "general", "margin", 0, 1000,
                           &config->margin_px, error);
    keyfile_get_uint_range(key_file, "general", "max-items", 1, 20,
                           &config->max_items, error);
    keyfile_get_uint_range(key_file, "general", "window-width", 240, 3000,
                           &config->window_width, error);
    keyfile_get_uint_range(key_file, "general", "window-height", 80, 1000,
                           &config->window_height, error);

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

    return TRUE;
}

static gboolean parse_args(SeekeyConfig *config, int *argc, char ***argv, GError **error)
{
    for (int i = 1; i < *argc; i++) {
        const char *arg = (*argv)[i];
        if (g_strcmp0(arg, "--no-layer-shell") == 0) {
            config->no_layer_shell = TRUE;
        } else if (g_strcmp0(arg, "--debug-input") == 0) {
            config->debug_input = TRUE;
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
        } else if (g_strcmp0(arg, "-h") == 0 || g_strcmp0(arg, "--help") == 0) {
            g_print("Usage: seekey [OPTIONS]\n\n"
                    "Options:\n"
                    "  --config PATH          Config file path, default ~/.config/seekey/config.ini\n"
                    "  --no-layer-shell       Disable gtk4-layer-shell even if available\n"
                    "  --duration MS          Bubble duration, default 1200\n"
                    "  --typing-idle MS       Pause before typed text starts a new bubble, default 650\n"
                    "  --fade-ms MS           Fade duration when disappear=fade, default 180\n"
                    "  --margin PX            Bottom margin in layer-shell mode, default 96\n"
                    "  --max-items N          Maximum visible bubbles, default 5\n"
                    "  --align left|center|right\n"
                    "  --disappear instant|fade\n"
                    "  --debug-input          Print input discovery and events\n"
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

static void activate(GtkApplication *app, gpointer user_data)
{
    AppState *state = user_data;
    state->app = app;

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
    if (!load_config_file(&state.config, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return 2;
    }

    if (!parse_args(&state.config, &argc, &argv, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return 2;
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
