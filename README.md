<div align="center">

# ⌨️ Seekey

**A tiny Wayland keyboard visualizer.**

Shows your last keystrokes as floating bubbles anchored to the bottom of the
screen — see shortcuts, modifiers, and typed text at a glance, without
breaking your flow.

![image-20260627131530605](https://img.nkns.cc/2026/06/0de85ce2c0f23714d6743a3e4696fbca.png)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Language: C](https://img.shields.io/badge/Language-C-00599C.svg)
![Platform: Linux](https://img.shields.io/badge/Platform-Linux%20%2F%20Wayland-FCC624.svg)
![GTK4](https://img.shields.io/badge/GTK-4-7FE7FE.svg)
![Version](https://img.shields.io/badge/version-0.3.0-44cc11.svg)
<!-- ALL-CONTRIBUTORS-BADGE:START - Do not remove or modify this section -->
[![All Contributors](https://img.shields.io/badge/all_contributors-3-orange.svg?style=flat-square)](#contributors-)
<!-- ALL-CONTRIBUTORS-BADGE:END -->

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

### For Arch Linux Users

```sh
yay -S seekey             # install stable version
yay -S seekey-git         # install main branch version
```

### For Other Distro Users

```sh
make                       # build
./seekey --init-config     # drop a starter ./seekey.ini (optional)
./seekey --config-gui      # open the graphical settings menu
./seekey --config-tui      # tweak settings in a terminal UI (optional)
./seekey                   # run
```

Press some keys — bubbles appear at the bottom of the screen. That's it.

> ⚠️ **No bubbles on first run?** You need read access to
> `/dev/input/event*`. The easiest fix is `./install.sh`, which installs an
> active-session udev ACL. Systems without `uaccess` can use the explicit
> `--input-group` fallback. See
> [Troubleshooting](https://github.com/Nakanomk/Seekey/wiki/Troubleshooting).

Keyboard and mouse event nodes are watched at runtime. Reconnecting a USB
keyboard, switching a dock, or replacing an input device does not require
restarting Seekey. If no readable keyboard exists at launch, the placeholder
stays visible while Seekey retries after hotplug or permission changes.

---

## 🌟 Highlights

| | |
|---|---|
| 🖥️ **Works on every Wayland compositor** | Reads input via `libevdev`, not the Wayland keyboard protocol — no per-compositor glue needed. |
| 📌 **Anchors out of your way** | Uses `gtk4-layer-shell` on niri / Hyprland / Sway / river / Wayfire / labwc to pin to the bottom edge and stay put across workspaces. Remembers which monitor it was on. |
| 🪟 **Falls back gracefully** | On GNOME / KDE it runs as a transparent window and tells you exactly which settings have no effect. |
| 🖱️ **Click-through** | Clicks pass straight through to whatever is underneath. Keyboard focus is never stolen. |
| 🎨 **Looks nice out of the box** | Seven built-in themes (default, nord, dracula, catppuccin, monokai, light, matugen) + custom colors + custom key icons. |
| 🖼️ **Matugen-aware** | Pick Matugen from the GUI/TUI or reference `@matugen:<role>` directly; colors follow your wallpaper with safe static fallbacks. |
| ⚙️ **Real fuzzel menu** | The settings menu runs as a real fuzzel session and inherits your fuzzel theme; a built-in GTK menu takes over when fuzzel is absent. |
| ⌨️ **TUI config editor** | Browse, change, and save every setting with an on-screen overlay. |
| 🔒 **Typed-text privacy** | Keep normal typing, replace each typing burst with one fixed label, or hide typed characters while shortcuts remain visible. |

---

## 📦 Install

The easiest path is the bundled installer — it detects your distro, installs
build deps, builds, installs to `~/.local/bin`, and sets up input permissions:

```sh
./install.sh                # user install (default)
./install.sh --system       # system install to /usr/local (sudo)
./install.sh --uninstall    # reverse it
```

Installation also adds a Seekey desktop entry. On its first launch, choose
whether future application-menu launches open the settings menu or start the
key overlay directly. `seekey --config-gui` always opens settings.

The settings menu is driven by a real [fuzzel](https://codeberg.org/dnkl/fuzzel)
session (fuzzel ≥ 1.11), so it looks and behaves exactly like your launcher and
follows your `fuzzel.ini` — including Matugen colors written there. Fuzzel is an
optional runtime dependency: when it is missing, too old, or misconfigured,
Seekey falls back to a built-in GTK menu with the same pages and a live preview.

When `~/.cache/matugen/colors.json` is available, the GUI root menu also
offers **Use Matugen colors** for the key overlay itself. A custom
`--matugen <path>` is preserved by the live preview and by overlays launched
from the GUI.

The editors keep at most one key-rendering surface visible. If the real input
overlay is already running, it remains on screen and the sample preview is
suppressed. Otherwise, the editor starts the live sample preview. Stopping the
real overlay from the GUI immediately switches to the sample preview; starting
the real overlay removes the sample first.

For password prompts or other sensitive input, set `typing-display` in the
GUI/TUI or configuration file:

```ini
[general]
typing-display=masked  # full, masked, or off
```

`masked` displays one fixed `<Some Characters>` bubble without exposing the
text or its length. `off` suppresses ordinary typed characters while keeping
shortcuts and non-text keys visible. Privacy modes also suppress ordinary
character lines from `--debug-input` output.

Typed-character grouping currently uses a built-in US evdev key map. Named
keys and shortcuts still work on other layouts, but grouped text can differ
from text produced by an active non-US layout or IME; see
[Troubleshooting](https://github.com/Nakanomk/Seekey/wiki/Troubleshooting#grouped-text-does-not-match-my-keyboard-layout).

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
- [GUI Editor](https://github.com/Nakanomk/Seekey/wiki/GUI-Editor) — graphical menu and desktop-launch behavior
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

<a id="contributors-"></a>

## Contributors

Thanks to everyone contributing code, documentation, bug reports and fixes.
This project uses [All Contributors](https://allcontributors.org/) to recognize
contributions beyond Git's commit statistics; the icons link to the work.

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/Nakanomk"><img src="https://avatars.githubusercontent.com/u/56832666?v=4&size=100" width="100px" height="100px" alt="Nakanosan"/><br /><sub><b>Nakanosan</b></sub></a><br /><a href="https://github.com/Nakanomk/Seekey/commits?author=Nakanomk" title="Code">💻</a> <a href="https://github.com/Nakanomk/Seekey/commits?author=Nakanomk" title="Documentation">📖</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/fzhnf"><img src="https://avatars.githubusercontent.com/u/93417786?v=4&size=100" width="100px" height="100px" alt="Tima Gutmensch"/><br /><sub><b>Tima Gutmensch</b></sub></a><br /><a href="https://github.com/Nakanomk/Seekey/pull/1" title="Code">💻</a></td>
      <td align="center" valign="top" width="14.28%"><a href="https://github.com/reivigloss9"><img src="https://avatars.githubusercontent.com/u/189671960?v=4&size=100" width="100px" height="100px" alt="reivigloss9"/><br /><sub><b>reivigloss9</b></sub></a><br /><a href="https://github.com/Nakanomk/Seekey/discussions/2" title="Bug reports">🐛</a> <a href="https://github.com/Nakanomk/Seekey/discussions/2" title="Code">💻</a></td>
    </tr>
  </tbody>
</table>

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

Maintainers: update `.all-contributorsrc`, then regenerate both READMEs with
`npx --yes --package=@all-contributors/cli@7.1.0 all-contributors generate`.
Node.js is only needed for this maintenance command, not to build or run Seekey.

---

<div align="center">

## 📄 License

MIT — see [LICENSE](LICENSE).

</div>
