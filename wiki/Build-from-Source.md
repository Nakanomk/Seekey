# Build from Source

**Language:** **English** · [简体中文](Build-from-Source.zh-CN)

Prefer to build manually instead of using `install.sh`? Here's how.

## 1. Install dependencies

| Distro | Command |
|---|---|
| Arch | `sudo pacman -S gtk4 libevdev json-glib ncurses pkgconf gcc make` |
| Fedora | `sudo dnf install gtk4-devel libevdev-devel json-glib-devel ncurses-devel pkgconf-pkg-config gcc make` |
| Debian / Ubuntu | `sudo apt install libgtk-4-dev libevdev-dev libjson-glib-dev libncursesw5-dev pkg-config build-essential` |
| openSUSE | `sudo zypper install gtk4-devel libevdev-devel ncurses-devel json-glib-devel pkg-config gcc make` |
| Alpine | `sudo apk add gtk4-dev libevdev-dev ncurses-dev json-glib-dev pkgconf gcc make musl-dev` |
| Void | `sudo xbps-install gtk4-devel libevdev-devel ncurses-devel json-glib-devel pkg-config gcc make` |

### Optional: `gtk4-layer-shell`

This is what lets seekey anchor to the bottom edge and stay put across
workspaces on niri / Hyprland / Sway / river / Wayfire / labwc. Without it,
seekey falls back to a plain transparent window (see [[Compositor Compatibility]]).

```sh
# Arch
sudo pacman -S gtk4-layer-shell
# Fedora
sudo dnf install gtk4-layer-shell-devel
```

On Debian/Ubuntu there is no official package yet — see the
[gtk4-layer-shell](https://github.com/wmww/gtk4-layer-shell) project for
build instructions. Seekey works without it (fallback mode).

## 2. Build

```sh
make                # build seekey
```

The Makefile auto-detects `gtk4-layer-shell` via pkg-config (probing both
`gtk4-layer-shell` and `gtk4-layer-shell-0` pc-file names) and links it
**before** GTK4. That ordering is required: gtk4-layer-shell registers a
static constructor that must run before libwayland-client is initialized,
otherwise `gtk_layer_init_for_window()` fails.

## 3. Run

```sh
./seekey                       # run from the build dir
./seekey --init-config         # create ./seekey.ini with defaults
./seekey --config-tui          # edit settings interactively
```

## 4. (Optional) Install manually

```sh
make install        # installs to PREFIX (default /usr/local)
make uninstall      # removes installed files
```

Override the prefix:

```sh
make install PREFIX=$HOME/.local
```

`make install` puts the binary in `$PREFIX/bin/seekey`, the example config in
`$PREFIX/share/seekey/seekey.ini.example`, and translations in
`$PREFIX/share/locale/<lang>/`.

You still need input permissions — see [[Installation#input-permissions]].

## Makefile targets

| Target | What it does |
|---|---|
| `make` / `make all` | Build `seekey` + compile translations (`.mo`) |
| `make check` | Build and run the unit tests |
| `make install` | Install binary, config sample, translations |
| `make uninstall` | Remove installed files |
| `make pot` | Regenerate `po/seekey.pot` from source (needs `xgettext`) |
| `make mo` | Compile `.po` → `.mo` under `locale/` |
| `make format` | Run `clang-format` on `src/` and `tests/` (if installed) |
| `make clean` | Remove build artifacts + `locale/` |

See [[Testing]] for the test setup and [[Translations]] for the i18n workflow.
