# Seekey

Seekey is a small Wayland key-visualizer prototype. It shows recent keyboard
operations in a compact transparent overlay window. Shortcut keys appear as
individual bubbles, while normal typing is grouped into short text bubbles.
When typing pauses briefly, the next typed character starts a new bubble.

## Compatibility model

Wayland compositors do not expose a universal global keyboard hook. Seekey uses
Linux evdev devices through `libevdev`, so it works across niri, Hyprland,
GNOME, KDE Plasma, Sway, River, and other compositors as long as the user has
permission to read `/dev/input/event*`.

For the overlay window:

- niri / Hyprland / wlroots compositors: if `gtk4-layer-shell` is installed,
  Seekey uses layer-shell near the bottom edge. The bubble row alignment is
  controlled by `style.align`, defaulting to a fixed right boundary. Use
  `layer-shell=required` when you want Seekey to fail instead of falling back
  to a workspace-bound normal window.
- GNOME / KDE Plasma: layer-shell is usually unavailable, so Seekey falls back
  to a normal undecorated transparent GTK4 window. Pinning it above other
  windows may require a compositor/window-rule setting because standard Wayland
  intentionally limits client-side always-on-top control.

  When running in fallback mode, Seekey prints a notice to stderr listing the
  settings that have no effect (see [Window position](#window-position)).
  The summary is:

  > The following settings have NO EFFECT in fallback mode:
  >   - `style.align` (left/center/right) — no edge anchoring
  >   - `general.margin` / `margin-horizontal` — no edge anchoring
  >   - saved window position — compositor controls placement

  To enable the full feature set on these compositors, install
  `gtk4-layer-shell` (if the compositor supports wlr-layer-shell) or switch
  to a layer-shell compositor such as niri, Hyprland, Sway, or river.

## Quick start

```sh
# Build
make

# Optional: drop a starter config in the current directory
./seekey --init-config

# Edit interactively (TUI)
./seekey --config-tui

# Run
./seekey
```

## Configuration

Seekey reads a single INI file. The lookup order is:

1. `--config <path>` (explicit, highest priority)
2. `<cwd>/seekeyini` (project-local file)
3. Built-in defaults (no file is read or written)

This means the configuration lives next to the binary you are running, which
keeps each project independent and avoids cluttering `~/.config/`. There is no
implicit XDG fallback; if you want a global config, point `--config` at it
or symlink `./seekey.ini` to one.

A complete sample with all keys and explanations is at
[seekey.ini.example](seekey.ini.example). You can generate a defaults-only copy
with:

```sh
./seekey --init-config
./seekey --init-config --force   # overwrite an existing file
```

`--init-config` writes to `./seekey.ini` by default. Use `--init-config --xdg`
once to bootstrap a config at `~/.config/seekey/config.ini` and then symlink
or pass `--config` to it.

### Useful configuration commands

```sh
./seekey --print-config         # show the effective configuration and exit
./seekey --validate-config      # validate the loaded configuration
./seekey --init-config          # create ./seekey.ini with current settings
```

`--print-config` shows where the configuration came from:

```ini
# source: project
# path: /home/you/project/seekey.ini
[general]
...
```

## Matugen integration

[Matugen](https://github.com/InioX/matugen) generates a Material You color
palette from a single wallpaper image. Seekey can pick up those colors at
startup by referencing them in `seekey.ini`:

```ini
[style]
foreground=@matugen:on_surface
background=@matugen:surface@0.86
border-color=@matugen:outline
placeholder-foreground=@matugen:on_surface@0.74
```

The `@matugen:<role>` syntax is resolved to the hex value from matugen's
`colors.json`. The optional `@<float>` suffix wraps the result in
`alpha(<hex>, <float>)` for GTK CSS:

```text
@matugen:surface         → #1a1a1a
@matugen:surface@0.86    → alpha(#1a1a1a, 0.86)
@matugen:missing_key     → @matugen:missing_key   (kept verbatim, see warning)
```

Seekey looks for the matugen JSON in this order:

1. `--matugen <path>` on the command line
2. `$MATUGEN_COLORS` environment variable
3. `$XDG_CACHE_HOME/matugen/colors.json` (default matugen output)
4. `~/.cache/matugen/colors.json` (fallback)

A minimal matugen config that writes the JSON to the default location looks
like this (`~/.config/matugen/config.toml`):

```toml
[config]
reload_apps = false

[palettes.colors]
# This section is unused unless you use the 'colors' template; we
# use the built-in Material You scheme in the templates block.
```

```toml
[[templates]]
input_path  = "/dev/stdin"
output_path = "~/.cache/matugen/colors.json"
```

```text
{ "colors": { "{{colors.primary}}": "{{colors.primary}}", ... } }
```

In practice, matugen's stock output already produces a JSON file in
`~/.cache/matugen/colors.json` containing a `colors` object with role names
like `primary`, `on_primary`, `surface`, `on_surface`, `outline`, etc. — seekey
reads this object directly.

**Reload after wallpaper change:** changing your wallpaper and re-running
`matugen` updates the JSON file, but seekey does not auto-watch it. Restart
`seekey` (or run `pkill seekey && seekey &`) to apply the new palette.

If the JSON cannot be loaded (file missing or malformed), seekey prints a
single warning to stderr and uses whatever values are in `seekey.ini`
verbatim. This makes the matugen integration fully optional.

## TUI editor

`./seekey --config-tui` opens a terminal UI to browse and edit every setting.
The top bar shows the path to the active config file and an `[Unsaved]`
indicator when there are uncommitted changes.

### Key bindings

| Key | Action |
|---|---|
| `Up`/`Down` or `k`/`j` | Select field |
| `Left`/`Right` or `h`/`l` | Adjust numeric/choice/color/bool value |
| `Enter` | Edit (numbers/strings/colors) or open picker (choice/theme/color) |
| `s` | Save to current path |
| `S` | Save as a new path |
| `r` | Reset the current field to its default |
| `R` | Reset ALL fields to defaults (confirms first) |
| `L` | Reload from disk (discards pending changes, confirms first) |
| `?` | Show full help |
| `q` / `Esc` | Quit (asks to save if there are unsaved changes) |

For `TUI_CHOICE` fields (e.g. `align`, `disappear`, `layer-shell`, `theme`),
pressing `Enter` opens a list picker. You can no longer end up in a text
input mode that requires you to type the option name from memory.

For color fields, pressing `Enter` opens a 24-color palette. Press `c` to
enter a custom `#rrggbb` value (input is validated; invalid hex is
rejected with an error message, no silent loss of your value).

## Window position

On layer-shell compositors (niri, Hyprland, Sway, river, ...), the
overlay's position is fully determined by the configuration:

- `style.align` — `left` / `center` / `right` choose the horizontal edge
- `general.margin` — bottom margin in pixels
- `general.margin-horizontal` — left/right margin in pixels (ignored when `align=center`)

These three keys give a fixed, reproducible position on a given monitor
(anchor + offset is deterministic). On a single-monitor setup you
already get a stable position out of the box.

### Multi-monitor

On a multi-monitor setup, Seekey remembers which monitor it was on the
last time it ran and opens on that monitor next launch:

- On shutdown, Seekey writes the current monitor's connector name (e.g.
  `HDMI-A-1`, `DP-1`, `eDP-1`) to
  `$XDG_STATE_HOME/seekey/window.ini` (fallback `~/.local/state/seekey/window.ini`).
- On startup, Seekey reads that file and uses
  [`gtk_layer_set_monitor`](https://github.com/wmww/gtk4-layer-shell)
  to pin the overlay to the same monitor.
- If the saved monitor is no longer connected (cable unplugged, dock
  removed, etc.), Seekey silently falls back to the focused output and
  the next shutdown updates the saved state.

To forget the saved monitor, run `rm ~/.local/state/seekey/window.ini`
or press `W` in the TUI.

### Fallback mode (no layer-shell)

If layer-shell is unavailable (GNOME, KDE, missing `gtk4-layer-shell`),
Seekey prints a notice to stderr explaining which settings have no
effect:

```
seekey: using fallback window: <reason>
seekey: NOTE: you are not running under a wlr-layer-shell compositor.
seekey:       The following settings have NO EFFECT in fallback mode:
seekey:         - style.align / margin / margin-horizontal (no edge anchoring)
seekey:         - saved window position (compositor controls placement)
seekey:       To enable them, use a layer-shell compositor (niri, Hyprland,
seekey:       Sway, river) or install gtk4-layer-shell on a supported system.
```

On the X11 fallback path, `gtk_window_move` and the saved position
would normally restore a window to its previous spot, but on pure
Wayland the compositor decides placement by design. The state file is
still written so an X session (if you ever log into one) restores the
position correctly.

## Input permissions

If Seekey prints `No readable keyboard devices found`, your user cannot read
input devices. On many distributions the practical setup is:

```sh
sudo usermod -aG input "$USER"
```

Then log out and log back in. Some systems do not use an `input` group by
default; in that case configure a local udev rule for the keyboard devices.

Running as root is useful for a quick test, but not recommended for daily use.

## Autostart

The install script does **not** install an autostart entry — different
Wayland compositors have different startup mechanisms and the right choice
depends on your workflow. Add seekey to the compositor of your choice:

### GNOME / KDE Plasma / Cinnamon / XFCE (XDG autostart)
Create `~/.config/autostart/seekey.desktop`:
```ini
[Desktop Entry]
Type=Application
Name=Seekey
Comment=Keyboard visualizer overlay for Wayland
Exec=seekey
Terminal=false
Categories=Utility;
X-GNOME-Autostart-enabled=true
```

### Hyprland
Add to `~/.config/hypr/hyprland.conf`:
```ini
exec-once = seekey
```

### Sway
Add to `~/.config/sway/config`:
```
exec seekey
```

### niri
Add to `~/.config/niri/config.kdl`:
```kdl
spawn-at-startup "seekey"
```

### river
Add to `~/.config/river/init`:
```sh
riverctl spawn seekey
```

### systemd user service (compositor-agnostic)
Create `~/.config/systemd/user/seekey.service`:
```ini
[Unit]
Description=Seekey keyboard visualizer
After=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/local/bin/seekey
Restart=on-failure

[Install]
WantedBy=default.target
```
Then enable it:
```sh
systemctl --user enable --now seekey.service
```

> Tip: `~/.local/bin` (the default `install.sh` prefix) is not on `$PATH`
> in some login sessions. Use the absolute path (e.g. `ExecStart=%h/.local/bin/seekey`)
> in the systemd unit, or add `~/.local/bin` to your shell rc.

## Build

Install dependencies:

```sh
# Arch
sudo pacman -S gtk4 libevdev gcc pkgconf

# Fedora
sudo dnf install gtk4-devel libevdev-devel gcc pkgconf

# Debian / Ubuntu
sudo apt install libgtk-4-dev libevdev-dev gcc pkg-config
```

Optional for niri/Hyprland/wlroots layer mode:

```sh
# Arch
sudo pacman -S gtk4-layer-shell

# Fedora
sudo dnf install gtk4-layer-shell-devel
```

Then build:

```sh
make
```

Other Make targets:

```sh
make            # build seekey
make check      # run unit tests (Unity)
make install    # install to PREFIX (default /usr/local)
make uninstall  # remove installed files
make format     # clang-format sources if available
make clean      # remove build artifacts
```

## Options

```text
Usage: seekey [OPTIONS]

Options:
  --config PATH          Config file path (default: ./seekey.ini)
  --config-tui           Open terminal UI to edit and save configuration
  --init-config          Write the current default/config/CLI settings to config
  --init-config --xdg    Write to ~/.config/seekey/config.ini instead
  --force                Allow --init-config to overwrite an existing config
  --print-config         Print the effective configuration and exit
  --validate-config      Validate configuration and exit
  --no-layer-shell       Disable gtk4-layer-shell even if available
  --layer-shell auto|required|off
  --theme NAME           Color preset: default, nord, dracula, catppuccin, monokai, light
  --merge-repeats        Stack identical key bubbles as Key xN (default on)
  --no-merge-repeats     Show each key press as a separate bubble
  --merge-modifiers      Update modifier bubble when combo extends (default on)
  --no-merge-modifiers   Keep each modifier press as a separate bubble
  --show-mouse           Show mouse clicks (default off)
  --no-mouse             Hide mouse clicks (default)
  --duration MS          How long each key bubble remains visible (default: 1200)
  --typing-idle MS       Pause before typed text starts a new bubble (default: 650)
  --fade-ms MS           Fade duration when disappear=fade (default: 180)
  --margin PX            Bottom margin in layer-shell mode (default: 96)
  --margin-horizontal PX Horizontal margin for left/right anchor (default: 0)
  --max-items N          Maximum bubbles visible at once (default: 5)
  --align left|center|right
  --disappear instant|fade
  --debug-input          Print device discovery and events to stderr
  -V, --version          Print version and exit
  -h, --help             Show help
```

## Configuration reference

The most common settings (the ones most users will touch) are at the top of
each section. The advanced settings below are kept for fine-tuning the look
and feel.

```ini
[general]
duration-ms=1200
typing-idle-ms=650
fade-ms=180
margin=96
margin-horizontal=0
max-items=5
layer-shell=auto
theme=default
merge-repeats=true
merge-modifiers=true
show-mouse=false

[style]
align=right
disappear=fade
spacing=7
overlay-padding=12
key-min-width=0
key-padding-x=14
key-padding-y=8
key-radius=6
key-border-width=1
key-font-px=20
key-font-weight=700
typing-max-width=480
window-width=720
window-height=160
foreground=#f7f7f2
background=alpha(#111318, 0.86)
border-color=alpha(#ffffff, 0.18)
shadow=0 7px 22px alpha(#000000, 0.30)
placeholder-text=Seekey
placeholder-foreground=alpha(#f7f7f2, 0.74)
placeholder-background=alpha(#111318, 0.56)
placeholder-border-color=alpha(#ffffff, 0.14)

[icons]
Backspace="⌫"
Enter="↵"
Space="␣"
Tab="↹"
Esc="⎋"
Delete="⌦"
Up="↑"
Down="↓"
Left="←"
Right="→"
```

`align=right` is the default. With layer-shell, the window anchors to the
bottom-right corner of the screen (bottom-left for `left`, full-width bottom
for `center`). Older bubbles move away from the anchored edge.
`disappear=instant` removes bubbles directly; `disappear=fade` uses
`fade-ms` and the `.fading` CSS state.

## Current scope

- compositor-independent input capture through evdev (`libevdev`)
- mouse button & scroll-wheel display (`⬤ ◉ ◎ ▲ ▼ ◀ ▶`)
- wlroots/niri/Hyprland layer-shell overlay, GNOME/KDE fallback window
- compositor auto-detection via `XDG_CURRENT_DESKTOP`
- compact key-combination rendering with configurable horizontal alignment
- modifier-key progressive display (`Ctrl` → `Ctrl + Shift` → `Ctrl + Shift + X`)
- consecutive identical shortcut bubbles merged as `Key xN` (toggleable)
- typed-text aggregation for letters, digits, punctuation, and spaces
- special-key icons (`⌫ ↵ ␣ ↹ ⎋ ⌦ ↑ ↓ ← →`), user-configurable
- 6 built-in colour themes (`default`, `nord`, `dracula`, `catppuccin`,
  `monokai`, `light`)
- TUI config editor with colour palette picker, theme selector, choice
  pickers, save/reset/reload actions, and a help overlay
- fade-out animation or instant bubble removal
- behaviour flags: `merge-repeats`, `merge-modifiers`, `show-mouse`
- startup placeholder bubble for easier fallback-window placement
- project-local configuration (`./seekey.ini`) for clean per-project setups
- unit tests with Unity (`make check`)

Future work should add per-monitor placement, compositor-specific install
snippets, and packaging.

## Project layout

```
src/
  seekey.h     public types
  main.c       GTK application, event loop, bubble logic
  config.c/h   config load/save/parse/theme
  tui.c/h      TUI editor
  input.c      evdev key/mouse capture
  keynames.c   key name + icon lookup
  layer_shell.c gtk4-layer-shell integration
tests/
  test_config.c  config roundtrip, validation, theme
  test_tui.c     TUI field/adjust/reset (pure logic)
  test_keynames.c key/icon/modifier tests
  vendor/unity/  vendored Unity test framework
seekey.ini.example  sample configuration
```
