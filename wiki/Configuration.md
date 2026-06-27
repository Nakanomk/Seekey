# Configuration

Seekey reads a single INI file. Settings can also be overridden on the
command line. Most people only touch a few keys.

## Where the config comes from

Lookup order (first match wins):

1. `--config <path>` on the command line (highest priority)
2. `<cwd>/seekey.ini` — a project-local file next to where you run seekey
3. Built-in defaults (no file is read or written)

There is **no implicit `~/.config/` fallback**. The config lives next to the
binary you run, so each project / directory stays independent. If you want a
global config, point `--config` at one, or symlink `./seekey.ini` to it.

> Want a one-time XDG bootstrap? `./seekey --init-config --xdg` writes
> `~/.config/seekey/config.ini` once.

## Managing the config file

```sh
./seekey --print-config         # show the effective configuration + its source
./seekey --validate-config      # validate the config and exit
./seekey --init-config          # create ./seekey.ini with current settings
./seekey --init-config --force  # overwrite an existing file
./seekey --init-config --xdg    # bootstrap ~/.config/seekey/config.ini once
./seekey --config-tui           # edit interactively in the terminal
```

`--print-config` prints a header showing where the config came from:

```ini
# source: project
# path: /home/you/project/seekey.ini
[general]
...
```

A fully commented sample lives at
[`seekey.ini.example`](https://github.com/Nakanomk/Seekey/blob/main/seekey.ini.example)
in the repo.

## Command-line overrides

Most settings can be set on the command line (these override the file). Run
`./seekey --help` for the full list. The common ones:

| Flag | Effect |
|---|---|
| `--config <path>` | Use this config file |
| `--matugen <path>` | Use this matugen `colors.json` |
| `--no-layer-shell` | Force fallback window mode |
| `--debug-input` | Print raw input events to stderr |
| `--duration <ms>` | Bubble visible time (100–10000) |
| `--typing-idle <ms>` | Pause that ends a typing group (100–5000) |
| `--fade-ms <ms>` | Fade-out duration (0–3000; 0 = instant) |
| `--margin <px>` | Bottom margin |
| `--margin-horizontal <px>` | Side margin (ignored when align=center) |
| `--max-items <n>` | Max bubbles on screen (1–20) |
| `--align left\|center\|right` | Bubble row alignment |
| `--disappear instant\|fade` | Removal animation |
| `--layer-shell auto\|required\|off` | Layer-shell mode |
| `--theme <name>` | Theme preset |
| `--merge-repeats` / `--no-merge-repeats` | Stack repeated keys as "x3" |
| `--merge-modifiers` / `--no-merge-modifiers` | Merge modifier-only bubbles |
| `--show-mouse` / `--no-mouse` | Show mouse clicks/scroll |
| `-V`, `--version` | Print version |
| `-h`, `--help` | Print help |

## The common settings (most users only need these)

```ini
[general]
duration-ms=1200          # how long a bubble stays visible
typing-idle-ms=650        # pause before next char starts a new text bubble
fade-ms=180               # fade-out length (0 = instant)
margin=96                 # bottom margin (layer-shell mode)
margin-horizontal=0       # side margin
max-items=5               # max bubbles at once
layer-shell=auto          # auto / required / off
theme=default             # default, nord, dracula, catppuccin, monokai, light
merge-repeats=true        # "Ctrl+C x3" instead of 3 separate bubbles
merge-modifiers=true      # "Ctrl" → "Ctrl+C" instead of two bubbles
show-mouse=false          # mouse bubbles off by default

[style]
align=right               # right / center / left
disappear=fade            # fade / instant
```

For every key, its type, range, and default, see
[[Configuration Reference]]. For colors, themes, and key icons, see
[[Themes and Icons]].

## How key bubbles are grouped

Two behaviors shape what you see:

- **`merge-repeats`** — pressing the same combo repeatedly stacks into one
  bubble with a counter: `[ Ctrl+C ]` → `[ Ctrl+C x3 ]` instead of three
  separate bubbles.
- **`merge-modifiers`** — a modifier-only bubble (`[ Ctrl ]`) gets extended
  in place when you then press a non-modifier, becoming `[ Ctrl+C ]` rather
  than spawning a second bubble.
- **Typing groups** — consecutive character keys are collected into one text
  bubble (`[ hello world ]`). After `typing-idle-ms` with no input, the next
  character starts a new bubble.

## File format notes

- Lines starting with `#` or `;` are comments.
- Sections are `[general]`, `[style]`, `[icons]`.
- Boolean values are `true` / `false`.
- Colors accept GTK CSS: `#rrggbb`, named CSS colors, or `alpha(#rrggbb, 0.86)`.
- Any color field can use `@matugen:<role>` — see [[Matugen Integration]].
