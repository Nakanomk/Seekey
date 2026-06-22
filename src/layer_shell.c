#include "seekey.h"

#include <dlfcn.h>

typedef enum {
    GTK_LAYER_SHELL_LAYER_BACKGROUND = 0,
    GTK_LAYER_SHELL_LAYER_BOTTOM = 1,
    GTK_LAYER_SHELL_LAYER_TOP = 2,
    GTK_LAYER_SHELL_LAYER_OVERLAY = 3,
} GtkLayerShellLayerCompat;

typedef enum {
    GTK_LAYER_SHELL_EDGE_LEFT = 0,
    GTK_LAYER_SHELL_EDGE_RIGHT = 1,
    GTK_LAYER_SHELL_EDGE_TOP = 2,
    GTK_LAYER_SHELL_EDGE_BOTTOM = 3,
} GtkLayerShellEdgeCompat;

typedef enum {
    GTK_LAYER_SHELL_KEYBOARD_MODE_NONE = 0,
    GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE = 1,
    GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND = 2,
} GtkLayerShellKeyboardModeCompat;

typedef void (*InitForWindowFn)(GtkWindow *);
typedef void (*SetLayerFn)(GtkWindow *, GtkLayerShellLayerCompat);
typedef void (*SetAnchorFn)(GtkWindow *, GtkLayerShellEdgeCompat, gboolean);
typedef void (*SetMarginFn)(GtkWindow *, GtkLayerShellEdgeCompat, int);
typedef void (*SetKeyboardModeFn)(GtkWindow *, GtkLayerShellKeyboardModeCompat);
typedef void (*SetNamespaceFn)(GtkWindow *, const char *);
typedef void (*SetExclusiveZoneFn)(GtkWindow *, int);
typedef void (*SetMonitorFn)(GtkWindow *, GdkMonitor *);

gboolean seekey_layer_shell_try_init(GtkWindow *window,
                                     const SeekeyConfig *config,
                                     GdkMonitor *monitor,
                                     GError **error)
{
    if (config->no_layer_shell || g_strcmp0(config->layer_shell, "off") == 0) {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_NOT_SUPPORTED,
                            "layer-shell disabled");
        return FALSE;
    }

    void *handle = dlopen("libgtk4-layer-shell.so.0", RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        handle = dlopen("libgtk4-layer-shell.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (handle == NULL) {
        g_set_error(error,
                    G_IO_ERROR,
                    G_IO_ERROR_NOT_FOUND,
                    "gtk4-layer-shell unavailable: %s",
                    dlerror());
        return FALSE;
    }

    InitForWindowFn init_for_window =
        (InitForWindowFn)dlsym(handle, "gtk_layer_init_for_window");
    SetLayerFn set_layer = (SetLayerFn)dlsym(handle, "gtk_layer_set_layer");
    SetAnchorFn set_anchor = (SetAnchorFn)dlsym(handle, "gtk_layer_set_anchor");
    SetMarginFn set_margin = (SetMarginFn)dlsym(handle, "gtk_layer_set_margin");
    SetKeyboardModeFn set_keyboard_mode =
        (SetKeyboardModeFn)dlsym(handle, "gtk_layer_set_keyboard_mode");
    SetNamespaceFn set_namespace =
        (SetNamespaceFn)dlsym(handle, "gtk_layer_set_namespace");
    SetExclusiveZoneFn set_exclusive_zone =
        (SetExclusiveZoneFn)dlsym(handle, "gtk_layer_set_exclusive_zone");
    SetMonitorFn set_monitor =
        (SetMonitorFn)dlsym(handle, "gtk_layer_set_monitor");

    if (init_for_window == NULL || set_layer == NULL || set_anchor == NULL ||
        set_margin == NULL || set_keyboard_mode == NULL) {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_NOT_SUPPORTED,
                            "gtk4-layer-shell is missing required symbols");
        return FALSE;
    }

    init_for_window(window);
    if (set_namespace != NULL) {
        set_namespace(window, "seekey");
    }
    set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);

    if (monitor != NULL && set_monitor != NULL) {
        set_monitor(window, monitor);
    }

    set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM, (int)config->margin_px);

    if (g_strcmp0(config->align, "left") == 0) {
        set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        if (config->margin_horizontal_px > 0) {
            set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT,
                       (int)config->margin_horizontal_px);
        }
    } else if (g_strcmp0(config->align, "right") == 0) {
        set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        if (config->margin_horizontal_px > 0) {
            set_margin(window, GTK_LAYER_SHELL_EDGE_RIGHT,
                       (int)config->margin_horizontal_px);
        }
    } else {
        set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    }
    if (set_exclusive_zone != NULL) {
        set_exclusive_zone(window, 0);
    }
    set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    if (g_strcmp0(config->align, "left") == 0) {
        g_print(_("seekey: layer-shell active (anchor bottom-left, margin bottom=%u horizontal=%u)\n"),
                config->margin_px, config->margin_horizontal_px);
    } else if (g_strcmp0(config->align, "right") == 0) {
        g_print(_("seekey: layer-shell active (anchor bottom-right, margin bottom=%u horizontal=%u)\n"),
                config->margin_px, config->margin_horizontal_px);
    } else {
        g_print(_("seekey: layer-shell active (anchor bottom full-width center)\n"));
    }

    return TRUE;
}
