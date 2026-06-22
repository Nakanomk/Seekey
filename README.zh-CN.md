# Seekey

一个轻量的 Wayland 按键可视化工具。把最近的按键以悬浮气泡的形式锚
定在屏幕底部——一眼看清修饰键、快捷键和正在输入的文字，又不会
打断你的节奏。

```
[ Ctrl + C ]  [ Ctrl + C x3 ]  [ hello world ]
```

**语言：** [English](README.md) · [简体中文](README.zh-CN.md)

---

## 目录

- [亮点](#亮点)
- [快速上手](#快速上手)
- [兼容性](#兼容性)
- [安装](#安装)
- [配置](#配置)
- [TUI 编辑器](#tui-编辑器)
- [窗口位置](#窗口位置)
- [Matugen 集成](#matugen-集成)
- [开机自启](#开机自启)
- [从源码编译](#从源码编译)
- [测试](#测试)
- [多语言](#多语言)
- [项目结构](#项目结构)

---

## 亮点

- **与合成器解耦的输入。** 通过 `libevdev` 直接读 `/dev/input/event*`，
  不走 Wayland 键盘协议。任何 Wayland 合成器都能跑，无需逐个适配。
- **能用 layer-shell 时自动用 layer-shell。** niri、Hyprland、Sway、
  river、Wayfire、labwc、KWinFT——seekey 锚定在底边，不挡你。多显示器的
  状态会跨会话记忆。
- **GNOME / KDE 上优雅降级。** 跑成一个透明无边框窗口，并在启动时
  明确告诉你哪些配置项在这种模式下不生效。
- **点击穿透。** 浮层从不吃输入事件——点击会穿透到下面的应用。键
  盘焦点也永远不会被抢。
- **Matugen 集成。** `seekey.ini` 里写 `@matugen:on_surface@0.86`
  就能直接用 matugen 生成的色板，跟随你的壁纸变色。
- **TUI 配置编辑器。** 在终端里翻 / 改 / 存所有设置项，不需要手编
  ini 文件。
- **默认项目本地配置。** `./seekeyini` 紧贴二进制，不污染
  `~/.config/`。

---

## 快速上手

```sh
make                       # 编译
./seekey --init-config     # 生成一个 ./seekey.ini 起点（可选）
./seekey --config-tui      # 交互式编辑
./seekey                   # 运行
```

详见下面的 [配置](#配置) 和 [TUI 编辑器](#tui-编辑器)。

---

## 兼容性

seekey 用 Linux evdev 读输入，所以只要当前用户能读 `/dev/input/event*`
就能跑。浮层的行为取决于合成器：

| 合成器 | layer-shell | fallback | 备注 |
|---|---|---|---|
| niri | ✓ | — | 主要测试目标，开箱即用。 |
| Hyprland | ✓ | — | 完整功能集。 |
| Sway | ✓ | — | 完整功能集。 |
| river | ✓ | — | 完整功能集。 |
| Wayfire | ✓ | — | 完整功能集。 |
| labwc | ✓ | — | 完整功能集。 |
| KWinFT（KDE 的 wlroots 分支） | ✓ | — | 完整功能集。 |
| GNOME（mutter） | — | ✓ | 用 "Auto Move Windows" 扩展或 `dconf`——见[固定窗口](#固定窗口gnome--kde-fallback-模式下)。 |
| KDE Plasma（KWin） | — | ✓ | 改 `~/.config/kwinrulesrc`——见[固定窗口](#固定窗口gnome--kde-fallback-模式下)。 |
| X11 会话 | — | — | 不支持（这是 Wayland 工具）。 |
| macOS / Windows / BSD | — | — | `libevdev` 是 Linux 专用。 |

降级模式（GNOME / KDE / 缺 `gtk4-layer-shell`）启动时 seekey 会在
stderr 提示哪些配置项在这种模式下不生效：

```
seekey: using fallback window: <原因>
seekey: NOTE: you are not running under a wlr-layer-shell compositor.
seekey:       The following settings have NO EFFECT in fallback mode:
seekey:         - style.align / margin / margin-horizontal (no edge anchoring)
seekey:         - saved window position (compositor controls placement)
seekey:       To enable them, use a layer-shell compositor (niri, Hyprland,
seekey:       Sway, river) or install gtk4-layer-shell on a supported system.
```

即使在 layer-shell 合成器上想强制 fallback 路径（用于调试），
`seekey.ini` 里设 `layer-shell=off` 或加 `--no-layer-shell`。

---

## 安装

仓库自带 `install.sh`，负责装依赖、编译、用户级安装、udev 规则
（输入设备权限）：

```sh
./install.sh                # 用户级安装（默认）
./install.sh --system      # 系统级安装到 /usr/local（需要 sudo）
./install.sh --no-input    # 跳过 udev + input 组设置
./install.sh --dry-run     # 只打印要做什么
./install.sh --uninstall    # 撤销之前的安装
```

脚本自动识别包管理器（pacman / dnf / apt / zypper / apk /
xbps-install）并安装编译依赖。完整选项见
[install.sh](install.sh) 头注释。

---

## 配置

seekey 读一个 INI 文件。查找顺序：

1. `--config <path>`（显式指定，优先级最高）
2. `<cwd>/seekeyini`（项目本地文件）
3. 内置默认值（不读不写文件）

这样配置就紧贴着你跑的二进制，每个项目独立，不会污染
`~/.config/`。没有隐式 XDG fallback；想要全局配置，就用
`--config` 指过去，或者把 `./seekey.ini` 软链到全局位置。

```sh
./seekey --print-config         # 打印生效配置
./seekey --validate-config      # 校验后退出
./seekey --init-config          # 用当前设置生成 ./seekey.ini
./seekey --init-config --force  # 覆盖已有文件
./seekey --init-config --xdg    # 一次性生成 ~/.config/seekey/config.ini
```

`--print-config` 会显示配置的来源：

```ini
# source: project
# path: /home/you/project/seekey.ini
[general]
...
```

完整带注释的样例见 [seekey.ini.example](seekey.ini.example)。

最常用的设置（大多数用户只需要这些）：

```ini
[general]
duration-ms=1200
typing-idle-ms=650
fade-ms=180
margin=96
margin-horizontal=0
max-items=5
layer-shell=auto
theme=default
merge-repeats=true
merge-modifiers=true
show-mouse=false        # 默认不显示鼠标气泡

[style]
align=right             # right / center / left
disappear=fade          # fade / instant
```

完整字段列表见 [配置参考](#配置参考)。

---

## TUI 编辑器

`./seekey --config-tui` 开一个终端界面来浏览和修改所有设置。顶部会
显示当前配置文件的路径，dirty 时会有 `[Unsaved]` 标记。

| 按键 | 动作 |
|---|---|
| `Up` / `Down` 或 `k` / `j` | 选字段 |
| `Left` / `Right` 或 `h` / `l` | 调整数值 / 选项 / 颜色 / 布尔 |
| `Enter` | 编辑（数值 / 字符串 / 颜色）或打开选择器（选项 / 主题 / 颜色） |
| `s` | 保存到当前路径 |
| `S` | 另存为新路径 |
| `r` | 把当前字段重置为默认 |
| `R` | 把所有字段重置为默认（会先确认） |
| `L` | 从磁盘重新加载（丢弃未保存的修改，会先确认） |
| `W` | 重置窗口状态（忘记保存的 monitor） |
| `?` | 完整帮助 |
| `q` / `Esc` | 退出（dirty 时会问要不要保存） |

`TUI_CHOICE` 字段（`align` / `disappear` / `layer-shell` / `theme`）
按 `Enter` 打开列表选择器，不用再凭记忆敲选项名。

颜色字段按 `Enter` 打开 24 色调色板；按 `c` 输入自定义 `#rrggbb`，
非法的十六进制会被拒绝并提示错误。

---

## 窗口位置

### Layer-shell：由配置决定

在 layer-shell 合成器上，位置完全由这三个配置决定：

- `style.align` —— `left` / `center` / `right` 选水平边
- `general.margin` —— 离底边像素
- `general.margin-horizontal` —— 离左右边像素（`align=center` 时忽略）

这三个键在给定显示器上得到的位置是可复现的。单显示器用户开箱即用
就有稳定位置。

### 多显示器：跨会话记忆

多显示器场景下，seekey 记住上次运行所在的显示器：

- 启动时读 `$XDG_STATE_HOME/seekey/window.ini`（fallback
  `~/.local/state/seekey/window.ini`），用
  [`gtk_layer_set_monitor`](https://github.com/wmww/gtk4-layer-shell)
  把浮层钉到同一显示器。
- 关闭时（或者首次 map 之后）写入当前显示器的 connector 名（比如
  `HDMI-A-1`、`DP-1`、`eDP-1`）。
- 拔了显示器 / 换了显卡导致保存的 monitor 找不到时，静默 fallback
  到聚焦的输出，下次保存时更新。

忘记保存的 monitor：`rm ~/.local/state/seekey/window.ini` 或
在 TUI 里按 `W`。

### 点击穿透

seekey 是纯视觉浮层。它给 `GdkSurface` 设了**空的 input region**，
所有指针事件都穿透到下面的应用：

- 可以正常点击底下的 dock / 任务栏 / 窗口控件，不会被截胡。
- 鼠标 hover / focus / drag 都不受影响。
- 键盘输入也独立——seekey 用 `libevdev` 读输入，永远不抢键盘焦点
  （layer-shell keyboard mode 是 `KEYBOARD_MODE_NONE`）。

在 layer-shell 和 fallback 路径下行为一致。

### 固定窗口（GNOME / KDE fallback 模式下）

seekey 设置了稳定的 app id（`dev.seekey`），window rules 可以按
名字锁定它。

#### KDE Plasma（KWin）

创建 `~/.config/kwinrulesrc`：

```ini
[General]
count=1
rules=1

[1]
Description=seekey
above=true
position=1920,1040
positionrule=Force
wmclass=dev_seekey dev_seekey
wmclasscomplete=true
```

重载 KWin 规则：`qdbus org.kde.KWin /KWin reconfigure`（或重新登录）。
查目标显示器分辨率用 `kscreen-doctor -o`。

#### GNOME（Mutter）

GNOME 的 shell UI 没有一等公民的 per-app window rule 编辑器。
两条可行路径：

1. **"Auto Move Windows" 扩展**（推荐）。从
   <https://extensions.gnome.org> 安装，添加规则时 app id 填
   `dev.seekey`，选位置和显示器，完事。
2. **直接 `dconf` 改**（高级，能力有限）：
   ```sh
   dconf write /org/gnome/mutter/wayland/x-window-rules \
     "['seekey:app-id:dev.seekey']"
   ```

#### Sway / Hyprland（layer-shell 合成器上用 fallback 调试时）

```ini
# sway config
for_window [app_id="dev.seekey"] floating enable
for_window [app_id="dev.seekey"] sticky enable
```

```ini
# hyprland.conf
windowrulev2 = float, class:^(dev\.seekey)$
windowrulev2 = keep-floating, class:^(dev\.seekey)$
```

---

## Matugen 集成

[Matugen](https://github.com/InioX/matugen) 从一张壁纸生成 Material You
色板。seekey 启动时可以从 `colors.json` 取色：

```ini
[style]
foreground=@matugen:on_surface
background=@matugen:surface@0.86
border-color=@matugen:outline
placeholder-foreground=@matugen:on_surface@0.74
```

`@matugen:<role>` 解析为 matugen `colors.json` 里的十六进制值。可
选的 `@<float>` 后缀会包成 GTK CSS 的 `alpha(...)` 写法：

```text
@matugen:surface         → #1a1a1a
@matugen:surface@0.86    → alpha(#1a1a1a, 0.86)
@matugen:missing_key     → 原样保留（让你看到拼错的角色名）
```

路径解析顺序：

1. 命令行 `--matugen <path>`
2. 环境变量 `MATUGEN_COLORS`
3. `$XDG_CACHE_HOME/matugen/colors.json`（matugen 默认输出位置）
4. `~/.cache/matugen/colors.json`（fallback）

**换壁纸后重新加载：** seekey 不会自动 watch 这个 JSON。换壁纸后
重新跑 matugen，再重启 seekey（`pkill seekey && seekey &`）让新色
板生效。

如果 JSON 加载失败，seekey 在 stderr 打一行警告，然后按 `seekey.ini`
里的值原样使用——matugen 集成完全是可选的。

---

## 开机自启

`./install.sh` **不**装自启条目——不同 Wayland 合成器的启动机制不
同，选什么要看你的工作流。在你选的合成器里加上 seekey：

**GNOME / KDE Plasma / Cinnamon / XFCE**——创建 `~/.config/autostart/seekey.desktop`：
```ini
[Desktop Entry]
Type=Application
Name=Seekey
Comment=Keyboard visualizer overlay for Wayland
Exec=seekey
Terminal=false
Categories=Utility;
X-GNOME-Autostart-enabled=true
```

**Hyprland**——加到 `~/.config/hypr/hyprland.conf`：
```ini
exec-once = seekey
```

**Sway**——加到 `~/.config/sway/config`：
```
exec seekey
```

**niri**——加到 `~/.config/niri/config.kdl`：
```kdl
spawn-at-startup "seekey"
```

**river**——加到 `~/.config/river/init`：
```sh
riverctl spawn seekey
```

**systemd 用户服务**（合成器无关）——创建 `~/.config/systemd/user/seekey.service`：
```ini
[Unit]
Description=Seekey keyboard visualizer
After=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/local/bin/seekey
Restart=on-failure

[Install]
WantedBy=default.target
```
然后启用：
```sh
systemctl --user enable --now seekey.service
```

> **提示：** `~/.local/bin`（`install.sh` 的默认 prefix）在某些登录会话
> 里不在 `$PATH` 里。在 systemd unit 里用绝对路径（比如
> `ExecStart=%h/.local/bin/seekey`），或者把 `~/.local/bin` 加到
> shell rc 里。

---

## 从源码编译

安装依赖：

```sh
# Arch
sudo pacman -S gtk4 libevdev json-glib ncurses pkgconf gcc make

# Fedora
sudo dnf install gtk4-devel libevdev-devel json-glib-devel ncurses-devel \
                 pkgconf-pkg-config gcc make

# Debian / Ubuntu
sudo apt install libgtk-4-dev libevdev-dev libjson-glib-dev \
                 libncursesw5-dev pkg-config build-essential
```

可选（niri / Hyprland / wlroots layer 模式）：

```sh
# Arch
sudo pacman -S gtk4-layer-shell
# Fedora
sudo dnf install gtk4-layer-shell-devel
```

然后编译：

```sh
make                # 编译 seekey
make check          # 跑单元测试
make install        # 装到 PREFIX（默认 /usr/local）
make uninstall      # 卸载
make format         # 跑 clang-format（如果有）
make clean          # 清构建产物
```

`Makefile` 自动探测 `gtk4-layer-shell` 是否安装，并把它链接到 GTK4
**之前**（layer-shell 的 static constructor 要求这个顺序）。在
pc 文件叫 `gtk4-layer-shell-0.pc` 的发行版上，Makefile 会同时探测
两个名字。

---

## 测试

```sh
make check
```

68 个单元测试，用 vendored 的 [Unity](https://www.throwtheswitch.org/unity)
框架。覆盖：

- `config`：默认值、查找顺序、load / save 往返、校验、主题预设、
  matugen 引用解析
- `tui`：字段值格式化、adjust / reset 纯逻辑、bool `string_target` 回归
  （修过那个触发重构的 `dest != NULL` 断言）
- `keynames`：按键名 / 图标 / shift 配对 / 修饰键顺序查找
- `window_state`：路径解析、save / load 往返、损坏文件容错、clear

TUI 渲染逻辑（ncurses）只通过纯函数测——ncurses 那段代码在
`src/tui.c` 里用 `#ifdef SEEKEY_TEST` 隔离。

---

## 多语言

应用遵循 GNU gettext 约定。`Makefile` 把 `po/*.po` 编译成
`locale/<lang>/LC_MESSAGES/seekey.mo`，`install.sh` 装到
`$(PREFIX)/share/locale/<lang>/`。

| 语言 | 状态 | 翻译文件 |
|---|---|---|
| 英文（`en`） | 源码 | n/a |
| 简体中文（`zh-CN`） | ✓ | `po/zh-CN.po` |

加新翻译：

```sh
# 1. 把语言代码加到 po/LINGUAS
echo "ja" >> po/LINGUAS

# 2. 抽出源字符串
make po/seekey.pot

# 3. 初始化 .po
msginit -l ja -o po/ja.po -i po/seekey.pot

# 4. 用你喜欢的编辑器翻译 po/ja.po

# 5. 编译并测试
make
LANG=ja ./seekey
```

要加翻译版 README，参考 `README.zh-CN.md` 的格式即可。

---

## 项目结构

```
seekey/
├── LICENSE                    MIT
├── README.md                  本文件（英文）
├── README.zh-CN.md            简体中文翻译
├── Makefile
├── install.sh                 用户级安装 + 依赖探测
├── seekey.ini.example         带注释的样例配置
├── src/
│   ├── seekey.h               公共类型
│   ├── main.c                 GTK 应用、事件循环、气泡逻辑
│   ├── config.c / .h          配置 load / save / parse / theme
│   ├── tui.c / .h              TUI 编辑器
│   ├── window_state.c / .h     monitor 持久化（XDG_STATE_HOME）
│   ├── input.c                 evdev 键鼠捕获
│   ├── keynames.c              按键名 + 图标查找
│   └── layer_shell.c           gtk4-layer-shell 集成
├── tests/
│   ├── test_main.c
│   ├── test_config.c
│   ├── test_tui.c
│   ├── test_keynames.c
│   ├── test_window_state.c
│   ├── test_helpers.c / .h
│   └── vendor/unity/           vendored Unity 测试框架（MIT）
├── po/
│   ├── POTFILES.in
│   ├── LINGUAS
│   ├── seekey.pot
│   └── zh-CN.po
```

---

## 协议

MIT。详见 [LICENSE](LICENSE)。
