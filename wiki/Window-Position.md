# Window Position

How seekey decides where to appear, how it remembers your monitor, how
click-through works, and how to pin the window on GNOME / KDE.

## Layer-shell: fixed by config

On layer-shell compositors (niri / Hyprland / Sway / river / Wayfire / labwc),
position is fully determined by three config keys:

- `style.align` — `left` / `center` / `right` chooses the horizontal edge
- `general.margin` — bottom margin in pixels
- `general.margin-horizontal` — left/right margin (ignored when `align=center`)

```ini
[style]
align=right
[general]
margin=96
margin-horizontal=0
```

These give a reproducible position on a given monitor. On a single-monitor
setup you get a stable position out of the box. See [[Compositor Compatibility]]
for which compositors support layer-shell.

## Multi-monitor: persisted across sessions

On a multi-monitor setup, seekey remembers which monitor it was on:

- On startup, seekey reads `$XDG_STATE_HOME/seekey/window.ini` (fallback
  `~/.local/state/seekey/window.ini`) and pins the overlay to that monitor
  via `gtk_layer_set_monitor`.
- On shutdown (and after the first surface map, so even a hard-killed process
  leaves state), it writes the current monitor's connector name (e.g.
  `HDMI-A-1`, `DP-1`, `eDP-1`).
- If the saved monitor is no longer connected, seekey silently falls back to
  the focused output, and the next save updates the state.

To forget the saved monitor:

```sh
rm ~/.local/state/seekey/window.ini
```

…or press `W` in the TUI editor (`--config-tui`).

## Click-through

Seekey is purely a visual overlay. The window sets an **empty input region**
on its `GdkSurface`, so all pointer events pass through to whatever is behind
it:

- Click the dock / taskbar / window controls underneath the bubbles — nothing
  intercepts the click.
- Mouse hover, focus, and drag of underlying apps are unaffected.
- Keyboard input is independent: seekey reads it via `libevdev` and never
  claims keyboard focus (the layer-shell keyboard mode is
  `KEYBOARD_MODE_NONE`).

This works the same in layer-shell mode and in fallback mode.

## Pinning the window on GNOME / KDE (fallback mode)

Seekey sets a stable app id of `dev.seekey` so window rules can target it by
name. (On KDE/KWin the class is rendered as `dev_seekey`.)

### KDE Plasma (KWin)

Create `~/.config/kwinrulesrc`:

```ini
[General]
count=1
rules=1

[1]
Description=seekey
above=true
position=1920,1040
positionrule=Force
wmclass=dev_seekey dev_seekey
wmclasscomplete=true
```

Reload KWin's rules:

```sh
qdbus org.kde.KWin /KWin reconfigure
```

(or log out and back in). Find your target monitor's resolution with
`kscreen-doctor -o` and set `position` so the window lands where you want.

### GNOME (Mutter)

GNOME has no first-class per-app window-rule editor in the shell UI. Two
practical paths:

1. **Extension — "Auto Move Windows"** (recommended). Install from
   <https://extensions.gnome.org>, add a rule with app id `dev.seekey`, pick
   the position + monitor, done.
2. **`dconf` direct edit** (advanced, limited):
   ```sh
   dconf write /org/gnome/mutter/wayland/x-window-rules \
     "['seekey:app-id:dev.seekey']"
   ```

### Sway / Hyprland (forcing fallback on a layer-shell compositor)

If you set `layer-shell=off` on a wlroots compositor (e.g. for debugging),
pin the fallback window with the compositor's own rules:

```ini
# sway config
for_window [app_id="dev.seekey"] floating enable
for_window [app_id="dev.seekey"] sticky enable
```

```ini
# hyprland.conf
windowrulev2 = float, class:^(dev\.seekey)$
windowrulev2 = keep-floating, class:^(dev\.seekey)$
```
