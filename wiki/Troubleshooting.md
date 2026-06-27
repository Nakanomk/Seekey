# Troubleshooting

Common problems and fixes.

## No bubbles appear on first run

**Cause:** seekey can't read `/dev/input/event*`. Those nodes are normally
root-only.

**Fix (recommended):** run the installer, which sets up a udev rule and the
`input` group:

```sh
./install.sh
```

Then **log out and back in** so the new group membership takes effect
(the installer warns you about this).

**Fix (manual):** do what the installer does by hand:

```sh
sudo groupadd input                       # if it doesn't exist
sudo usermod -aG input "$USER"            # then log out and back in
# udev rule so the input group can read event devices:
echo 'KERNEL=="event*", SUBSYSTEM=="input", GROUP="input", MODE="0640"' \
  | sudo tee /etc/udev/rules.d/99-seekey.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

**Quick check** (does seekey see input at all?):

```sh
./seekey --debug-input
```

This prints every raw evdev event to stderr. If you see events but no
bubbles, it's a rendering/compositor issue (keep reading). If you see
nothing, it's a permissions issue (or no keyboard is being detected — check
that the device has keyboard keys).

## "using fallback window" notice on startup

On GNOME / KDE (or anywhere `gtk4-layer-shell` is missing), seekey prints:

```
seekey: using fallback window: <reason>
seekey: NOTE: you are not running under a wlr-layer-shell compositor.
seekey:       The following settings have NO EFFECT in fallback mode:
seekey:         - style.align / margin / margin-horizontal (no edge anchoring)
seekey:         - saved window position (compositor controls placement)
```

This is **expected** on non-layer-shell desktops. The overlay still works;
it's just a floating transparent window instead of an edge-anchored layer.
To pin it where you want, see [[Window Position#pinning-the-window-on-gnome--kde-fallback-mode]].

To **silence** it on a layer-shell compositor where you intentionally want
fallback, that's normal too — the notice only prints in fallback mode.

## Bubbles appear but in the wrong place / wrong monitor

- On layer-shell compositors, position is set by `style.align`,
  `general.margin`, `general.margin-horizontal`. See [[Window Position]].
- On multi-monitor setups, seekey remembers the last monitor in
  `~/.local/state/seekey/window.ini`. To forget it:

  ```sh
  rm ~/.local/state/seekey/window.ini
  ```

  …or press `W` in `--config-tui`.

## `gtk_layer_init_for_window()` fails / layer-shell not working

This usually means gtk4-layer-shell's static constructor ran too late. The
Makefile links `gtk4-layer-shell` **before** GTK4 to avoid this. If you
built manually with a custom link line, make sure gtk4-layer-shell comes
before gtk4. On distros where the pc file is `gtk4-layer-shell-0.pc`, the
Makefile probes both names automatically.

If you're on GNOME or KDE, layer-shell simply isn't supported by the
compositor — use fallback mode (`layer-shell=off`).

## Clicks don't pass through

The overlay sets an empty input region for click-through. If something
underneath isn't receiving clicks:

- Make sure you're not running a second overlay on top.
- On fallback (non-layer-shell) mode, confirm the window is actually
  seekey's and not a different transparent window.

Click-through works the same in both modes by design.

## `--config-tui` looks broken / no colors

The TUI uses ncurses. If it renders without color or garbled, your
`$TERM` may not advertise colors. Try `TERM=xterm-256color ./seekey --config-tui`.

## Config changes aren't applied

- Seekey reads the config **at startup**. Restart it after editing:
  `pkill seekey && ./seekey &`.
- Check which file is actually loaded: `./seekey --print-config` prints the
  source path at the top.
- Remember the lookup order: `--config <path>` → `<cwd>/seekey.ini` →
  defaults. There's no `~/.config/` fallback unless you ask for it. See
  [[Configuration]].

## Matugen colors not applied

- Seekey doesn't watch the JSON. Re-run `matugen`, then **restart** seekey.
  See [[Matugen Integration]].
- Check the path: `--matugen <path>` > `$MATUGEN_COLORS` >
  `$XDG_CACHE_HOME/matugen/colors.json` > `~/.cache/matugen/colors.json`.
- If a role is unknown, seekey leaves the literal `@matugen:role` visible in
  the bubble so the typo is obvious.

## Build fails: package not found

```sh
make
# ... Package gtk4 was not found ...
```

Install the build dependencies for your distro — see
[[Build from Source#1-install-dependencies]]. Or just run `./install.sh`,
which installs deps for you.

## Still stuck

Run with debug to see what's happening:

```sh
./seekey --debug-input        # raw evdev events to stderr
./seekey --print-config       # effective config + source path
./seekey --validate-config    # check the config for problems
```

If you think it's a bug, open an issue at
<https://github.com/Nakanomk/Seekey/issues> with the output of
`./seekey --print-config`, your compositor, and what you expected vs. what
happened.
