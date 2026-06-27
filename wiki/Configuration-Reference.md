# Configuration Reference

Every config key, its section, type, allowed range, and default. Extracted
from `src/config.c` and `seekey.ini.example`.

Comments: lines starting with `#` or `;` are ignored. Booleans are
`true` / `false`. Colors are GTK CSS values (`#rrggbb`, named colors, or
`alpha(#rrggbb, 0.86)`).

## `[general]`

### Common

| Key | Type | Range | Default | Meaning |
|---|---|---|---|---|
| `duration-ms` | uint | 100–10000 | `1200` | How long a bubble stays visible (ms) |
| `typing-idle-ms` | uint | 100–5000 | `650` | Pause before the next char starts a new text bubble |
| `fade-ms` | uint | 0–3000 | `180` | Fade-out length (0 = no fade, instant) |
| `margin` | uint | 0–1000 | `0`* | Bottom margin in layer-shell mode (px) |
| `margin-horizontal` | uint | 0–1000 | `0` | Side margin (ignored when `align=center`) |
| `max-items` | uint | 1–20 | `5` | Max bubbles shown at once; older are trimmed |
| `layer-shell` | choice | `auto` / `required` / `off` | `auto` | Layer-shell mode (see [[Compositor Compatibility]]) |
| `theme` | string | preset name | `default` | Built-in theme (see [[Themes and Icons]]) |
| `merge-repeats` | bool | — | `true` | Stack repeated identical bubbles as "x3" |
| `merge-modifiers` | bool | — | `true` | Extend a modifier-only bubble when more keys arrive |
| `show-mouse` | bool | — | `false` | Show mouse clicks and scroll as bubbles |

\* The code default is `0`; `seekey.ini.example` suggests `96` as a sensible
value. Set it to whatever clears your dock / taskbar.

### Window (fallback mode only)

Used only when layer-shell is unavailable (GNOME / KDE / no gtk4-layer-shell).

| Key | Type | Range | Default | Meaning |
|---|---|---|---|---|
| `window-width` | uint | 240–3000 | `720` | Fallback window width (px) |
| `window-height` | uint | 80–1000 | `160` | Fallback window height (px) |

## `[style]`

### Layout

| Key | Type | Range | Default | Meaning |
|---|---|---|---|---|
| `align` | choice | `left` / `center` / `right` | `right` | Bubble row alignment |
| `disappear` | choice | `instant` / `fade` | `fade` | Bubble removal animation |
| `spacing` | uint | 0–80 | `7` | Gap between bubbles (px) |
| `overlay-padding` | uint | 0–80 | `12` | Inner padding of the overlay box |
| `key-min-width` | uint | 0–300 | `0` | Min bubble width (0 = content-based) |

### Bubble appearance

| Key | Type | Range | Default | Meaning |
|---|---|---|---|---|
| `key-padding-x` | uint | 0–80 | `14` | Horizontal padding inside a bubble |
| `key-padding-y` | uint | 0–80 | `8` | Vertical padding inside a bubble |
| `key-radius` | uint | 0–80 | `12` | Corner radius |
| `key-border-width` | uint | 0–20 | `1` | Border thickness |
| `key-font-px` | uint | 8–96 | `20` | Font size |
| `key-font-weight` | uint | 100–1000 | `700` | Font weight (CSS numeric) |
| `typing-max-width` | uint | 80–2000 | `480` | Max width of a typing bubble; longer text gets "…" (0 = no max) |

### Colors (override the theme preset)

Leave a key out (or comment it out) to fall back to the preset.

| Key | Type | Default (`default` theme) | Meaning |
|---|---|---|---|
| `foreground` | color | `#f7f7f2` | Bubble text color |
| `background` | color | `alpha(#111318, 0.86)` | Bubble background |
| `border-color` | color | `alpha(#ffffff, 0.18)` | Bubble border |
| `shadow` | shadow | `0 7px 22px alpha(#000000, 0.30)` | GTK CSS box-shadow |
| `placeholder-text` | string | `Seekey` | Text of the idle placeholder bubble |
| `placeholder-foreground` | color | `alpha(#f7f7f2, 0.74)` | Placeholder text color |
| `placeholder-background` | color | `alpha(#111318, 0.56)` | Placeholder background |
| `placeholder-border-color` | color | `alpha(#ffffff, 0.14)` | Placeholder border |

Any color field may use `@matugen:<role>` or `@matugen:<role>@<alpha>` — see
[[Matugen Integration]].

## `[icons]` (optional)

Override the glyph shown for a key. The key name matches the bubble label
(`Backspace`, `Enter`, `Space`, `Tab`, `Esc`, `Delete`, `Up`, `Down`,
`Left`, `Right`, etc.). Remove the whole section (or comment out every key)
to use the built-in defaults.

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

Up to 64 icon overrides are supported (`SEEKEY_MAX_ICON_OVERRIDES`).

## Theme presets

| Theme | foreground | background |
|---|---|---|
| `default` | `#f7f7f2` | `alpha(#111318, 0.86)` |
| `light` | `#1a1a2e` | `alpha(#fafafa, 0.90)` |
| `nord` | `#d8dee9` | `alpha(#2e3440, 0.90)` |
| `dracula` | `#f8f8f2` | `alpha(#282a36, 0.90)` |
| `catppuccin` | `#cdd6f4` | `alpha(#1e1e2e, 0.90)` |
| `monokai` | `#f8f8f2` | `alpha(#272822, 0.90)` |

Each preset also sets border, shadow, and placeholder colors. See
[[Themes and Icons]] for the full palette and how overrides interact with
presets.
