# Matugen Integration

**Language:** **English** · [简体中文](Matugen-Integration.zh-CN)

[Matugen](https://github.com/InioX/matugen) generates a Material You color
palette from a single wallpaper image. Seekey can pick up those colors at
startup by referencing them in `seekey.ini`.

## Usage

Reference matugen roles with `@matugen:<role>` in any color field:

```ini
[style]
foreground=@matugen:on_surface
background=@matugen:surface@0.86
border-color=@matugen:outline
placeholder-foreground=@matugen:on_surface@0.74
```

The `@matugen:<role>` syntax is resolved to the hex value from matugen's
`colors.json`. The optional `@<float>` suffix wraps the result in
`alpha(<hex>, <float>)`:

```text
@matugen:surface         → #1a1a1a
@matugen:surface@0.86    → alpha(#1a1a1a, 0.86)
@matugen:missing_key     → kept verbatim (visible so you notice the typo)
```

An unknown role is left as-is on purpose — you'll see the literal
`@matugen:missing_key` in the bubble, which makes typos obvious.

## Where seekey looks for `colors.json`

Resolution order (first match wins):

1. `--matugen <path>` on the command line
2. `$MATUGEN_COLORS` environment variable
3. `$XDG_CACHE_HOME/matugen/colors.json` (matugen's default)
4. `~/.cache/matugen/colors.json` (fallback)

## Reload after a wallpaper change

Seekey does **not** auto-watch the JSON. After running `matugen` on a new
wallpaper, restart seekey to apply the new palette:

```sh
pkill seekey && seekey &
```

## If the JSON can't be loaded

If the file is missing or invalid, seekey prints one warning to stderr and
uses whatever values are in `seekey.ini` verbatim. The matugen integration is
fully optional — seekey runs fine without matugen installed.
