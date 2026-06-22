# Seekey

A small Wayland key-visualizer. Shows your last keystrokes as floating
bubbles anchored to the bottom of the screen — see modifiers, shortcuts,
and typed text at a glance without breaking your flow.

```
[ Ctrl + C ]  [ Ctrl + C x3 ]  [ hello world ]
```

**Languages:** [English](README.md) · [简体中文](README.zh-CN.md)

---

## Contents

- [Highlights](#highlights)
- [Quick start](#quick-start)
- [Compatibility](#compatibility)
- [Install](#install)
- [Configuration](#configuration)
- [TUI editor](#tui-editor)
- [Window position](#window-position)
- [Matugen integration](#matugen-integration)
- [Autostart](#autostart)
- [Build from source](#build-from-source)
- [Tests](#tests)
- [Languages](#languages)
- [Project layout](#project-layout)

---

## Highlights

- **Compositor-independent input.** Reads `/dev/input/event*` via
  `libevdev`, not the Wayland keyboard protocol. Works on every Wayland
  compositor without a per-compositor protocol dance.
- **Layer-shell overlay when available.** niri, Hyprland, Sway, river,
  Wayfire, labwc, KWinFT — seekey anchors to the bottom edge and stays
  out of your way. Multi-monitor state is persisted across sessions.
- **Graceful fallback on GNOME / KDE.** Runs as a transparent undecorated
  window and tells you exactly which config keys have no effect.
- **Click-through.** The overlay never blocks input; clicks pass to
  whatever is underneath. Keyboard focus is never claimed.
- **Matugen integration.** Reference matugen roles in `seekey.ini` with
  `@matugen:on_surface@0.86` and the colors follow your wallpaper.
- **TUI config editor.** Browse / change / save every setting from the
  terminal; no need to hand-edit ini files.
- **Project-local config by default.** `./seekeyini` next to the binary;
  no implicit `~/.config/` clutter.

---

## Quick start

```sh
make                       # build
./seekey --init-config     # drop a starter ./seekey.ini (optional)
./seekey --config-tui      # edit interactively
./seekey                   # run
```

See [Configuration](#configuration) and [TUI editor](#tui-editor) below.

---

## Compatibility

Seekey uses Linux evdev for input, so it works on any Linux distribution
as long as the user can read `/dev/input/event*`. The overlay's behaviour
depends on the compositor:

| Compositor | layer-shell | Fallback | Notes |
|---|---|---|---|
| niri | ✓ | — | Default test target. Works out of the box. |
| Hyprland | ✓ | — | Full feature set. |
| Sway | ✓ | — | Full feature set. |
| river | ✓ | — | Full feature set. |
| Wayfire | ✓ | — | Full feature set. |
| labwc | ✓ | — | Full feature set. |
| KWinFT (KDE wlroots fork) | ✓ | — | Full feature set. |
| GNOME (mutter) | — | ✓ | Use "Auto Move Windows" extension or `dconf` — see [Pinning the window](#pinning-the-window-on-gnome--kde-fallback). |
| KDE Plasma (KWin) | — | ✓ | Configure `~/.config/kwinrulesrc` — see [Pinning the window](#pinning-the-window-on-gnome--kde-fallback). |
| X11 session | — | — | Not supported (this is a Wayland tool). |
| macOS, Windows, BSD | — | — | `libevdev` is Linux-only. |

In fallback mode (GNOME / KDE / missing `gtk4-layer-shell`), seekey prints
a notice to stderr listing the settings that have no effect:

```
seekey: using fallback window: <reason>
seekey: NOTE: you are not running under a wlr-layer-shell compositor.
seekey:       The following settings have NO EFFECT in fallback mode:
seekey:         - style.align / margin / margin-horizontal (no edge anchoring)
seekey:         - saved window position (compositor controls placement)
seekey:       To enable them, use a layer-shell compositor (niri, Hyprland,
seekey:       Sway, river) or install gtk4-layer-shell on a supported system.
```

To force the fallback path even on a layer-shell compositor (for
debugging), set `layer-shell=off` in `seekey.ini` or pass `--no-layer-shell`.

---

## Install

The repo includes an `install.sh` that handles deps, build, user-level
install, and a udev rule for input access:

```sh
./install.sh                # user install (default)
./install.sh --system      # system install to /usr/local (sudo)
./install.sh --no-input    # skip udev + input group setup
./install.sh --dry-run     # print what would happen, do nothing
./install.sh --uninstall    # reverse a previous install
```

The script detects the package manager (pacman / dnf / apt / zypper /
apk / xbps-install) and installs build deps automatically. See the
[install.sh](install.sh) header for the full option list.

---

## Configuration

Seekey reads a single INI file. Lookup order:

1. `--config <path>` (explicit, highest priority)
2. `<cwd>/seekeyini` (project-local file)
3. Built-in defaults (no file is read or written)

This means the configuration lives next to the binary you are running,
keeping each project independent. There is no implicit `~/.config/`
fallback; if you want a global config, point `--config` at it or symlink
`./seekey.ini` to one.

```sh
./seekey --print-config         # show the effective configuration
./seekey --validate-config      # validate and exit
./seekey --init-config          # create ./seekey.ini with current settings
./seekey --init-config --force  # overwrite an existing file
./seekey --init-config --xdg    # bootstrap ~/.config/seekey/config.ini once
```

`--print-config` shows where the configuration came from:

```ini
# source: project
# path: /home/you/project/seekey.ini
[general]
...
```

A complete sample with all keys and explanations is at
[seekey.ini.example](seekey.ini.example).

The most common settings (most users will only touch these):

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
show-mouse=false        # mouse bubbles off by default

[style]
align=right             # right / center / left
disappear=fade          # fade / instant
```

See [Configuration reference](#configuration-reference) for the full list.

---

## TUI editor

`./seekey --config-tui` opens a terminal UI to browse and edit every
setting. The top bar shows the path to the active config file and an
`[Unsaved]` indicator when there are uncommitted changes.

| Key | Action |
|---|---|
| `Up` / `Down` or `k` / `j` | Select field |
| `Left` / `Right` or `h` / `l` | Adjust numeric / choice / color / bool |
| `Enter` | Edit (numbers / strings / colors) or open picker (choice / theme / color) |
| `s` | Save to current path |
| `S` | Save as a new path |
| `r` | Reset the current field to its default |
| `R` | Reset ALL fields to defaults (confirms first) |
| `L` | Reload from disk (discards pending changes, confirms first) |
| `W` | Reset window state (forget saved monitor) |
| `?` | Show full help |
| `q` / `Esc` | Quit (asks to save if there are unsaved changes) |

For `TUI_CHOICE` fields (`align`, `disappear`, `layer-shell`, `theme`),
`Enter` opens a list picker — no more typing option names from memory.

For color fields, `Enter` opens a 24-color palette; press `c` to enter
a custom `#rrggbb` value (invalid hex is rejected with an error message).

---

## Window position

### Layer-shell: fixed by config

On layer-shell compositors, the position is fully determined by:

- `style.align` — `left` / `center` / `right` choose the horizontal edge
- `general.margin` — bottom margin in pixels
- `general.margin-horizontal` — left/right margin in pixels (ignored when `align=center`)

These three keys give a reproducible position on a given monitor. On a
single-monitor setup you get a stable position out of the box.

### Multi-monitor: persisted across sessions

On a multi-monitor setup, seekey remembers which monitor it was on the
last time it ran:

- On startup, seekey reads `$XDG_STATE_HOME/seekey/window.ini`
  (fallback `~/.local/state/seekey/window.ini`) and uses
  [`gtk_layer_set_monitor`](https://github.com/wmww/gtk4-layer-shell)
  to pin the overlay to the same monitor.
- On shutdown (or after the first surface map), seekey writes the
  current monitor's connector name (e.g. `HDMI-A-1`, `DP-1`, `eDP-1`).
- If the saved monitor is no longer connected, seekey silently falls
  back to the focused output and the next save updates the state.

To forget the saved monitor, run `rm ~/.local/state/seekey/window.ini`
or press `W` in the TUI.

### Click-through

seekey is purely a visual overlay. The window sets an **empty input
region** on its `GdkSurface`, so all pointer events pass through to apps
behind it:

- You can click the dock / taskbar / window controls that sit underneath
  the bubbles without anything intercepting the click.
- Mouse hover / focus / drag are unaffected.
- Keyboard input is independent — seekey reads it via `libevdev` and
  never claims keyboard focus (the layer-shell keyboard mode is
  `KEYBOARD_MODE_NONE`).

This works the same in layer-shell mode and in the fallback path.

### Pinning the window on GNOME / KDE (fallback mode)

seekey sets a stable app id (`dev.seekey`) so window rules can target it
by name.

#### KDE Plasma (KWin)

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

Reload KWin's rules: `qdbus org.kde.KWin /KWin reconfigure` (or log out/in).
Find your target monitor's resolution with `kscreen-doctor -o`.

#### GNOME (Mutter)

GNOME has no first-class per-app window rule editor in the shell UI.
Two practical paths:

1. **Extension — "Auto Move Windows"** (recommended). Install from
   <https://extensions.gnome.org>, add a rule with app id `dev.seekey`,
   pick the position + monitor, done.
2. **`dconf` direct edit** (advanced, limited):
   ```sh
   dconf write /org/gnome/mutter/wayland/x-window-rules \
     "['seekey:app-id:dev.seekey']"
   ```

#### Sway / Hyprland (fallback on a layer-shell compositor, e.g. for debug)

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

---

## Matugen integration

[Matugen](https://github.com/InioX/matugen) generates a Material You
color palette from a single wallpaper image. Seekey can pick up those
colors at startup by referencing them in `seekey.ini`:

```ini
[style]
foreground=@matugen:on_surface
background=@matugen:surface@0.86
border-color=@matugen:outline
placeholder-foreground=@matugen:on_surface@0.74
```

The `@matugen:<role>` syntax is resolved to the hex value from
matugen's `colors.json`. The optional `@<float>` suffix wraps the result
in `alpha(<hex>, <float>)`:

```text
@matugen:surface         → #1a1a1a
@matugen:surface@0.86    → alpha(#1a1a1a, 0.86)
@matugen:missing_key     → kept verbatim (visible so you notice the typo)
```

Path resolution order:

1. `--matugen <path>` on the command line
2. `$MATUGEN_COLORS` environment variable
3. `$XDG_CACHE_HOME/matugen/colors.json` (matugen's default)
4. `~/.cache/matugen/colors.json` (fallback)

**Reload after wallpaper change:** seekey does not auto-watch the
JSON. After running `matugen` on a new wallpaper, restart seekey
(`pkill seekey && seekey &`) to apply the new palette.

If the JSON cannot be loaded, seekey prints one warning to stderr and
uses whatever values are in `seekey.ini` verbatim — the matugen
integration is fully optional.

---

## Autostart

`./install.sh` does **not** install an autostart entry — different
Wayland compositors have different startup mechanisms and the right
choice depends on your workflow. Add seekey to the compositor of your
choice:

**GNOME / KDE Plasma / Cinnamon / XFCE** — create `~/.config/autostart/seekey.desktop`:
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

**Hyprland** — add to `~/.config/hypr/hyprland.conf`:
```ini
exec-once = seekey
```

**Sway** — add to `~/.config/sway/config`:
```
exec seekey
```

**niri** — add to `~/.config/niri/config.kdl`:
```kdl
spawn-at-startup "seekey"
```

**river** — add to `~/.config/river/init`:
```sh
riverctl spawn seekey
```

**systemd user service** (compositor-agnostic) — create `~/.config/systemd/user/seekey.service`:
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
> `$PATH` in some login sessions. Use the absolute path (e.g.
> `ExecStart=%h/.local/bin/seekey`) in the systemd unit, or add
> `~/.local/bin` to your shell rc.

---

## Build from source

Install dependencies:

```sh
# Arch
sudo pacman -S gtk4 libevdev json-glib ncurses pkgconf gcc make

# Fedora
sudo dnf install gtk4-devel libevdev-devel json-glib-devel ncurses-devel \
                 pkgconf-pkg-config gcc make

# Debian / Ubuntu
sudo apt install libgtk-4-dev libevdev-dev libjson-glib-dev \
                 libncursesw5-dev pkg-config build-essential
```

Optional for niri / Hyprland / wlroots layer mode:

```sh
# Arch
sudo pacman -S gtk4-layer-shell
# Fedora
sudo dnf install gtk4-layer-shell-devel
```

Then build:

```sh
make                # build seekey
make check          # run unit tests
make install        # install to PREFIX (default /usr/local)
make uninstall      # remove installed files
make format         # clang-format sources if available
make clean          # remove build artifacts
```

The `Makefile` automatically detects whether `gtk4-layer-shell` is
installed and links it before GTK4 (a static constructor ordering
requirement). On systems where the pc file is named
`gtk4-layer-shell-0.pc` (some distros), the Makefile probes both names.

---

## Tests

```sh
make check
```

68 unit tests using the vendored [Unity](https://www.throwtheswitch.org/unity)
framework. Tests cover:

- `config`: default values, search order, load / save roundtrip, validation,
  theme presets, matugen reference resolution
- `tui`: field value formatting, adjust / reset pure logic, bool string_target
  regression (the `dest != NULL` assertion that prompted the refactor)
- `keynames`: key name / icon / shift-pair / modifier-order lookups
- `window_state`: path resolution, save / load roundtrip, malformed-file
  tolerance, clear

TUI rendering logic (ncurses) is tested via pure helpers only — the
ncurses code path is gated by `#ifdef SEEKEY_TEST` in `src/tui.c`.

---

## Languages

The application follows the GNU gettext convention. The Makefile
compiles `po/*.po` to `locale/<lang>/LC_MESSAGES/seekey.mo` and
`install.sh` installs them under `$(PREFIX)/share/locale/<lang>/`.

| Language | Status | Translation |
|---|---|---|
| English (`en`) | Source | n/a |
| 简体中文 (`zh-CN`) | ✓ | `po/zh-CN.po` |

To add a new translation:

```sh
# 1. Add the language code to po/LINGUAS
echo "ja" >> po/LINGUAS

# 2. Extract the source strings
make po/seekey.pot

# 3. Initialize a new .po file
msginit -l ja -o po/ja.po -i po/seekey.pot

# 4. Translate po/ja.po with your editor of choice

# 5. Build and test
make
LANG=ja ./seekey
```

To add a translated README, follow the existing `README.zh-CN.md` template.

---

## Project layout

```
seekey/
├── LICENSE                    MIT
├── README.md                  this file (English)
├── README.zh-CN.md            简体中文翻译
├── Makefile
├── install.sh                 user-level installer + deps detection
├── seekey.ini.example         sample configuration with comments
├── src/
│   ├── seekey.h               public types
│   ├── main.c                 GTK application, event loop, bubble logic
│   ├── config.c / .h          config load / save / parse / theme
│   ├── tui.c / .h              TUI editor
│   ├── window_state.c / .h     monitor persistence (XDG_STATE_HOME)
│   ├── input.c                 evdev key / mouse capture
│   ├── keynames.c              key name + icon lookup
│   └── layer_shell.c           gtk4-layer-shell integration
├── tests/
│   ├── test_main.c
│   ├── test_config.c
│   ├── test_tui.c
│   ├── test_keynames.c
│   ├── test_window_state.c
│   ├── test_helpers.c / .h
│   └── vendor/unity/           vendored Unity test framework (MIT)
├── po/
│   ├── POTFILES.in
│   ├── LINGUAS
│   ├── seekey.pot
│   └── zh-CN.po
└── examples/                   (removed; see seekey.ini.example)
```

---

## License

MIT. See [LICENSE](LICENSE).
