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
  controlled by `style.align`, defaulting to a fixed right boundary.
- GNOME / KDE Plasma: layer-shell is usually unavailable, so Seekey falls back
  to a normal undecorated transparent GTK4 window. Pinning it above other
  windows may require a compositor/window-rule setting because standard Wayland
  intentionally limits client-side always-on-top control.

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

Run:

```sh
./seekey
```

Seekey reads configuration from `~/.config/seekey/config.ini` by default. A
sample file is available at [examples/config.ini](examples/config.ini).

## Input permissions

If Seekey prints `No readable keyboard devices found`, your user cannot read
input devices. On many distributions the practical setup is:

```sh
sudo usermod -aG input "$USER"
```

Then log out and log back in. Some systems do not use an `input` group by
default; in that case configure a local udev rule for the keyboard devices.

Running as root is useful for a quick test, but not recommended for daily use.

## Options

```text
Usage: seekey [OPTIONS]

Options:
  --config PATH          Config file path (default: ~/.config/seekey/config.ini)
  --no-layer-shell       Disable gtk4-layer-shell even if available
  --duration MS          How long each key bubble remains visible (default: 1200)
  --typing-idle MS       Pause before typed text starts a new bubble (default: 650)
  --fade-ms MS           Fade duration when disappear=fade (default: 180)
  --margin PX            Bottom margin in layer-shell mode (default: 96)
  --max-items N          Maximum bubbles visible at once (default: 5)
  --align left|center|right
  --disappear instant|fade
  --debug-input          Print device discovery and key events to stderr
  -h, --help             Show help
```

## Configuration

Create `~/.config/seekey/config.ini`:

```ini
[general]
duration-ms=1200
typing-idle-ms=650
fade-ms=180
margin=96
max-items=5
window-width=720
window-height=160

[style]
align=right
disappear=fade
spacing=7
overlay-padding=12
key-min-width=58
key-padding-x=14
key-padding-y=8
key-radius=6
key-border-width=1
key-font-px=20
key-font-weight=700
typing-max-width=480
foreground=#f7f7f2
background=alpha(#111318, 0.86)
border-color=alpha(#ffffff, 0.18)
shadow=0 7px 22px alpha(#000000, 0.30)
placeholder-text=Seekey
placeholder-foreground=alpha(#f7f7f2, 0.74)
placeholder-background=alpha(#111318, 0.56)
placeholder-border-color=alpha(#ffffff, 0.14)
```

`align=right` is the default, so new bubbles start from a fixed right boundary
and older bubbles move left. `align=center` and `align=left` are available for
other layouts. `disappear=instant` removes bubbles directly; `disappear=fade`
uses `fade-ms` and the `.fading` CSS state.

## Current scope

This prototype focuses on the core Wayland-safe architecture:

- compositor-independent input capture through evdev
- wlroots/niri/Hyprland layer-shell overlay when available
- GNOME/KDE fallback window
- compact key-combination rendering with configurable horizontal alignment
- basic typed-text aggregation for letters, digits, punctuation, and spaces
- startup placeholder bubble for easier fallback-window placement

Future work should add pointer button display, per-monitor placement,
compositor-specific install snippets, and packaging.
