# Architecture

A tour of how the source is laid out and how data flows through it. Read this
first if the code confuses you.

## One-line summary

Seekey reads raw keyboard events from the Linux kernel (`/dev/input/event*`
via `libevdev`) in a background thread, marshals each keypress to the GTK
main thread, and draws the last few keys as CSS-styled bubbles in a
click-through overlay window anchored to the bottom of the screen.

## Project layout

```
seekey/
├── LICENSE                    MIT
├── README.md / README.zh-CN.md
├── Makefile
├── install.sh                 user-level installer + deps detection
├── seekey.ini.example         sample configuration with comments
├── src/
│   ├── seekey.h               shared types + public input/keyname API
│   ├── main.c                 GTK app, event loop, bubble logic
│   ├── config.c / .h          config load/save/parse, themes, matugen
│   ├── tui.c / .h             TUI editor (ncurses)
│   ├── window_state.c / .h    monitor persistence (XDG_STATE_HOME)
│   ├── input.c                evdev key/mouse capture (background thread)
│   ├── keynames.c             key name + icon lookup tables
│   └── layer_shell.c          gtk4-layer-shell integration (dlopen'd)
├── tests/
│   ├── test_main.c            test runner entry
│   ├── test_config.c
│   ├── test_tui.c
│   ├── test_keynames.c
│   ├── test_window_state.c
│   ├── test_helpers.c / .h
│   └── vendor/unity/          vendored Unity test framework (MIT)
├── po/                        gettext translations
│   ├── POTFILES.in  LINGUAS  seekey.pot  zh-CN.po
└── locale/                    compiled .mo files (build output)
```

## The modules and what they do

### `src/seekey.h` — shared types
The central header. Defines `SeekeyConfig` (every setting as a plain struct
field), `KeyEventMessage` (one keypress handed from input to UI), modifier
bitmask flags, scroll pseudo key-codes, and the public API for
`input.c` / `keynames.c` / `layer_shell.c`. Included by everything.

### `src/config.c` / `config.h` — configuration
- `seekey_config_set_defaults` — fills `SeekeyConfig` with hardcoded defaults.
- Theme presets table (`default`, `light`, `nord`, `dracula`, `catppuccin`,
  `monokai`) + `seekey_config_apply_theme`.
- `seekey_config_resolve_path` — picks the config file
  (`--config` → `<cwd>/seekey.ini` → none).
- `seekey_config_load` — reads the INI with `GKeyFile`, clamps numbers to
  ranges, applies theme then per-key color overrides, reads `[icons]`.
- `seekey_config_save` / `seekey_config_init` / `seekey_config_print` /
  `seekey_config_validate`.
- `seekey_parse_args` — CLI flags override config fields.
- Matugen helpers: `seekey_matugen_resolve_path`, `seekey_matugen_load`
  (parses `colors.json` into a `GHashTable`), `seekey_matugen_resolve_value`
  (turns `@matugen:role@0.86` into `alpha(#hex, 0.86)`).

### `src/input.c` — evdev capture
The input side. `seekey_input_new` opens **all** `/dev/input/event*` devices
with `libevdev`, keeping the ones that look like keyboards (have a useful set
of keys) and, if `show-mouse=true`, mouse-like devices. It spawns a
**background thread** running a `poll(2)` loop across all fds. When a key
event arrives it builds a `KeyEventMessage`, tracks modifier state and
shifted/non-shift-modifier flags, and pushes it to the GTK main thread via
`g_idle_add` (`dispatch_key_event` → the callback). This is why seekey works
on any compositor: it never touches the Wayland keyboard protocol.

### `src/keynames.c` — key labels
Static tables mapping evdev key codes (`KEY_*` from
`linux/input-event-codes.h`) to human labels (`Backspace`, `Enter`, `Up`,
`Volume Up`, …). Provides `seekey_key_name`, `seekey_key_text` (the typed
character for a key, shifted or not), `seekey_key_icon` (honoring `[icons]`
overrides), `seekey_is_modifier`, and `seekey_modifier_order` (the canonical
display order of Ctrl/Shift/Alt/Super in a combo).

### `src/layer_shell.c` — overlay anchoring
Uses `gtk4-layer-shell` **loaded at runtime with `dlopen`** (not linked
hard), probing `libgtk4-layer-shell.so.0` then `libgtk4-layer-shell.so`.
`seekey_layer_shell_try_init` initializes the window for layer-shell, sets
the layer (top), anchors it to the bottom edge, applies margins, sets the
monitor, sets keyboard mode to `NONE` (so seekey never grabs the keyboard),
and a namespace. If the library is missing or `layer-shell=off`, it returns
an error and `main.c` falls back to a normal transparent window.

> Why `dlopen` and why the Makefile links it before GTK4? gtk4-layer-shell
> registers a static constructor that must run before libwayland-client is
> initialized. When linked, the Makefile puts it before GTK4; when not
> linked, the runtime `dlopen` path still works for distros that ship the
> library but not a `.pc` file.

### `src/window_state.c` / `window_state.h` — monitor memory
Persists which monitor the overlay was on, in
`$XDG_STATE_HOME/seekey/window.ini` (fallback `~/.local/state/seekey/...`).
`seekey_window_state_load` (missing/malformed files are not errors — returns
zeroed state), `seekey_window_state_save` (writes the connector name, e.g.
`DP-1`), `seekey_window_state_clear`, and `seekey_find_monitor_by_name`
(matches a connector against `GdkDisplay`'s monitors).

### `src/tui.c` / `tui.h` — terminal editor
The `--config-tui` editor (ncurses). Builds a `TuiField` array (33 fields)
describing each setting — type (`TUI_UINT` / `TUI_STRING` / `TUI_CHOICE` /
`TUI_BOOL` / `TUI_COLOR`), target pointer, min/max/step, default. The
**pure helper** functions (`tui_field_value`, `tui_adjust_field`,
`tui_reset_field`, `tui_nearest_color_index`, `tui_current_choice_index`)
are unit-tested without ncurses. The ncurses rendering code is gated by
`#ifdef SEEKEY_TEST` so the test build skips it.

### `src/main.c` — the app
The biggest file. Responsibilities:
- `main()`: setlocale, bind gettext, set defaults, resolve+load config,
  parse CLI args, handle `--init-config`/`--print-config`/`--validate-config`/
  `--config-tui` (then exit), else create `GtkApplication`.
- `activate()`: build the window + a horizontal `GtkBox` for bubbles, set the
  empty input region (click-through), try layer-shell (else fallback window),
  load saved monitor state, start `input.c`.
- **Bubble logic**: `on_key_event` (the callback from `input.c`) decides
  whether to merge into the last bubble (`merge-repeats`, `merge-modifiers`),
  start/extend a typing group, or create a new bubble. Each bubble schedules
  a `duration-ms` timeout; if `disappear=fade`, a two-phase removal adds the
  `fading` CSS class then removes the widget after `fade-ms`. `trim_bubbles`
  enforces `max-items`.
- `shutdown()`: persist the current monitor connector to `window_state`, free
  the input thread.

## Data flow (one keypress)

```
/dev/input/event*  ──libevdev──▶  input.c  poll thread
                                      │ builds KeyEventMessage
                                      │ g_idle_add(dispatch_key_event)
                                      ▼
                          main.c  on_key_event  (GTK main thread)
                                      │ merge? typing group? new bubble?
                                      ▼
                        GtkBox  ──CSS──▶  screen bubbles
                                      │ after duration-ms (+ fade-ms)
                                      ▼
                                   remove / fade out
```

## Key design choices

- **Compositor-independent input.** Reading evdev directly means seekey works
  on every Wayland compositor with no per-compositor protocol code. The
  trade-off: it needs read access to `/dev/input/event*` (see
  [[Troubleshooting]]).
- **Runtime `dlopen` for layer-shell.** Lets one binary work on both
  layer-shell and non-layer-shell desktops; the library is optional.
- **Thread → main-thread marshaling.** evdev polling blocks, so it lives in
  a thread; all GTK calls happen on the main thread via `g_idle_add`.
- **Click-through via empty input region** on the `GdkSurface`, plus
  `KEYBOARD_MODE_NONE` so keyboard focus is never stolen.
- **Config as a plain struct.** No accessors/getters; `SeekeyConfig` fields
  are read directly. Simple, and the TUI's `TuiField` targets point right at
  them.

See [[Testing]] for how the pure logic is tested without a display.
