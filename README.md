<div align="center">

# ⌨️ Seekey

**A tiny Wayland keyboard visualizer.**

Shows your last keystrokes as floating bubbles anchored to the bottom of the
screen — see shortcuts, modifiers, and typed text at a glance, without
breaking your flow.

![image-20260627131530605](https://img.nkns.cc/2026/06/0de85ce2c0f23714d6743a3e4696fbca.png)

```
   ┌──────────┐  ┌──────────────┐  ┌───────────────┐
   │ Ctrl + C │  │ Ctrl + C x3  │  │  hello world  │
   └──────────┘  └──────────────┘  └───────────────┘
```

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Language: C](https://img.shields.io/badge/Language-C-00599C.svg)]
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux%20%2F%20Wayland-FCC624.svg)]
[![GTK4](https://img.shields.io/badge/GTK-4-7FE7FE.svg)]
[![Version](https://img.shields.io/badge/version-0.2.0-44cc11.svg)]

**[English](README.md)** · **[简体中文](README.zh-CN.md)** · **[📖 Wiki](https://github.com/Nakanomk/Seekey/wiki)**

</div>

---

## ✨ What is this?

Seekey is a keyboard visualizer for Linux Wayland. It listens to your
keyboard (and optionally your mouse) and draws the last few keys as little
rounded bubbles anchored to the bottom edge of the screen.

> Great for **streaming & recording**, **live tutorials**, or just catching
> your own accidental modifier presses and typos.

It is **not** a keylogger — it only displays the most recent few keys
temporarily, reads input straight from the kernel (`/dev/input/event*`), and
never claims keyboard focus. The overlay is **click-through**, so it never
gets in the way of whatever is underneath.

---

## 🚀 Quick start

```sh
make                       # build
./seekey --init-config     # drop a starter ./seekey.ini (optional)
./seekey --config-tui      # tweak settings in a terminal UI (optional)
./seekey                   # run
```

Press some keys — bubbles appear at the bottom of the screen. That's it.

> ⚠️ **No bubbles on first run?** You need read access to
> `/dev/input/event*`. The easiest fix is `./install.sh` (it sets up a udev
> rule + the `input` group for you). See
> [Troubleshooting](https://github.com/Nakanomk/Seekey/wiki/Troubleshooting).

---

## 🌟 Highlights

| | |
|---|---|
| 🖥️ **Works on every Wayland compositor** | Reads input via `libevdev`, not the Wayland keyboard protocol — no per-compositor glue needed. |
| 📌 **Anchors out of your way** | Uses `gtk4-layer-shell` on niri / Hyprland / Sway / river / Wayfire / labwc to pin to the bottom edge and stay put across workspaces. Remembers which monitor it was on. |
| 🪟 **Falls back gracefully** | On GNOME / KDE it runs as a transparent window and tells you exactly which settings have no effect. |
| 🖱️ **Click-through** | Clicks pass straight through to whatever is underneath. Keyboard focus is never stolen. |
| 🎨 **Looks nice out of the box** | Six built-in themes (default, nord, dracula, catppuccin, monokai, light) + custom colors + custom key icons. |
| 🖼️ **Matugen-aware** | Reference `@matugen:<role>` in the config and colors follow your wallpaper. |
| ⌨️ **TUI config editor** | Browse / change / save every setting from the terminal — no need to hand-edit ini files. |

---

## 📦 Install

The easiest path is the bundled installer — it detects your distro, installs
build deps, builds, installs to `~/.local/bin`, and sets up input permissions:

```sh
./install.sh                # user install (default)
./install.sh --system       # system install to /usr/local (sudo)
./install.sh --uninstall    # reverse it
```

Prefer building manually? See
[Build from source](https://github.com/Nakanomk/Seekey/wiki/Build-from-Source)
in the wiki.

---

## 📚 Where to go next

The README stays short on purpose. Everything else lives in the
**[📖 Wiki](https://github.com/Nakanomk/Seekey/wiki)**:

**Getting started**
- [Installation](https://github.com/Nakanomk/Seekey/wiki/Installation) — `install.sh` options, distro dependencies, input permissions
- [Build from Source](https://github.com/Nakanomk/Seekey/wiki/Build-from-Source) — manual build, dependencies per distro, Makefile targets
- [Configuration](https://github.com/Nakanomk/Seekey/wiki/Configuration) — the config file, lookup order, common settings
- [Configuration Reference](https://github.com/Nakanomk/Seekey/wiki/Configuration-Reference) — every key, its type, range, and default
- [TUI Editor](https://github.com/Nakanomk/Seekey/wiki/TUI-Editor) — keybindings for `--config-tui`

**Behaviour & compatibility**
- [Compositor Compatibility](https://github.com/Nakanomk/Seekey/wiki/Compositor-Compatibility) — layer-shell vs fallback per desktop
- [Window Position](https://github.com/Nakanomk/Seekey/wiki/Window-Position) — anchoring, multi-monitor, click-through, GNOME/KDE pinning
- [Autostart](https://github.com/Nakanomk/Seekey/wiki/Autostart) — start seekey with your compositor

**Looks**
- [Themes and Icons](https://github.com/Nakanomk/Seekey/wiki/Themes-and-Icons) — presets, custom colors, custom key glyphs
- [Matugen Integration](https://github.com/Nakanomk/Seekey/wiki/Matugen-Integration) — wallpaper-driven colors

**For contributors**
- [Architecture](https://github.com/Nakanomk/Seekey/wiki/Architecture) — how the source is laid out *(start here if the code confuses you)*
- [Testing](https://github.com/Nakanomk/Seekey/wiki/Testing) — running and understanding the unit tests
- [Translations](https://github.com/Nakanomk/Seekey/wiki/Translations) — adding a new language

**Reference**
- [Troubleshooting](https://github.com/Nakanomk/Seekey/wiki/Troubleshooting) — input permissions, fallback mode, no bubbles, etc.

---

<div align="center">

## 📄 License

MIT — see [LICENSE](LICENSE).

</div>
