#include "style.h"

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
