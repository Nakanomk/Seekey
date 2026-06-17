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

gboolean seekey_layer_shell_try_init(GtkWindow *window,
                                     const SeekeyConfig *config,
                                     GError **error)
{
    if (config->no_layer_shell) {
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

    if (init_for_window == NULL || set_layer == NULL || set_anchor == NULL ||
        set_margin == NULL || set_keyboard_mode == NULL) {
        g_set_error_literal(error,
                            G_IO_ERROR,
                            G_IO_ERROR_NOT_SUPPORTED,
                            "gtk4-layer-shell is missing required symbols");
        return FALSE;
    }

    init_for_window(window);
    set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM, (int)config->margin_px);
    set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    return TRUE;
}
