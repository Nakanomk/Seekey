# 问题排查

**语言：** [English](Troubleshooting) · **简体中文**

常见问题和修复。

## 第一次运行没有气泡

**原因：** seekey 读不了 `/dev/input/event*`。这些节点默认只有 root 能读。

**修复（推荐）：** 跑安装脚本，它会配好 udev 规则和 `input` 组：

```sh
./install.sh
```

然后**注销再登录**一次让新组成员资格生效（脚本会提示）。

**手动修复：** 照安装脚本做的来：

```sh
sudo groupadd input                       # 如果组不存在
sudo usermod -aG input "$USER"            # 然后注销再登录
# udev 规则，让 input 组能读 event 设备：
echo 'KERNEL=="event*", SUBSYSTEM=="input", GROUP="input", MODE="0640"' \
  | sudo tee /etc/udev/rules.d/99-seekey.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

**快速检查**（seekey 到底有没有收到输入？）：

```sh
./seekey --debug-input
```

会把每个原始 evdev 事件打到 stderr。有事件但没气泡，就是渲染/合成器问题（往下看）。没事件，就是权限问题（或者没检测到键盘——检查设备有没有键盘按键）。

## 启动时 "using fallback window" 提示

在 GNOME / KDE（或任何没装 `gtk4-layer-shell` 的地方），seekey 会打印：

```
seekey: using fallback window: <reason>
seekey: NOTE: you are not running under a wlr-layer-shell compositor.
seekey:       The following settings have NO EFFECT in fallback mode:
seekey:         - style.align / margin / margin-horizontal (no edge anchoring)
seekey:         - saved window position (compositor controls placement)
```

在非 layer-shell 桌面上这是**预期行为**。浮层照样能用，只是变成了飘浮透明窗口而不是边缘锚定层。要固定到你想要的位置，见 [[窗口位置#在-gnome--kde-上固定窗口降级模式]]。

在 layer-shell 合成器上你**故意**要降级时想消掉这个提示？那也是正常的——提示只在降级模式打印。

## 有气泡但位置 / 显示器不对

- layer-shell 合成器上，位置由 `style.align`、`general.margin`、`general.margin-horizontal` 决定。见 [[窗口位置]]。
- 多显示器时，seekey 会把上次所在的显示器记在 `~/.local/state/seekey/window.ini`。清除：

  ```sh
  rm ~/.local/state/seekey/window.ini
  ```

  或在 `--config-tui` 里按 `W`。

## `gtk_layer_init_for_window()` 失败 / layer-shell 不工作

通常意味着 gtk4-layer-shell 的静态构造函数跑晚了。Makefile 把 `gtk4-layer-shell` 链接在 **GTK4 之前**来避免这个。如果你手动编译用了自定义链接顺序，确保 gtk4-layer-shell 在 gtk4 之前。pc 文件名为 `gtk4-layer-shell-0.pc` 的发行版，Makefile 会自动探测两个名字。

如果你在 GNOME 或 KDE 上，layer-shell 就是合成器不支持——用降级模式（`layer-shell=off`）。

## 点击不穿透

浮层设了空输入区域实现点击穿透。如果下面的东西收不到点击：

- 确认上面没有另一个浮层。
- 降级模式（非 layer-shell）下，确认那个窗口确实是 seekey 的，不是别的透明窗口。

点击穿透在两种模式下设计上是一样的。

## `--config-tui` 显示乱 / 没颜色

TUI 用 ncurses。如果没颜色或乱码，可能是你的 `$TERM` 没声明颜色。试试 `TERM=xterm-256color ./seekey --config-tui`。

## 配置改动没生效

- seekey **启动时**读配置。改完重启：`pkill seekey && ./seekey &`。
- 确认实际加载的是哪个文件：`./seekey --print-config` 顶部打印来源路径。
- 记住查找顺序：`--config <path>` → `<cwd>/seekey.ini` → 默认值。没有 `~/.config/` 回退，除非你主动要。见 [[配置]]。

## Matugen 颜色没生效

- seekey 不监视 JSON。重跑 `matugen`，然后**重启** seekey。见 [[Matugen 集成]]。
- 检查路径：`--matugen <path>` > `$MATUGEN_COLORS` > `$XDG_CACHE_HOME/matugen/colors.json` > `~/.cache/matugen/colors.json`。
- 角色未知时，seekey 会把字面的 `@matugen:role` 留在气泡里，拼错一目了然。

## 编译失败：找不到包

```sh
make
# ... Package gtk4 was not found ...
```

按你的发行版装依赖——见 [[从源码编译#1-安装依赖]]。或直接跑 `./install.sh`，它帮你装依赖。

## 还是卡住

开 debug 看发生了什么：

```sh
./seekey --debug-input        # 原始 evdev 事件打到 stderr
./seekey --print-config       # 生效配置 + 来源路径
./seekey --validate-config    # 检查配置问题
```

觉得是 bug 的话，去 <https://github.com/Nakanomk/Seekey/issues> 开个 issue，附上 `./seekey --print-config` 的输出、你的合成器、以及你期望 vs 实际发生的。
