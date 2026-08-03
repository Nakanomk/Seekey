#include "seekey.h"
#include "runtime_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#define MAX_KEY_CODE KEY_MAX

typedef struct {
    int fd;
    struct libevdev *dev;
    char *path;
    char *name;
    gboolean active;
} InputDevice;

struct SeekeyInput {
    SeekeyConfig config;
    SeekeyRuntimeLock *runtime_lock;
    GPtrArray *devices;
    GPtrArray *mouse_devices;
    int watch_fd;
    GThread *thread;
    GMutex lock;
    gboolean stop;
    gboolean pressed[MAX_KEY_CODE + 1];
    gboolean caps_lock;
    gboolean caps_lock_initialized;
    /* Shift is deferred: we don't emit it on press, only on release if no
     * other key was pressed while Shift was held (i.e. the user really
     * pressed Shift alone). Cleared when any non-shift key is pressed. */
    gboolean shift_pending;
    SeekeyKeyCallback callback;
    gpointer user_data;
    gint64 last_scroll_time[4];  /* up / down / left / right */
};

typedef struct {
    SeekeyKeyCallback callback;
    gpointer user_data;
    KeyEventMessage event;
} Dispatch;

static gboolean dispatch_key_event(gpointer data)
{
    Dispatch *dispatch = data;
    dispatch->callback(&dispatch->event, dispatch->user_data);
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

static gboolean device_has_mouse_buttons(struct libevdev *dev)
{
    if (!libevdev_has_event_type(dev, EV_KEY)) return FALSE;
    if (!libevdev_has_event_code(dev, EV_KEY, BTN_LEFT)) return FALSE;
    if (!libevdev_has_event_code(dev, EV_KEY, BTN_RIGHT)) return FALSE;
    return libevdev_has_event_type(dev, EV_REL);
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

    /* Seed Caps Lock from the device LED when available. This matters when
     * seekey starts while Caps Lock is already enabled. Subsequent EV_LED
     * events keep the state authoritative. */
    if (!input->caps_lock_initialized &&
        libevdev_has_event_code(dev, EV_LED, LED_CAPSL)) {
        input->caps_lock =
            libevdev_get_event_value(dev, EV_LED, LED_CAPSL) != 0;
        input->caps_lock_initialized = TRUE;
    }

    InputDevice *device = g_new0(InputDevice, 1);
    device->fd = fd;
    device->dev = dev;
    device->path = g_strdup(path);
    device->active = TRUE;
    const char *device_name = libevdev_get_name(dev);
    device->name = g_strdup(device_name != NULL ? device_name : "keyboard");
    g_ptr_array_add(input->devices, device);

    if (input->config.debug_input) {
        g_printerr("seekey: using %s (%s)\n", device->path, device->name);
    }

    return TRUE;
}

/* Like add_device but for mice — relaxed criteria, shares the fd model. */
static gboolean add_mouse_device(SeekeyInput *input, const char *path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return FALSE;

    struct libevdev *dev = NULL;
    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) { close(fd); return FALSE; }

    if (!device_has_mouse_buttons(dev)) {
        libevdev_free(dev);
        close(fd);
        return FALSE;
    }

    InputDevice *device = g_new0(InputDevice, 1);
    device->fd = fd;
    device->dev = dev;
    device->path = g_strdup(path);
    device->active = TRUE;
    const char *name = libevdev_get_name(dev);
    device->name = g_strdup(name ? name : "mouse");
    g_ptr_array_add(input->mouse_devices, device);

    if (input->config.debug_input)
        g_printerr("seekey: using mouse %s (%s)\n", device->path, device->name);
    return TRUE;
}

static gboolean is_event_node_name(const char *name)
{
    if (!g_str_has_prefix(name, "event") || name[5] == '\0') return FALSE;
    for (const char *p = name + 5; *p != '\0'; p++) {
        if (!g_ascii_isdigit(*p)) return FALSE;
    }
    return TRUE;
}

static gboolean device_array_has_path(GPtrArray *devices, const char *path)
{
    for (guint i = 0; i < devices->len; i++) {
        InputDevice *device = g_ptr_array_index(devices, i);
        if (device->active && g_strcmp0(device->path, path) == 0) return TRUE;
    }
    return FALSE;
}

static gboolean prune_inactive_devices(GPtrArray *devices)
{
    gboolean changed = FALSE;
    for (guint i = devices->len; i > 0; i--) {
        InputDevice *device = g_ptr_array_index(devices, i - 1);
        if (!device->active ||
            !g_file_test(device->path, G_FILE_TEST_EXISTS)) {
            g_ptr_array_remove_index(devices, i - 1);
            changed = TRUE;
        }
    }
    return changed;
}

static gboolean scan_input_devices(SeekeyInput *input)
{
    gboolean keyboard_changed = prune_inactive_devices(input->devices);
    prune_inactive_devices(input->mouse_devices);

    GDir *dir = g_dir_open("/dev/input", 0, NULL);
    if (dir == NULL) return keyboard_changed;

    const char *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (!is_event_node_name(name)) continue;
        char *path = g_build_filename("/dev/input", name, NULL);
        if (!device_array_has_path(input->devices, path)) {
            keyboard_changed = add_device(input, path) || keyboard_changed;
        }
        if (input->config.show_mouse &&
            !device_array_has_path(input->mouse_devices, path)) {
            add_mouse_device(input, path);
        }
        g_free(path);
    }
    g_dir_close(dir);
    return keyboard_changed;
}

static void setup_input_watch(SeekeyInput *input)
{
    input->watch_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (input->watch_fd < 0) return;
    if (inotify_add_watch(input->watch_fd, "/dev/input",
                          IN_CREATE | IN_DELETE | IN_MOVED_FROM |
                              IN_MOVED_TO | IN_ATTRIB) < 0) {
        close(input->watch_fd);
        input->watch_fd = -1;
    }
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
        const char *icon = seekey_key_icon(code, &input->config);
        g_string_append(combo, icon ? icon : seekey_key_name(code));
    }

    g_strlcpy(buffer, combo->str, size);
    g_string_free(combo, TRUE);
    g_ptr_array_free(parts, TRUE);
}

/* Build a KeyEventMessage for `code` from the current pressed state and
 * hand it to the GTK main thread. The pressed[] table must already reflect
 * the state we want reflected in the event (modifiers held, etc). */
static void dispatch_press(SeekeyInput *input, guint code)
{
    Dispatch *dispatch = g_new0(Dispatch, 1);
    dispatch->callback = input->callback;
    dispatch->user_data = input->user_data;
    dispatch->event.code = code;
    dispatch->event.value = 1;
    dispatch->event.shifted = input->pressed[KEY_LEFTSHIFT] ||
                              input->pressed[KEY_RIGHTSHIFT];
    dispatch->event.caps_lock = input->caps_lock;
    dispatch->event.has_non_shift_modifier =
        input->pressed[KEY_LEFTCTRL] || input->pressed[KEY_RIGHTCTRL] ||
        input->pressed[KEY_LEFTALT] || input->pressed[KEY_RIGHTALT] ||
        input->pressed[KEY_LEFTMETA] || input->pressed[KEY_RIGHTMETA];
    g_strlcpy(dispatch->event.name,
              seekey_key_name(code),
              sizeof(dispatch->event.name));
    build_combo(input,
                code,
                dispatch->event.combo,
                sizeof(dispatch->event.combo));

    dispatch->event.modifier_mask = 0;
    if (input->pressed[KEY_LEFTCTRL] || input->pressed[KEY_RIGHTCTRL])
        dispatch->event.modifier_mask |= SEEKEY_MOD_CTRL;
    if (input->pressed[KEY_LEFTSHIFT] || input->pressed[KEY_RIGHTSHIFT])
        dispatch->event.modifier_mask |= SEEKEY_MOD_SHIFT;
    if (input->pressed[KEY_LEFTALT] || input->pressed[KEY_RIGHTALT])
        dispatch->event.modifier_mask |= SEEKEY_MOD_ALT;
    if (input->pressed[KEY_LEFTMETA] || input->pressed[KEY_RIGHTMETA])
        dispatch->event.modifier_mask |= SEEKEY_MOD_SUPER;

    gboolean private_typing =
        g_strcmp0(input->config.typing_display, "full") != 0 &&
        !dispatch->event.has_non_shift_modifier &&
        !seekey_is_modifier(code) &&
        seekey_key_text(code, dispatch->event.shifted,
                        dispatch->event.caps_lock) != NULL;
    if (input->config.debug_input && !private_typing) {
        g_printerr("seekey: key %u value 1: %s\n",
                   code,
                   dispatch->event.combo);
    }

    g_main_context_invoke(NULL, dispatch_key_event, dispatch);
}

static gboolean is_shift_code(guint code)
{
    return code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT;
}

static void emit_event(SeekeyInput *input, const struct input_event *ev)
{
    if (ev->type != EV_KEY || ev->code > MAX_KEY_CODE) {
        return;
    }

    const char *code_name =
        libevdev_event_code_get_name(EV_KEY, ev->code);
    if (code_name != NULL && g_str_has_prefix(code_name, "BTN_")) {
        return;
    }

    if (ev->value == 2) {
        return;
    }

    if (ev->value == 1) {
        /* Press. */
        input->pressed[ev->code] = TRUE;

        /* EV_LED normally follows this key event and confirms the value.
         * Toggle here as well so keyboards that do not expose LED events
         * still get correct Caps Lock behaviour. */
        if (ev->code == KEY_CAPSLOCK) {
            input->caps_lock = !input->caps_lock;
            input->caps_lock_initialized = TRUE;
        }

        if (is_shift_code(ev->code)) {
            /* Defer Shift: don't emit yet. Only show a lone Shift bubble if
             * the user presses Shift alone and releases it without having
             * pressed any other key. So don't arm the pending flag if some
             * other modifier is already held (e.g. Ctrl then Shift) — in
             * that case Shift is part of a combo, not a solo press. */
            gboolean other_modifier_held =
                input->pressed[KEY_LEFTCTRL] || input->pressed[KEY_RIGHTCTRL] ||
                input->pressed[KEY_LEFTALT] || input->pressed[KEY_RIGHTALT] ||
                input->pressed[KEY_LEFTMETA] || input->pressed[KEY_RIGHTMETA];
            input->shift_pending = !other_modifier_held;
            return;
        }

        /* Any non-shift key clears the pending Shift: it was used as a
         * modifier, not pressed alone. */
        input->shift_pending = FALSE;

        dispatch_press(input, ev->code);
        return;
    }

    if (ev->value == 0) {
        /* Release. */
        if (is_shift_code(ev->code)) {
            gboolean was_pending = input->shift_pending;
            input->pressed[ev->code] = FALSE;
            input->shift_pending = FALSE;
            /* If Shift was held and released with no other key pressed in
             * between, the user really did press Shift alone — show it. */
            if (was_pending) {
                dispatch_press(input, ev->code);
            }
            return;
        }
        input->pressed[ev->code] = FALSE;
        return;
    }
}

/* Mouse button events: only emit on press, one-shot bubbles. */
static void emit_mouse_event(SeekeyInput *input, guint code)
{
    const char *name = seekey_mouse_button_name(code);
    if (name == NULL) return;

    Dispatch *dispatch = g_new0(Dispatch, 1);
    dispatch->callback = input->callback;
    dispatch->user_data = input->user_data;
    dispatch->event.code = code;
    dispatch->event.value = 1;
    dispatch->event.is_mouse = TRUE;
    g_strlcpy(dispatch->event.name, name, sizeof(dispatch->event.name));
    g_strlcpy(dispatch->event.combo, name, sizeof(dispatch->event.combo));

    if (input->config.debug_input)
        g_printerr("seekey: mouse %u: %s\n", code, name);

    g_main_context_invoke(NULL, dispatch_key_event, dispatch);
}

static void emit_scroll_event(SeekeyInput *input, gboolean horizontal,
                              gint value)
{
    if (value == 0) return;

    int direction;
    guint code;
    if (horizontal) {
        direction = value > 0 ? 2 : 3;
        code = value > 0 ? SEEKEY_SCROLL_RIGHT : SEEKEY_SCROLL_LEFT;
    } else {
        direction = value > 0 ? 0 : 1;
        code = value > 0 ? SEEKEY_SCROLL_UP : SEEKEY_SCROLL_DOWN;
    }

    gint64 now = g_get_monotonic_time();
    if (now - input->last_scroll_time[direction] <= 200000) return;
    input->last_scroll_time[direction] = now;
    emit_mouse_event(input, code);
}

static void handle_mouse_event(SeekeyInput *input, InputDevice *device,
                               const struct input_event *ev)
{
    if (ev->type == EV_KEY && ev->value == 1) {
        emit_mouse_event(input, ev->code);
        return;
    }
    if (ev->type != EV_REL) return;

    if (ev->code == REL_WHEEL) {
        emit_scroll_event(input, FALSE, ev->value);
    } else if (ev->code == REL_HWHEEL) {
        emit_scroll_event(input, TRUE, ev->value);
    }
#ifdef REL_WHEEL_HI_RES
    else if (ev->code == REL_WHEEL_HI_RES &&
             !libevdev_has_event_code(device->dev, EV_REL, REL_WHEEL)) {
        emit_scroll_event(input, FALSE, ev->value);
    }
#endif
#ifdef REL_HWHEEL_HI_RES
    else if (ev->code == REL_HWHEEL_HI_RES &&
             !libevdev_has_event_code(device->dev, EV_REL, REL_HWHEEL)) {
        emit_scroll_event(input, TRUE, ev->value);
    }
#endif
}

static void handle_keyboard_event(SeekeyInput *input,
                                  const struct input_event *ev)
{
    if (ev->type == EV_LED && ev->code == LED_CAPSL) {
        input->caps_lock = ev->value != 0;
        input->caps_lock_initialized = TRUE;
    } else {
        emit_event(input, ev);
    }
}

static void rebuild_keyboard_state(SeekeyInput *input)
{
    memset(input->pressed, 0, sizeof(input->pressed));
    gboolean found_caps_led = FALSE;
    gboolean caps_lock = FALSE;

    for (guint i = 0; i < input->devices->len; i++) {
        InputDevice *device = g_ptr_array_index(input->devices, i);
        if (!device->active) continue;
        for (guint code = 0; code <= MAX_KEY_CODE; code++) {
            const char *name = libevdev_event_code_get_name(EV_KEY, code);
            if (name != NULL && g_str_has_prefix(name, "BTN_")) continue;
            if (libevdev_has_event_code(device->dev, EV_KEY, code) &&
                libevdev_get_event_value(device->dev, EV_KEY, code) != 0) {
                input->pressed[code] = TRUE;
            }
        }
        if (libevdev_has_event_code(device->dev, EV_LED, LED_CAPSL)) {
            found_caps_led = TRUE;
            caps_lock = caps_lock ||
                        libevdev_get_event_value(device->dev, EV_LED,
                                                 LED_CAPSL) != 0;
        }
    }

    if (found_caps_led) {
        input->caps_lock = caps_lock;
        input->caps_lock_initialized = TRUE;
    }
    input->shift_pending = FALSE;
}

static void drain_sync_events(SeekeyInput *input, InputDevice *device,
                              gboolean is_mouse)
{
    struct input_event ignored;
    int rc;
    do {
        rc = libevdev_next_event(device->dev, LIBEVDEV_READ_FLAG_SYNC,
                                 &ignored);
    } while (rc == LIBEVDEV_READ_STATUS_SYNC);

    if (!is_mouse) rebuild_keyboard_state(input);
    if (input->config.debug_input) {
        g_printerr("seekey: resynchronized %s after dropped input events\n",
                   device->path);
    }
}

static gboolean read_device_events(SeekeyInput *input, InputDevice *device,
                                   gboolean is_mouse)
{
    while (TRUE) {
        struct input_event ev;
        int rc = libevdev_next_event(device->dev, LIBEVDEV_READ_FLAG_NORMAL,
                                     &ev);
        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            if (is_mouse)
                handle_mouse_event(input, device, &ev);
            else
                handle_keyboard_event(input, &ev);
            continue;
        }
        if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            drain_sync_events(input, device, is_mouse);
            continue;
        }
        if (rc == -EAGAIN) return TRUE;
        if (input->config.debug_input) {
            g_printerr("seekey: cannot read %s: %s\n", device->path,
                       g_strerror(-rc));
        }
        return FALSE;
    }
}

static void drain_input_watch(int fd)
{
    char buffer[4096];
    while (read(fd, buffer, sizeof(buffer)) > 0) {
    }
}

static gpointer input_thread(gpointer data)
{
    SeekeyInput *input = data;
    gint64 next_periodic_scan =
        g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;

    while (TRUE) {
        g_mutex_lock(&input->lock);
        gboolean stop = input->stop;
        g_mutex_unlock(&input->lock);
        if (stop) break;

        guint device_count = 0;
        for (guint i = 0; i < input->devices->len; i++) {
            InputDevice *device = g_ptr_array_index(input->devices, i);
            if (device->active) device_count++;
        }
        for (guint i = 0; i < input->mouse_devices->len; i++) {
            InputDevice *device = g_ptr_array_index(input->mouse_devices, i);
            if (device->active) device_count++;
        }

        guint total = device_count + (input->watch_fd >= 0 ? 1 : 0);
        struct pollfd *fds = g_new0(struct pollfd, total);
        InputDevice **polled_devices = g_new0(InputDevice *, device_count);
        gboolean *mouse_flags = g_new0(gboolean, device_count);
        guint index = 0;
        for (guint i = 0; i < input->devices->len; i++) {
            InputDevice *device = g_ptr_array_index(input->devices, i);
            if (!device->active) continue;
            fds[index] = (struct pollfd){.fd = device->fd, .events = POLLIN};
            polled_devices[index] = device;
            index++;
        }
        for (guint i = 0; i < input->mouse_devices->len; i++) {
            InputDevice *device = g_ptr_array_index(input->mouse_devices, i);
            if (!device->active) continue;
            fds[index] = (struct pollfd){.fd = device->fd, .events = POLLIN};
            polled_devices[index] = device;
            mouse_flags[index] = TRUE;
            index++;
        }
        guint watch_index = device_count;
        if (input->watch_fd >= 0) {
            fds[watch_index] = (struct pollfd){
                .fd = input->watch_fd, .events = POLLIN};
        }

        int rc = poll(fds, (nfds_t)total, 250);
        gboolean needs_rescan =
            g_get_monotonic_time() >= next_periodic_scan;
        gboolean keyboard_state_dirty = FALSE;
        gboolean watch_failed = FALSE;

        if (rc > 0) {
            for (guint i = 0; i < device_count; i++) {
                InputDevice *device = polled_devices[i];
                gboolean is_mouse = mouse_flags[i];
                if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    if (input->config.debug_input) {
                        g_printerr("seekey: input device disconnected: %s\n",
                                   device->path);
                    }
                    device->active = FALSE;
                    if (!is_mouse) keyboard_state_dirty = TRUE;
                    needs_rescan = TRUE;
                    continue;
                }
                if ((fds[i].revents & POLLIN) &&
                    !read_device_events(input, device, is_mouse)) {
                    device->active = FALSE;
                    if (!is_mouse) keyboard_state_dirty = TRUE;
                    needs_rescan = TRUE;
                }
            }

            if (input->watch_fd >= 0 && fds[watch_index].revents != 0) {
                if (fds[watch_index].revents & POLLIN) {
                    drain_input_watch(input->watch_fd);
                    needs_rescan = TRUE;
                }
                if (fds[watch_index].revents &
                    (POLLERR | POLLHUP | POLLNVAL)) {
                    watch_failed = TRUE;
                    needs_rescan = TRUE;
                }
            }
        }

        g_free(mouse_flags);
        g_free(polled_devices);
        g_free(fds);

        if (watch_failed) {
            close(input->watch_fd);
            input->watch_fd = -1;
        }
        if (needs_rescan) {
            gboolean keyboard_devices_changed = scan_input_devices(input);
            if (keyboard_state_dirty || keyboard_devices_changed) {
                rebuild_keyboard_state(input);
            }
            next_periodic_scan =
                g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
        }
    }
    return NULL;
}

SeekeyInput *seekey_input_new(const SeekeyConfig *config,
                              SeekeyKeyCallback callback,
                              gpointer user_data,
                              GError **error)
{
    SeekeyInput *input = g_new0(SeekeyInput, 1);
    input->config = *config;
    input->watch_fd = -1;
    input->devices = g_ptr_array_new_with_free_func(input_device_free);
    input->mouse_devices = g_ptr_array_new_with_free_func(input_device_free);
    input->callback = callback;
    input->user_data = user_data;
    g_mutex_init(&input->lock);

    input->runtime_lock =
        seekey_runtime_lock_acquire("seekey-input.lock", error);
    if (input->runtime_lock == NULL) {
        seekey_input_free(input);
        return NULL;
    }

    scan_input_devices(input);

    if (input->devices->len == 0) {
        g_set_error(error,
                    G_IO_ERROR,
                    G_IO_ERROR_PERMISSION_DENIED,
                    "No readable keyboard devices found under /dev/input/event*");
        seekey_input_free(input);
        return NULL;
    }

    setup_input_watch(input);

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
    if (input->watch_fd >= 0) close(input->watch_fd);
    g_ptr_array_free(input->devices, TRUE);
    if (input->mouse_devices != NULL)
        g_ptr_array_free(input->mouse_devices, TRUE);
    seekey_runtime_lock_free(input->runtime_lock);
    g_mutex_clear(&input->lock);
    g_free(input);
}
