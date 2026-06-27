# TUI Editor

`./seekey --config-tui` opens a terminal UI to browse and edit every setting
without hand-editing ini files.

The **top bar** shows the path to the active config file and an `[Unsaved]`
indicator when there are uncommitted changes.

## Keybindings

| Key | Action |
|---|---|
| `Up` / `Down` or `k` / `j` | Select field |
| `Left` / `Right` or `h` / `l` | Adjust numeric / choice / color / bool |
| `Enter` | Edit (numbers / strings / colors) or open picker (choice / theme / color) |
| `s` | Save to current path |
| `S` | Save as a new path |
| `r` | Reset the current field to its default |
| `R` | Reset **all** fields to defaults (confirms first) |
| `L` | Reload from disk (discards pending changes, confirms first) |
| `W` | Reset window state (forget the saved monitor) |
| `?` | Show full help |
| `q` / `Esc` | Quit (asks to save if there are unsaved changes) |

## Field types

- **`TUI_CHOICE`** fields (`align`, `disappear`, `layer-shell`, `theme`) —
  `Enter` opens a list picker, so you never type option names from memory.
- **Color fields** — `Enter` opens a 24-color palette; press `c` to type a
  custom `#rrggbb` value. Invalid hex is rejected with an error message.
- **Numeric / string / bool** — adjust with `Left`/`Right`, or `Enter` to
  type a value.

## Window state reset (`W`)

On multi-monitor setups, seekey remembers which monitor it was on (stored in
`$XDG_STATE_HOME/seekey/window.ini`). Press `W` to forget that and let
seekey pick the focused output next time. See [[Window Position]].
