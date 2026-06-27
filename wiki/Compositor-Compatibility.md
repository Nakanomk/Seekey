# Compositor Compatibility

**Language:** **English** · [简体中文](Compositor-Compatibility.zh-CN)

Seekey reads input via `libevdev` (the kernel evdev interface), **not** the
Wayland keyboard protocol. So input works on any Linux Wayland session as
long as you can read `/dev/input/event*` (see [[Installation]]).

What changes per compositor is the **overlay**: whether seekey can anchor to
an edge and stay put across workspaces (layer-shell), or has to fall back to
a normal window.

## Compatibility table

| Compositor | layer-shell | Fallback | Notes |
|---|:---:|:---:|---|
| niri | ✓ | — | Default test target. Works out of the box. |
| Hyprland | ✓ | — | Full feature set. |
| Sway | ✓ | — | Full feature set. |
| river | ✓ | — | Full feature set. |
| Wayfire | ✓ | — | Full feature set. |
| labwc | ✓ | — | Full feature set. |
| KWinFT (KDE wlroots fork) | ✓ | — | Full feature set. |
| GNOME (mutter) | — | ✓ | No layer-shell. Pin the window — see [[Window Position]]. |
| KDE Plasma (KWin) | — | ✓ | No layer-shell. Pin via `kwinrulesrc` — see [[Window Position]]. |
| X11 session | — | — | Not supported (this is a Wayland tool). |
| macOS / Windows / BSD | — | — | `libevdev` is Linux-only. |

## `layer-shell` setting

```ini
[general]
layer-shell=auto       # use layer-shell if available, else fall back (default)
# layer-shell=required # use layer-shell and exit if unavailable
# layer-shell=off      # always use a normal undecorated window
```

Or on the command line: `--layer-shell auto|required|off`, or
`--no-layer-shell` (same as `off`).

`required` is useful if you want seekey to refuse to run rather than appear
as a floating window on a non-layer-shell desktop.

## Fallback mode (GNOME / KDE / no gtk4-layer-shell)

When layer-shell isn't available, seekey runs as a transparent, undecorated
window and prints a notice to stderr listing the settings that have no effect:

```
seekey: using fallback window: <reason>
seekey: NOTE: you are not running under a wlr-layer-shell compositor.
seekey:       The following settings have NO EFFECT in fallback mode:
seekey:         - style.align / margin / margin-horizontal (no edge anchoring)
seekey:         - saved window position (compositor controls placement)
seekey:       To enable them, use a layer-shell compositor (niri, Hyprland,
seekey:        Sway, river) or install gtk4-layer-shell on a supported system.
```

In fallback mode the compositor controls where the window goes. To pin it to
a corner / monitor, use window rules — see [[Window Position]].

## Forcing fallback (for debugging)

Even on a layer-shell compositor you can force the fallback path:

```sh
./seekey --no-layer-shell
# or set layer-shell=off in seekey.ini
```

This is handy for reproducing fallback behaviour or testing the window-rule
setup on a wlroots compositor.
