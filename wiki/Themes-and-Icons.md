# Themes and Icons

**Language:** **English** · [简体中文](Themes-and-Icons.zh-CN)

## Built-in themes

Set `theme` in `[general]` to one of:

`default` · `light` · `nord` · `dracula` · `catppuccin` · `monokai`

A theme sets seven colors at once: foreground, background, border, shadow,
and the three placeholder colors. Individual `[style]` color keys **override**
the preset — leave a key out (or comment it out) to fall back to the preset.

### Full palette

| Theme | foreground | background | border | shadow |
|---|---|---|---|---|
| `default` | `#f7f7f2` | `alpha(#111318, 0.86)` | `alpha(#ffffff, 0.18)` | `0 7px 22px alpha(#000000, 0.30)` |
| `light` | `#1a1a2e` | `alpha(#fafafa, 0.90)` | `alpha(#cccccc, 0.40)` | `0 7px 22px alpha(#000000, 0.12)` |
| `nord` | `#d8dee9` | `alpha(#2e3440, 0.90)` | `alpha(#88c0d0, 0.25)` | `0 7px 22px alpha(#000000, 0.38)` |
| `dracula` | `#f8f8f2` | `alpha(#282a36, 0.90)` | `alpha(#bd93f9, 0.24)` | `0 7px 22px alpha(#000000, 0.38)` |
| `catppuccin` | `#cdd6f4` | `alpha(#1e1e2e, 0.90)` | `alpha(#cba6f7, 0.22)` | `0 7px 22px alpha(#000000, 0.38)` |
| `monokai` | `#f8f8f2` | `alpha(#272822, 0.90)` | `alpha(#a6e22e, 0.22)` | `0 7px 22px alpha(#000000, 0.38)` |

| Theme | placeholder-fg | placeholder-bg | placeholder-border |
|---|---|---|---|
| `default` | `alpha(#f7f7f2, 0.74)` | `alpha(#111318, 0.56)` | `alpha(#ffffff, 0.14)` |
| `light` | `alpha(#1a1a2e, 0.60)` | `alpha(#e8e8e8, 0.70)` | `alpha(#bbbbbb, 0.35)` |
| `nord` | `alpha(#d8dee9, 0.70)` | `alpha(#2e3440, 0.58)` | `alpha(#88c0d0, 0.16)` |
| `dracula` | `alpha(#f8f8f2, 0.70)` | `alpha(#282a36, 0.58)` | `alpha(#bd93f9, 0.16)` |
| `catppuccin` | `alpha(#cdd6f4, 0.72)` | `alpha(#1e1e2e, 0.58)` | `alpha(#cba6f7, 0.14)` |
| `monokai` | `alpha(#f8f8f2, 0.72)` | `alpha(#272822, 0.58)` | `alpha(#a6e22e, 0.14)` |

## Custom colors

Any `[style]` color key accepts GTK CSS color values:

```ini
[style]
foreground=#f7f7f2
background=alpha(#111318, 0.86)      # hex with alpha
border-color=alpha(#ffffff, 0.18)
shadow=0 7px 22px rgba(0, 0, 0, 0.30)   # full CSS box-shadow
```

Acceptable forms: `#rrggbb`, named CSS colors (`red`, `white`, …), or
`alpha(#rrggbb, 0.86)`. For wallpaper-driven colors, use
`@matugen:<role>` (see [[Matugen Integration]]).

## Custom key icons

Override the glyph shown for a key in the `[icons]` section. The key name
matches the bubble label. Up to 64 overrides are supported.

```ini
[icons]
Backspace="⌫"
Enter="↵"
Space="␣"
Tab="↹"
Esc="⎋"
Delete="⌦"
Up="↑"
Down="↓"
Left="←"
Right="→"
```

The built-in default labels (used when no override is set) include readable
names like `Backspace`, `Enter`, `Space`, `Tab`, `Esc`, `Delete`, `Caps Lock`,
`Num Lock`, `Page Up`, `Volume Up`, `Super`, `AltGr`, arrows, F-keys,
numpad keys (`Num 7`, `Num +`, …), media keys (`Mute`, `Volume Down`, …),
and edit keys (`Copy`, `Paste`, `Undo`, `Cut`, …). See `src/keynames.c` for
the full table.

## Picking a theme from the TUI

In `./seekey --config-tui`, the `theme` field is a `TUI_CHOICE` — press
`Enter` to open a list picker and pick a preset live. See [[TUI Editor]].
