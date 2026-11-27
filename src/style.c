#include "style.h"

#include <linux/input-event-codes.h>

char *seekey_style_build_css(const SeekeyConfig *config)
{
    char *font_family = NULL;
    char *font_rule = NULL;
    if (g_strcmp0(config->key_font_family, "inherit") == 0) {
        font_rule = g_strdup("");
    } else {
        font_family = g_strescape(config->key_font_family, NULL);
        font_rule = g_strdup_printf("  font-family: \"%s\";",
                                    font_family != NULL ? font_family : "Sans");
    }
    char *css = g_strdup_printf(
        "window.seekey-overlay { background: transparent; }"
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
        "%s"
        "  font-size: %upx;"
        /* Pango can round the baselines of mixed-font runs differently.
         * Reserve a shared line box, including room for their ascenders. */
        "  line-height: 1em;"
        "  min-height: 1.25em;"
        "  font-weight: %u;"
        "  opacity: 1;"
        "  transition: opacity %ums ease-out;"
        "}"
        ".placeholder-bubble {"
        "  color: %s;"
        "  background: %s;"
        "  border-color: %s;"
        "}"
        ".fading { opacity: 0; }",
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
        font_rule,
        config->key_font_px,
        config->key_font_weight,
        config->fade_ms,
        config->placeholder_foreground,
        config->placeholder_background,
        config->placeholder_border_color);
    g_free(font_rule);
    g_free(font_family);
    return css;
}

void seekey_style_install(const SeekeyConfig *config)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    char *css = seekey_style_build_css(config);
    gtk_css_provider_load_from_string(provider, css);
    g_free(css);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static GtkWidget *style_bubble(GtkWidget *widget)
{
    gtk_widget_set_valign(widget, GTK_ALIGN_END);
    gtk_widget_add_css_class(widget, "key-bubble");
    return widget;
}

GtkWidget *seekey_bubble_label_new(const char *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    return style_bubble(label);
}

static void draw_mouse_icon(GtkDrawingArea *area, cairo_t *cr, int width,
                            int height, gpointer data)
{
    guint code = GPOINTER_TO_UINT(data);
    double size = MIN(width, height);
    if (size <= 0) return;

    GdkRGBA color;
    gtk_widget_get_color(GTK_WIDGET(area), &color);
    cairo_save(cr);
    cairo_translate(cr, (width - size) / 2, (height - size) / 2);
    cairo_scale(cr, size / 24, size / 24);
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_set_line_width(cr, 1.6);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    /* The same mouse outline frames clicks and wheel directions. */
    cairo_move_to(cr, 4.5, 9);
    cairo_curve_to(cr, 4.5, 4, 7.5, 1.5, 12, 1.5);
    cairo_curve_to(cr, 16.5, 1.5, 19.5, 4, 19.5, 9);
    cairo_line_to(cr, 19.5, 15);
    cairo_curve_to(cr, 19.5, 19.5, 16.5, 22.5, 12, 22.5);
    cairo_curve_to(cr, 7.5, 22.5, 4.5, 19.5, 4.5, 15);
    cairo_close_path(cr);
    cairo_stroke_preserve(cr);
    cairo_clip(cr);

    if (code < SEEKEY_SCROLL_UP) {
        if (code == BTN_LEFT || code == BTN_RIGHT) {
            cairo_rectangle(cr, code == BTN_LEFT ? 0 : 14.5, 0, 9.5, 12);
            cairo_fill(cr);
        }
        cairo_move_to(cr, 4.5, 12);
        cairo_line_to(cr, 19.5, 12);
        cairo_stroke(cr);
        cairo_rectangle(cr, 10.4, 3.8, 3.2, 6.4);
        if (code == BTN_MIDDLE)
            cairo_fill(cr);
        else
            cairo_stroke(cr);
    } else {
        cairo_translate(cr, 12, 12);
        switch (code) {
        case SEEKEY_SCROLL_DOWN:  cairo_rotate(cr, G_PI); break;
        case SEEKEY_SCROLL_LEFT:  cairo_rotate(cr, -G_PI_2); break;
        case SEEKEY_SCROLL_RIGHT: cairo_rotate(cr, G_PI_2); break;
        }
        cairo_move_to(cr, 0, 4.5);
        cairo_line_to(cr, 0, -4.5);
        cairo_move_to(cr, -3.5, -1);
        cairo_line_to(cr, 0, -4.5);
        cairo_line_to(cr, 3.5, -1);
        cairo_stroke(cr);
    }
    cairo_restore(cr);
}

GtkWidget *seekey_mouse_bubble_new(const SeekeyConfig *config, guint code)
{
    const char *override = seekey_key_icon(code, config);
    if (override != NULL) return seekey_bubble_label_new(override);

    const char *name = seekey_mouse_button_name(code);
    if (code != BTN_LEFT && code != BTN_RIGHT && code != BTN_MIDDLE &&
        !(code >= SEEKEY_SCROLL_UP && code <= SEEKEY_SCROLL_RIGHT))
        return seekey_bubble_label_new(name);

    GtkWidget *area = g_object_new(GTK_TYPE_DRAWING_AREA,
                                   "accessible-role", GTK_ACCESSIBLE_ROLE_IMG, NULL);
    /* Reserve horizontal space only. CSS supplies the same line height as
     * a keyboard bubble; the drawing fits inside it, never the reverse. */
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(area), (int)config->key_font_px);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), draw_mouse_icon,
                                   GUINT_TO_POINTER(code), NULL);
    gtk_accessible_update_property(GTK_ACCESSIBLE(area), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   name, -1);
    return style_bubble(area);
}
