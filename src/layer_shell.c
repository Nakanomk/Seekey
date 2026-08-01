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
typedef gboolean (*IsSupportedFn)(void);

typedef struct {
    void *handle;
    InitForWindowFn init_for_window;
    SetLayerFn set_layer;
    SetAnchorFn set_anchor;
    SetMarginFn set_margin;
    SetKeyboardModeFn set_keyboard_mode;
    SetNamespaceFn set_namespace;
    SetExclusiveZoneFn set_exclusive_zone;
    SetMonitorFn set_monitor;
    IsSupportedFn is_supported;
} LayerShellApi;

static gboolean load_layer_shell_api(LayerShellApi *api, GError **error)
{
    memset(api, 0, sizeof(*api));
    api->handle = dlopen("libgtk4-layer-shell.so.0", RTLD_NOW | RTLD_LOCAL);
    if (api->handle == NULL) {
        api->handle = dlopen("libgtk4-layer-shell.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (api->handle == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    "gtk4-layer-shell unavailable: %s", dlerror());
        return FALSE;
    }

    api->init_for_window =
        (InitForWindowFn)dlsym(api->handle, "gtk_layer_init_for_window");
    api->set_layer =
        (SetLayerFn)dlsym(api->handle, "gtk_layer_set_layer");
    api->set_anchor =
        (SetAnchorFn)dlsym(api->handle, "gtk_layer_set_anchor");
    api->set_margin =
        (SetMarginFn)dlsym(api->handle, "gtk_layer_set_margin");
    api->set_keyboard_mode = (SetKeyboardModeFn)dlsym(
        api->handle, "gtk_layer_set_keyboard_mode");
    api->set_namespace =
        (SetNamespaceFn)dlsym(api->handle, "gtk_layer_set_namespace");
    api->set_exclusive_zone = (SetExclusiveZoneFn)dlsym(
        api->handle, "gtk_layer_set_exclusive_zone");
    api->set_monitor =
        (SetMonitorFn)dlsym(api->handle, "gtk_layer_set_monitor");
    api->is_supported =
        (IsSupportedFn)dlsym(api->handle, "gtk_layer_is_supported");

    if (api->init_for_window == NULL || api->set_layer == NULL ||
        api->set_anchor == NULL || api->set_margin == NULL ||
        api->set_keyboard_mode == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                            "gtk4-layer-shell is missing required symbols");
        dlclose(api->handle);
        api->handle = NULL;
        return FALSE;
    }
    if (api->is_supported != NULL && !api->is_supported()) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                            "the compositor does not support layer-shell");
        dlclose(api->handle);
        api->handle = NULL;
        return FALSE;
    }
    return TRUE;
}

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

    LayerShellApi api;
    if (!load_layer_shell_api(&api, error)) {
        return FALSE;
    }

    api.init_for_window(window);
    if (api.set_namespace != NULL) {
        api.set_namespace(window, "seekey");
    }
    api.set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);

    if (monitor != NULL && api.set_monitor != NULL) {
        api.set_monitor(window, monitor);
    }

    api.set_anchor(window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    api.set_margin(window, GTK_LAYER_SHELL_EDGE_BOTTOM,
                   (int)config->margin_px);

    if (g_strcmp0(config->align, "left") == 0) {
        api.set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        if (config->margin_horizontal_px > 0) {
            api.set_margin(window, GTK_LAYER_SHELL_EDGE_LEFT,
                           (int)config->margin_horizontal_px);
        }
    } else if (g_strcmp0(config->align, "right") == 0) {
        api.set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        if (config->margin_horizontal_px > 0) {
            api.set_margin(window, GTK_LAYER_SHELL_EDGE_RIGHT,
                           (int)config->margin_horizontal_px);
        }
    } else {
        api.set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        api.set_anchor(window, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    }
    if (api.set_exclusive_zone != NULL) {
        api.set_exclusive_zone(window, 0);
    }
    api.set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

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

gboolean seekey_layer_shell_try_init_menu(GtkWindow *window, GError **error)
{
    LayerShellApi api;
    if (!load_layer_shell_api(&api, error)) return FALSE;

    api.init_for_window(window);
    if (api.set_namespace != NULL)
        api.set_namespace(window, "seekey-config");
    api.set_layer(window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    if (api.set_exclusive_zone != NULL) api.set_exclusive_zone(window, 0);
    api.set_keyboard_mode(window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    return TRUE;
}
