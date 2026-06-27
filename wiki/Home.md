# Seekey Wiki

Welcome! Seekey is a tiny Wayland keyboard visualizer — it shows your last
few keystrokes as floating bubbles anchored to the bottom of the screen.

```
[ Ctrl + C ]  [ Ctrl + C x3 ]  [ hello world ]
```

If you're new, the fastest path is:

```sh
make                       # build
./seekey --init-config     # drop a starter ./seekey.ini (optional)
./seekey --config-tui      # tweak settings in a terminal UI (optional)
./seekey                   # run
```

No bubbles on first run? You need read access to `/dev/input/event*` —
`./install.sh` sets that up for you. See [[Troubleshooting]].

---

## Pages

**Getting started**
- [[Installation]] — `install.sh` options, distro dependencies, input permissions
- [[Build from Source]] — manual build, dependencies per distro, Makefile targets
- [[Configuration]] — the config file, lookup order, common settings
- [[Configuration Reference]] — every key, its type, range, and default
- [[TUI Editor]] — keybindings for `--config-tui`

**Behaviour & compatibility**
- [[Compositor Compatibility]] — layer-shell vs fallback per desktop
- [[Window Position]] — anchoring, multi-monitor persistence, click-through, GNOME/KDE pinning
- [[Autostart]] — start seekey with your compositor / desktop

**Looks**
- [[Themes and Icons]] — built-in presets, custom colors, custom key glyphs
- [[Matugen Integration]] — wallpaper-driven colors via `@matugen:<role>`

**For contributors**
- [[Architecture]] — how the source is laid out (read this if the code confuses you)
- [[Testing]] — running and understanding the unit tests
- [[Translations]] — adding a new language

**Reference**
- [[Troubleshooting]] — input permissions, fallback mode, no bubbles, etc.
