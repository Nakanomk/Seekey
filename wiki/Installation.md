# Installation

The repo ships an `install.sh` that does the whole job: detect your distro,
install build dependencies, build seekey, install the binary, and set up
input permissions so seekey can read `/dev/input/event*`.

## Quick install

```sh
./install.sh                # user install (default) -> ~/.local/bin
./install.sh --system       # system install -> /usr/local/bin (uses sudo)
```

After a user install, log out and back in if it added you to the `input`
group (it will tell you).

## Options

| Option | Effect |
|---|---|
| `--user` | Install to `$HOME/.local` (default) |
| `--system` | Install to `/usr/local` (uses sudo) |
| `--no-input` | Skip the udev rule + `input` group setup |
| `--no-deps` | Don't try to install build dependencies |
| `--force` | Overwrite an existing udev rule |
| `--dry-run` | Print what would happen, make no changes |
| `--uninstall` | Reverse a previous install |
| `-h`, `--help` | Show help |

## What it does, step by step

1. **Detects your package manager** — pacman / dnf / apt / zypper / apk /
   xbps-install. For an unknown manager it prints the dependency list and
   asks you to install manually.
2. **Installs build deps** — gtk4, libevdev, ncursesw, json-glib, pkg-config,
   gcc, make (plus `gtk4-layer-shell` optionally; see [[Build from Source]]).
3. **Builds** with `make`.
4. **Installs** the binary to `$PREFIX/bin/seekey` and the example config to
   `$PREFIX/share/seekey/seekey.ini.example`.
5. **Sets up input access** (unless `--no-input`):
   - Writes `/etc/udev/rules.d/99-seekey.rules` so the `input` group can read
     `/dev/input/event*` (mode `0640`).
   - Creates the `input` group if missing and adds your user to it.
   - Reloads udev rules.
6. **Prints compositor hints** — detects your desktop and warns if it lacks
   `wlr-layer-shell` (GNOME / KDE), suggesting `layer-shell=off`.

> The installer does **not** add an autostart entry. See [[Autostart]].

## Uninstall

```sh
./install.sh --uninstall
```

Removes the binary and data files. The udev rule is left in place (it's
harmless and shared with other input tools) — remove it manually if you want:

```sh
sudo rm /etc/udev/rules.d/99-seekey.rules
```

Your config and `input` group membership are left untouched.

## Input permissions

Seekey reads the kernel input devices directly, so it needs read access to
the `/dev/input/event*` nodes (normally root-only). The installer fixes this
by:

- giving the `input` group read access (mode `0640`), and
- adding your user to that group.

If you skipped that (`--no-input`) or built manually, you have two options:

```sh
# Option A: do it the same way the installer does
sudo groupadd input                       # if the group doesn't exist
sudo usermod -aG input "$USER"            # then log out and back in
# and add the udev rule from install.sh::install_udev_rule

# Option B: quick and dirty (less secure)
sudo chmod 644 /dev/input/event*
```

Option A is strongly preferred. See [[Troubleshooting]] for more.
