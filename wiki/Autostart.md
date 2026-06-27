# Autostart

**Language:** **English** · [简体中文](Autostart.zh-CN)

`./install.sh` does **not** install an autostart entry — different Wayland
compositors have different startup mechanisms, and the right choice depends
on your workflow. Pick the snippet for your desktop.

## GNOME / KDE Plasma / Cinnamon / XFCE

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

## Hyprland

Add to `~/.config/hypr/hyprland.conf`:

```ini
exec-once = seekey
```

## Sway

Add to `~/.config/sway/config`:

```
exec seekey
```

## niri

Add to `~/.config/niri/config.kdl`:

```kdl
spawn-at-startup "seekey"
```

## river

Add to `~/.config/river/init`:

```sh
riverctl spawn seekey
```

## Wayfire

Add to `~/.config/wayfire.ini`:

```ini
[autostart]
seekey = seekey
```

## labwc

Add to `~/.config/labwc/autostart` (make it executable):

```sh
#!/bin/sh
seekey &
```

## systemd user service (compositor-agnostic)

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

> **Tip:** `~/.local/bin` (the default `install.sh` prefix) is not on
> `$PATH` in some login sessions. Use the absolute path in the systemd unit
> (e.g. `ExecStart=%h/.local/bin/seekey`), or add `~/.local/bin` to your
> shell rc.

## Passing config / flags at startup

Append any command-line flags (see [[Configuration#command-line-overrides]])
to `Exec`/`ExecStart`/`spawn` as needed, e.g.:

```ini
ExecStart=/usr/local/bin/seekey --config /home/you/.config/seekey/config.ini
```
