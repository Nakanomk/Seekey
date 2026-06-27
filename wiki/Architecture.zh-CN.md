# 架构

**语言：** [English](Architecture) · **简体中文**

带你过一遍源码是怎么组织的、数据怎么流动。看不懂源码就先看这个。

## 一句话总结

seekey 在一个后台线程里从 Linux 内核读原始键盘事件（`/dev/input/event*`，经 `libevdev`），把每次按键送到 GTK 主线程，画成 CSS 样式的气泡，显示在锚定于屏幕底部的点击穿透浮层窗口里。

## 项目布局

```
seekey/
├── LICENSE                    MIT
├── README.md / README.zh-CN.md
├── Makefile
├── install.sh                 用户级安装器 + 依赖检测
├── seekey.ini.example         带注释的示例配置
├── src/
│   ├── seekey.h               共享类型 + input/keyname 公开 API
│   ├── main.c                 GTK 应用、事件循环、气泡逻辑
│   ├── config.c / .h          配置加载/保存/解析、主题、matugen
│   ├── tui.c / .h             TUI 编辑器（ncurses）
│   ├── window_state.c / .h    显示器记忆（XDG_STATE_HOME）
│   ├── input.c                evdev 键盘/鼠标捕获（后台线程）
│   ├── keynames.c             按键名 + 图标查找表
│   └── layer_shell.c          gtk4-layer-shell 集成（dlopen）
├── tests/
│   ├── test_main.c            测试运行器入口
│   ├── test_config.c
│   ├── test_tui.c
│   ├── test_keynames.c
│   ├── test_window_state.c
│   ├── test_helpers.c / .h
│   └── vendor/unity/          内置 Unity 测试框架（MIT）
├── po/                        gettext 翻译
│   ├── POTFILES.in  LINGUAS  seekey.pot  zh-CN.po
└── locale/                    编译出的 .mo 文件（编译产物）
```

## 各模块职责

### `src/seekey.h` —— 共享类型
中心头文件。定义 `SeekeyConfig`（每个设置就是一个普通结构体字段）、`KeyEventMessage`（从 input 递交给 UI 的一次按键）、修饰键位掩码、滚动伪键码，以及 `input.c` / `keynames.c` / `layer_shell.c` 的公开 API。所有文件都包含它。

### `src/config.c` / `config.h` —— 配置
- `seekey_config_set_defaults` —— 用硬编码默认值填充 `SeekeyConfig`。
- 主题预设表（`default`、`light`、`nord`、`dracula`、`catppuccin`、`monokai`）+ `seekey_config_apply_theme`。
- `seekey_config_resolve_path` —— 选配置文件（`--config` → `<cwd>/seekey.ini` → 无）。
- `seekey_config_load` —— 用 `GKeyFile` 读 INI，数值夹到范围内，应用主题后再应用逐键颜色覆盖，读 `[icons]`。
- `seekey_config_save` / `seekey_config_init` / `seekey_config_print` / `seekey_config_validate`。
- `seekey_parse_args` —— CLI 参数覆盖配置字段。
- Matugen 辅助：`seekey_matugen_resolve_path`、`seekey_matugen_load`（把 `colors.json` 解析成 `GHashTable`）、`seekey_matugen_resolve_value`（把 `@matugen:role@0.86` 变成 `alpha(#hex, 0.86)`）。

### `src/input.c` —— evdev 捕获
输入侧。`seekey_input_new` 用 `libevdev` 打开**所有** `/dev/input/event*` 设备，保留看起来像键盘的（有一组有用按键）设备，若 `show-mouse=true` 还保留鼠标类设备。它起一个**后台线程**跑跨所有 fd 的 `poll(2)` 循环。按键事件到来时构造 `KeyEventMessage`，跟踪修饰键状态和 shifted/非 shift 修饰键标志，通过 `g_idle_add`（`dispatch_key_event` → 回调）推到 GTK 主线程。这就是 seekey 能在任何合成器上工作的原因：它不碰 Wayland 键盘协议。

### `src/keynames.c` —— 按键标签
静态表把 evdev 键码（`linux/input-event-codes.h` 里的 `KEY_*`）映射到人类可读标签（`Backspace`、`Enter`、`Up`、`Volume Up`、…）。提供 `seekey_key_name`、`seekey_key_text`（某键打出的字符，区分 shifted）、`seekey_key_icon`（应用 `[icons]` 覆盖）、`seekey_is_modifier`、`seekey_modifier_order`（Ctrl/Shift/Alt/Super 在组合里的规范显示顺序）。

### `src/layer_shell.c` —— 浮层锚定
用**运行时 `dlopen` 加载**（非硬链接）的 `gtk4-layer-shell`，依次探测 `libgtk4-layer-shell.so.0` 和 `libgtk4-layer-shell.so`。`seekey_layer_shell_try_init` 为窗口初始化 layer-shell，设 layer（top）、锚定底边、应用边距、设显示器、键盘模式设为 `NONE`（这样 seekey 永不抢键盘）、设命名空间。库缺失或 `layer-shell=off` 时返回错误，`main.c` 降级成普通透明窗口。

> 为什么用 `dlopen`，Makefile 又把它链接在 GTK4 之前？gtk4-layer-shell 注册的静态构造函数必须在 libwayland-client 初始化前运行。链接时 Makefile 把它放在 GTK4 之前；不链接时，运行时 `dlopen` 路径对那些有库但没 `.pc` 文件的发行版仍可用。

### `src/window_state.c` / `window_state.h` —— 显示器记忆
把浮层上次在哪台显示器持久化到 `$XDG_STATE_HOME/seekey/window.ini`（回退 `~/.local/state/seekey/...`）。`seekey_window_state_load`（缺失/坏文件不是错误——返回零值）、`seekey_window_state_save`（写连接器名，如 `DP-1`）、`seekey_window_state_clear`、`seekey_find_monitor_by_name`（按连接器名匹配 `GdkDisplay` 的显示器）。

### `src/tui.c` / `tui.h` —— 终端编辑器
`--config-tui` 编辑器（ncurses）。构造一个 `TuiField` 数组（33 个字段）描述每个设置——类型（`TUI_UINT` / `TUI_STRING` / `TUI_CHOICE` / `TUI_BOOL` / `TUI_COLOR`）、目标指针、min/max/step、默认值。**纯辅助**函数（`tui_field_value`、`tui_adjust_field`、`tui_reset_field`、`tui_nearest_color_index`、`tui_current_choice_index`）无需 ncurses 即可单测。ncurses 渲染代码用 `#ifdef SEEKEY_TEST` 包裹，测试构建会跳过。

### `src/main.c` —— 应用
最大的文件。职责：
- `main()`：setlocale、绑 gettext、设默认、解析+加载配置、解析 CLI、处理 `--init-config`/`--print-config`/`--validate-config`/`--config-tui`（然后退出），否则创建 `GtkApplication`。
- `activate()`：建窗口 + 一个水平 `GtkBox` 放气泡、设空输入区域（点击穿透）、试 layer-shell（否则降级窗口）、加载保存的显示器状态、启动 `input.c`。
- **气泡逻辑**：`on_key_event`（来自 `input.c` 的回调）决定是合并进上一个气泡（`merge-repeats`、`merge-modifiers`）、开/扩展打字分组，还是新建气泡。每个气泡安排一个 `duration-ms` 超时；若 `disappear=fade`，两阶段移除先加 `fading` CSS 类，`fade-ms` 后再移除 widget。`trim_bubbles` 强制 `max-items`。
- `shutdown()`：把当前显示器连接器名持久化到 `window_state`，释放输入线程。

## 数据流（一次按键）

```
/dev/input/event*  ──libevdev──▶  input.c  poll 线程
                                      │ 构造 KeyEventMessage
                                      │ g_idle_add(dispatch_key_event)
                                      ▼
                          main.c  on_key_event  (GTK 主线程)
                                      │ 合并？打字分组？新气泡？
                                      ▼
                        GtkBox  ──CSS──▶  屏幕气泡
                                      │ duration-ms（+ fade-ms）后
                                      ▼
                                   移除 / 淡出
```

## 关键设计选择

- **与合成器无关的输入。** 直接读 evdev 意味着 seekey 在任何 Wayland 合成器上都能工作，无需逐合成器协议代码。代价：需要读 `/dev/input/event*`（见 [[问题排查]]）。
- **运行时 `dlopen` layer-shell。** 让一个二进制同时在 layer-shell 和非 layer-shell 桌面上工作；库是可选的。
- **线程 → 主线程递交。** evdev 轮询会阻塞，所以放线程里；所有 GTK 调用通过 `g_idle_add` 在主线程进行。
- **通过空输入区域实现点击穿透**（在 `GdkSurface` 上），加 `KEYBOARD_MODE_NONE` 保证永不抢键盘焦点。
- **配置是普通结构体。** 没有 getter/setter；`SeekeyConfig` 字段直接读。简单，TUI 的 `TuiField` 目标指针也直接指向它们。

纯逻辑如何在无显示器下测试见 [[测试]]。
