#include "seekey.h"
#include "config.h"
#include "tui.h"
#include "window_state.h"

#include <locale.h>
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
            /* typing-max-width is in CSS pixels; convert to chars using a
             * rough heuristic (≈ pixel / 0.55 / font-size). For default
             * 20px font this gives ≈ 1.4 chars per CSS px, so 480px ≈ 67 chars. */
            if (state->config.typing_max_width > 0) {
                guint chars = (state->config.typing_max_width * 10) /
                              (state->config.key_font_px * 6);
                if (chars > 0) {
                    gtk_label_set_max_width_chars(GTK_LABEL(state->typing_label),
                                                  (gint)chars);
                }
            }
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
        /* Note: max-width is not a GTK CSS property. The typing bubble
         * relies on the C-side `gtk_label_set_ellipsize(PANGO_ELLIPSIZE_END)`
         * plus `gtk_label_set_max_width_chars()` for long-text truncation;
         * style.typing-max-width controls the latter (in characters, not px). */
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

    /* Compositor-specific hints (informational only for now).
     *
     * Substring match on $XDG_CURRENT_DESKTOP, first match wins. Order
     * matters: more specific IDs must come BEFORE the more general
     * substrings they contain. E.g. "KWinFT" must precede "KDE" because
     * the latter is a substring of the former. */
    struct { const char *id; const char *hint; } hints[] = {
        /* Specific wlroots-based compositors first. */
        {"KWinFT", "  hint: KWinFT (KDE wlroots fork) supports wlr-layer-shell;"
                   " layer-shell=auto works well."},
        {"Hyprland", "  hint: Hyprland supports wlr-layer-shell; layer-shell=auto works well."},
        {"niri", "  hint: layer-shell works; window is anchored to the chosen edge"
                 " and follows the focused monitor (persisted across sessions)."},
        {"sway",  "  hint: Sway supports wlr-layer-shell; layer-shell=auto works well."},
        {"river", "  hint: river supports wlr-layer-shell; layer-shell=auto works well."},
        {"wayfire", "  hint: Wayfire supports wlr-layer-shell; layer-shell=auto works well."},
        {"labwc", "  hint: labwc supports wlr-layer-shell; layer-shell=auto works well."},
        /* Fallback-only compositors after. */
        {"KDE", "  hint: KWin (default KDE Plasma) does not support wlr-layer-shell."
                "  seekey falls back to a normal window. See README §GNOME/KDE"
                "  fallback for window rules to pin position and raise."},
        {"GNOME", "  hint: GNOME does not support wlr-layer-shell."
                  "  seekey falls back to a normal window. See README §GNOME/KDE"
                  "  fallback for window rules to pin position and raise."},
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
    /* The GApplication ID "dev.seekey" (set below) becomes the
     * Wayland `app_id` and the X11 `WM_CLASS` (dots → underscores:
     * `dev_seekey`). KWin / Hyprland window rules and GNOME extensions
     * can target it directly. */
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

    /* Resolve monitor from saved state (if any). NULL = let the
     * compositor / layer-shell choose. */
    SeekeyWindowState wstate;
    seekey_window_state_load(&wstate, NULL);
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = NULL;
    if (wstate.monitor[0] != '\0') {
        monitor = seekey_find_monitor_by_name(display, wstate.monitor);
        if (monitor == NULL) {
            g_printerr("seekey: saved monitor '%s' not found, using default\n",
                       wstate.monitor);
        }
    }

    GError *layer_error = NULL;
    if (!seekey_layer_shell_try_init(GTK_WINDOW(window), &state->config,
                                      monitor, &layer_error)) {
        if (g_strcmp0(state->config.layer_shell, "required") == 0) {
            g_printerr("seekey: layer-shell required but unavailable: %s\n",
                       layer_error->message);
            g_printerr("seekey: install gtk4-layer-shell and use a compositor that supports wlr-layer-shell.\n");
            g_clear_error(&layer_error);
            exit(2);
        }
        g_printerr("seekey: using fallback window: %s\n", layer_error->message);
        g_printerr("seekey: NOTE: you are not running under a wlr-layer-shell compositor.\n"
                   "seekey:       The following settings have NO EFFECT in fallback mode:\n"
                   "seekey:         - style.align / margin / margin-horizontal (no edge anchoring)\n"
                   "seekey:         - saved window position (compositor controls placement)\n"
                   "seekey:       To enable them, use a layer-shell compositor (niri, Hyprland,\n"
                   "seekey:       Sway, river) or install gtk4-layer-shell on a supported system.\n"
                   "seekey:       See README §Compatibility model for details.\n");
        g_clear_error(&layer_error);
    }

    /* Persist the monitor we are now on, regardless of how we exit.
     * layer-shell surfaces are anchored at startup and do not move at
     * runtime, so recording the monitor here is sufficient — no need to
     * wait for shutdown (which may not fire on SIGKILL / SIGHUP). */
    {
        GtkNative *native = gtk_widget_get_native(window);
        GdkSurface *surface = native != NULL ? gtk_native_get_surface(native) : NULL;
        if (surface != NULL) {
            GdkDisplay *display = gdk_surface_get_display(surface);
            GdkMonitor *m = gdk_display_get_monitor_at_surface(display, surface);
            if (m != NULL) {
                const char *connector = gdk_monitor_get_connector(m);
                if (connector != NULL && connector[0] != '\0') {
                    SeekeyWindowState s;
                    memset(&s, 0, sizeof(s));
                    g_strlcpy(s.monitor, connector, sizeof(s.monitor));
                    seekey_window_state_save(&s, NULL);
                }
            }
        }
    }
    g_clear_object(&monitor);

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

    /* Persist which monitor the window was on so the next launch can
     * restore it. Failure is silent — the state file is best-effort.
     * Note: we also save in activate() so even hard-killed processes
     * leave a state file. */
    if (state->window != NULL) {
        GtkNative *native = gtk_widget_get_native(state->window);
        GdkSurface *surface = native != NULL ? gtk_native_get_surface(native) : NULL;
        if (surface != NULL) {
            GdkDisplay *display = gdk_surface_get_display(surface);
            GdkMonitor *monitor =
                gdk_display_get_monitor_at_surface(display, surface);
            if (monitor != NULL) {
                const char *connector = gdk_monitor_get_connector(monitor);
                if (connector != NULL && connector[0] != '\0') {
                    SeekeyWindowState wstate;
                    memset(&wstate, 0, sizeof(wstate));
                    g_strlcpy(wstate.monitor, connector,
                              sizeof(wstate.monitor));
                    seekey_window_state_save(&wstate, NULL);
                }
            }
        }
    }

    cancel_active_typing_remove_timeout(state);
    cancel_active_typing_idle_timeout(state);
    clear_typing_group(state);
    seekey_input_free(state->input);
    state->input = NULL;
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    AppState state = {0};
    seekey_config_set_defaults(&state.config);

    /* If user explicitly says --init-config --xdg, pre-set path so init
     * writes to ~/.config/seekey/config.ini. */
    if (seekey_cli_has_flag(argc, argv, "--xdg")) {
        state.config.xdg_config = TRUE;
    }

    GError *error = NULL;
    if (!seekey_config_resolve_path(&state.config, argc, argv, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return 2;
    }

    /* For --init-config, default the path to ./seekey.ini (or XDG) when
     * none was found. */
    if (seekey_cli_has_flag(argc, argv, "--init-config") &&
        state.config.config_path[0] == '\0') {
        char *cwd = g_get_current_dir();
        char *p = g_build_filename(cwd, "seekey.ini", NULL);
        g_free(cwd);
        g_strlcpy(state.config.config_path, p, sizeof(state.config.config_path));
        g_free(p);
    }

    if (!seekey_config_load(&state.config, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return 2;
    }

    if (!seekey_parse_args(&state.config, &argc, &argv, &error)) {
        g_printerr("seekey: %s\n", error->message);
        g_clear_error(&error);
        return 2;
    }

    if (state.config.init_config) {
        if (!seekey_config_init(&state.config, state.config.force, &error)) {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
            return 2;
        }
        g_print("Wrote config to %s\n", state.config.config_path);
        return 0;
    }

    if (state.config.print_config) {
        if (!seekey_config_print(&state.config, &error)) {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
            return 2;
        }
        return 0;
    }

    if (state.config.validate_config) {
        if (!seekey_config_validate(&state.config, &error)) {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
            return 2;
        }
        g_print("Config OK: %s\n", state.config.config_path);
        return 0;
    }

    if (state.config.config_tui) {
        if (!seekey_tui_run(&state.config, &error)) {
            g_printerr("seekey: %s\n", error->message);
            g_clear_error(&error);
            return 2;
        }
        return 0;
    }

    GtkApplication *app = gtk_application_new("dev.seekey",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
    g_signal_connect(app, "shutdown", G_CALLBACK(shutdown_app), &state);

    int app_argc = 1;
    int status = g_application_run(G_APPLICATION(app), app_argc, argv);
    g_object_unref(app);
    return status;
}
