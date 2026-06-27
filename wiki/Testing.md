# Testing

**Language:** **English** · [简体中文](Testing.zh-CN)

## Run the tests

```sh
make check
```

This builds `build/test_runner` and runs it. The test build deliberately uses
different flags than the main build:

- `-DSEEKEY_TEST` — gates out the ncurses rendering code in `src/tui.c` so
  the pure helpers can be tested without a terminal.
- `-DG_DISABLE_ASSERT` — disables `g_assert` side effects in production paths.
- Links only **glib / gobject / gio / json-glib / gtk4** + the vendored Unity
  framework — no ncurses, no libevdev, no layer-shell, no display needed.

So the whole suite runs headless in CI.

## What's covered

68 unit tests using the vendored
[Unity](https://www.throwtheswitch.org/unity) framework (MIT; under
`tests/vendor/unity/`).

| File | Covers |
|---|---|
| `test_config.c` | default values, config search order, load/save roundtrip, validation, theme presets, matugen `@matugen:<role>` / `@<alpha>` resolution |
| `test_tui.c` | field value formatting, adjust/reset pure logic, bool `string_target` regression (the `dest != NULL` assertion that prompted the refactor) |
| `test_keynames.c` | key name / icon / shift-pair / modifier-order lookups |
| `test_window_state.c` | path resolution, save/load roundtrip, malformed-file tolerance, clear |
| `test_main.c` | test runner entry; registers all suites |
| `test_helpers.c` / `.h` | shared test helpers |

## Why pure helpers?

TUI rendering (ncurses) and the GTK event loop can't be exercised in a
headless unit test. So the testable logic in `src/tui.c` is split into pure
functions (`tui_field_value`, `tui_adjust_field`, `tui_reset_field`,
`tui_nearest_color_index`, `tui_current_choice_index`) that operate on a
`TuiField`/`SeekeyConfig` with no I/O. The ncurses code path is wrapped in
`#ifdef SEEKEY_TEST`. The same pattern is used in `config.c`,
`keynames.c`, and `window_state.c`, which is why they compile into the test
binary directly.

## Adding a test

1. Add a `TEST(<group>, <name>) { ... }` function in the relevant
   `tests/test_<area>.c`.
2. Register it in that file's `RUN_<GROUP>_TESTS()` (or equivalent
   `RUN_TEST` block).
3. Call the group runner from `test_main.c`.
4. `make check`.

## Other Makefile targets

| Target | Purpose |
|---|---|
| `make check` | build + run `build/test_runner` |
| `make format` | `clang-format` `src/` and `tests/` (if installed) |
| `make clean` | remove `seekey`, objects, `build/test_runner`, `locale/` |
