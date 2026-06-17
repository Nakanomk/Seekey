#include "seekey.h"

#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#define MAX_KEY_CODE KEY_MAX

typedef struct {
    int fd;
    struct libevdev *dev;
    char *path;
    char *name;
} InputDevice;

struct SeekeyInput {
    SeekeyConfig config;
    GPtrArray *devices;
    GThread *thread;
    GMutex lock;
    gboolean stop;
    gboolean pressed[MAX_KEY_CODE + 1];
    SeekeyKeyCallback callback;
    gpointer user_data;
};

typedef struct {
    SeekeyInput *input;
    KeyEventMessage event;
} Dispatch;

static gboolean dispatch_key_event(gpointer data)
{
    Dispatch *dispatch = data;
    dispatch->input->callback(&dispatch->event, dispatch->input->user_data);
    g_free(dispatch);
    return G_SOURCE_REMOVE;
}

static void input_device_free(gpointer data)
{
    InputDevice *device = data;
    if (device == NULL) {
        return;
    }
    if (device->dev != NULL) {
        libevdev_free(device->dev);
    }
    if (device->fd >= 0) {
        close(device->fd);
    }
    g_free(device->path);
    g_free(device->name);
    g_free(device);
}

static gboolean device_has_keyboard_keys(struct libevdev *dev)
{
    guint useful_keys[] = {
        KEY_A, KEY_Z, KEY_1, KEY_0, KEY_ENTER, KEY_SPACE, KEY_LEFTCTRL,
        KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTALT, KEY_RIGHTALT,
        KEY_LEFTMETA, KEY_RIGHTMETA,
    };

    if (!libevdev_has_event_type(dev, EV_KEY)) {
        return FALSE;
    }

    guint found = 0;
    for (gsize i = 0; i < G_N_ELEMENTS(useful_keys); i++) {
        if (libevdev_has_event_code(dev, EV_KEY, useful_keys[i])) {
            found++;
        }
    }

    return found >= 6;
}

static gboolean add_device(SeekeyInput *input, const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        if (input->config.debug_input) {
            g_printerr("seekey: skip %s: %s\n", path, g_strerror(errno));
        }
        return FALSE;
    }

    struct libevdev *dev = NULL;
    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        if (input->config.debug_input) {
            g_printerr("seekey: skip %s: %s\n", path, g_strerror(-rc));
        }
        close(fd);
        return FALSE;
    }

    if (!device_has_keyboard_keys(dev)) {
        libevdev_free(dev);
        close(fd);
        return FALSE;
    }

    InputDevice *device = g_new0(InputDevice, 1);
    device->fd = fd;
    device->dev = dev;
    device->path = g_strdup(path);
    const char *device_name = libevdev_get_name(dev);
    device->name = g_strdup(device_name != NULL ? device_name : "keyboard");
    g_ptr_array_add(input->devices, device);

    if (input->config.debug_input) {
        g_printerr("seekey: using %s (%s)\n", device->path, device->name);
    }

    return TRUE;
}

static gint compare_modifier_codes(gconstpointer a, gconstpointer b)
{
    guint ca = GPOINTER_TO_UINT(*(gconstpointer *)a);
    guint cb = GPOINTER_TO_UINT(*(gconstpointer *)b);
    int oa = seekey_modifier_order(ca);
    int ob = seekey_modifier_order(cb);
    if (oa != ob) {
        return oa - ob;
    }
    return (int)ca - (int)cb;
}

static void build_combo(SeekeyInput *input, guint code, char *buffer, gsize size)
{
    GPtrArray *parts = g_ptr_array_new();

    for (guint i = 0; i <= MAX_KEY_CODE; i++) {
        if (input->pressed[i] && seekey_is_modifier(i)) {
            gboolean duplicate = FALSE;
            const char *name = seekey_key_name(i);
            for (guint p = 0; p < parts->len; p++) {
                guint existing = GPOINTER_TO_UINT(g_ptr_array_index(parts, p));
                if (g_strcmp0(seekey_key_name(existing), name) == 0) {
                    duplicate = TRUE;
                    break;
                }
            }
            if (!duplicate) {
                g_ptr_array_add(parts, GUINT_TO_POINTER(i));
            }
        }
    }

    g_ptr_array_sort(parts, compare_modifier_codes);

    GString *combo = g_string_new(NULL);
    for (guint i = 0; i < parts->len; i++) {
        guint modifier = GPOINTER_TO_UINT(g_ptr_array_index(parts, i));
        if (combo->len > 0) {
            g_string_append(combo, " + ");
        }
        g_string_append(combo, seekey_key_name(modifier));
    }

    if (!seekey_is_modifier(code) || combo->len == 0) {
        if (combo->len > 0) {
            g_string_append(combo, " + ");
        }
        g_string_append(combo, seekey_key_name(code));
    }

    g_strlcpy(buffer, combo->str, size);
    g_string_free(combo, TRUE);
    g_ptr_array_free(parts, TRUE);
}

static void emit_event(SeekeyInput *input, const struct input_event *ev)
{
    if (ev->type != EV_KEY || ev->code > MAX_KEY_CODE) {
        return;
    }

    if (ev->value == 2) {
        return;
    }

    if (ev->value == 1) {
        input->pressed[ev->code] = TRUE;
    } else if (ev->value == 0) {
        input->pressed[ev->code] = FALSE;
        return;
    } else {
        return;
    }

    Dispatch *dispatch = g_new0(Dispatch, 1);
    dispatch->input = input;
    dispatch->event.code = ev->code;
    dispatch->event.value = ev->value;
    dispatch->event.shifted = input->pressed[KEY_LEFTSHIFT] ||
                              input->pressed[KEY_RIGHTSHIFT];
    dispatch->event.has_non_shift_modifier =
        input->pressed[KEY_LEFTCTRL] || input->pressed[KEY_RIGHTCTRL] ||
        input->pressed[KEY_LEFTALT] || input->pressed[KEY_RIGHTALT] ||
        input->pressed[KEY_LEFTMETA] || input->pressed[KEY_RIGHTMETA];
    g_strlcpy(dispatch->event.name,
              seekey_key_name(ev->code),
              sizeof(dispatch->event.name));
    build_combo(input,
                ev->code,
                dispatch->event.combo,
                sizeof(dispatch->event.combo));

    if (input->config.debug_input) {
        g_printerr("seekey: key %u value %d: %s\n",
                   ev->code,
                   ev->value,
                   dispatch->event.combo);
    }

    g_main_context_invoke(NULL, dispatch_key_event, dispatch);
}

static gpointer input_thread(gpointer data)
{
    SeekeyInput *input = data;
    guint count = input->devices->len;
    struct pollfd *fds = g_new0(struct pollfd, count);

    for (guint i = 0; i < count; i++) {
        InputDevice *device = g_ptr_array_index(input->devices, i);
        fds[i].fd = device->fd;
        fds[i].events = POLLIN;
    }

    while (TRUE) {
        g_mutex_lock(&input->lock);
        gboolean stop = input->stop;
        g_mutex_unlock(&input->lock);
        if (stop) {
            break;
        }

        int rc = poll(fds, count, 100);
        if (rc <= 0) {
            continue;
        }

        for (guint i = 0; i < count; i++) {
            if (!(fds[i].revents & POLLIN)) {
                continue;
            }

            InputDevice *device = g_ptr_array_index(input->devices, i);
            struct input_event ev;
            while (libevdev_next_event(device->dev,
                                       LIBEVDEV_READ_FLAG_NORMAL,
                                       &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
                emit_event(input, &ev);
            }
        }
    }

    g_free(fds);
    return NULL;
}

SeekeyInput *seekey_input_new(const SeekeyConfig *config,
                              SeekeyKeyCallback callback,
                              gpointer user_data,
                              GError **error)
{
    SeekeyInput *input = g_new0(SeekeyInput, 1);
    input->config = *config;
    input->devices = g_ptr_array_new_with_free_func(input_device_free);
    input->callback = callback;
    input->user_data = user_data;
    g_mutex_init(&input->lock);

    for (guint i = 0; i < 96; i++) {
        char path[64];
        g_snprintf(path, sizeof(path), "/dev/input/event%u", i);
        add_device(input, path);
    }

    if (input->devices->len == 0) {
        g_set_error(error,
                    G_IO_ERROR,
                    G_IO_ERROR_PERMISSION_DENIED,
                    "No readable keyboard devices found under /dev/input/event*");
        seekey_input_free(input);
        return NULL;
    }

    return input;
}

void seekey_input_start(SeekeyInput *input)
{
    input->thread = g_thread_new("seekey-input", input_thread, input);
}

void seekey_input_stop(SeekeyInput *input)
{
    if (input == NULL) {
        return;
    }

    g_mutex_lock(&input->lock);
    input->stop = TRUE;
    g_mutex_unlock(&input->lock);

    if (input->thread != NULL) {
        g_thread_join(input->thread);
        input->thread = NULL;
    }
}

void seekey_input_free(SeekeyInput *input)
{
    if (input == NULL) {
        return;
    }
    seekey_input_stop(input);
    g_ptr_array_free(input->devices, TRUE);
    g_mutex_clear(&input->lock);
    g_free(input);
}
