# 测试

**语言：** [English](Testing) · **简体中文**

## 跑测试

```sh
make check
```

会编译 `build/test_runner` 并运行它。测试构建刻意用和主构建不同的标志：

- `-DSEEKEY_TEST` —— 把 `src/tui.c` 里的 ncurses 渲染代码关掉，这样无需终端就能测纯辅助函数。
- `-DG_DISABLE_ASSERT` —— 关掉生产路径里 `g_assert` 的副作用。
- 只链接 **glib / gobject / gio / json-glib / gtk4** + 内置 Unity 框架——不要 ncurses、不要 libevdev、不要 layer-shell、不需要显示器。

所以整套测试能在 CI 里无头运行。

## 覆盖了什么

70 个单元测试，用内置 [Unity](https://www.throwtheswitch.org/unity) 框架（MIT；在 `tests/vendor/unity/`）。

| 文件 | 覆盖 |
|---|---|
| `test_config.c` | 默认值、配置查找顺序、加载/保存往返、校验、主题预设、matugen `@matugen:<role>` / `@<alpha>` 解析 |
| `test_tui.c` | 字段值格式化、调整/重置纯逻辑、bool `string_target` 回归（触发重构的那个 `dest != NULL` 断言）、字段分组完整性 |
| `test_keynames.c` | 按键名 / 图标 / shift 配对 / 修饰键顺序查找 |
| `test_window_state.c` | 路径解析、保存/加载往返、坏文件容忍、清除 |
| `test_main.c` | 测试运行器入口；注册所有套件 |
| `test_helpers.c` / `.h` | 共享测试辅助 |

## 为什么用纯辅助函数？

TUI 渲染（ncurses）和 GTK 事件循环无法在无头单测里跑。所以 `src/tui.c` 里可测的逻辑被拆成纯函数（`tui_field_value`、`tui_adjust_field`、`tui_reset_field`、`tui_nearest_color_index`、`tui_current_choice_index`），它们操作 `TuiField`/`SeekeyConfig` 不做 I/O。ncurses 代码路径用 `#ifdef SEEKEY_TEST` 包裹。`config.c`、`keynames.c`、`window_state.c` 也是同样模式，所以它们能直接编译进测试二进制。

## 加一个测试

1. 在相关 `tests/test_<area>.c` 里加一个 `TEST(<group>, <name>) { ... }` 函数。
2. 在该文件的 `RUN_<GROUP>_TESTS()`（或等价 `RUN_TEST` 块）里注册它。
3. 从 `test_main.c` 调用这个组运行器。
4. `make check`。

## 其他 Makefile 目标

| 目标 | 用途 |
|---|---|
| `make check` | 编译 + 运行 `build/test_runner` |
| `make format` | 对 `src/` 和 `tests/` 跑 `clang-format`（若已安装）|
| `make clean` | 删除 `seekey`、object 文件、`build/test_runner`、`locale/` |
