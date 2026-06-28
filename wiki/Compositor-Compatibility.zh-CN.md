# 合成器兼容性

**语言：** [English](Compositor-Compatibility) · **简体中文**

Seekey 通过 `libevdev`（内核 evdev 接口）读输入，**不走** Wayland 键盘协议。所以只要你能读 `/dev/input/event*`（见 [[Installation.zh-CN]]），输入在任何 Linux Wayland 会话都能工作。

各合成器的差异在于**浮层**：seekey 能不能锚定到边缘、切工作区时稳住（layer-shell），还是只能降级成普通窗口。

## 兼容性表

| 合成器 | layer-shell | 降级 | 备注 |
|---|:---:|:---:|---|
| niri | ✓ | — | 默认测试目标，开箱即用。|
| Hyprland | ✓ | — | 全功能。|
| Sway | ✓ | — | 全功能。|
| river | ✓ | — | 全功能。|
| Wayfire | ✓ | — | 全功能。|
| labwc | ✓ | — | 全功能。|
| KWinFT（KDE wlroots fork）| ✓ | — | 全功能。|
| GNOME（mutter）| — | ✓ | 无 layer-shell。固定窗口——见 [[Window-Position.zh-CN]]。|
| KDE Plasma（KWin）| — | ✓ | 无 layer-shell。用 `kwinrulesrc` 固定——见 [[Window-Position.zh-CN]]。|
| X11 会话 | — | — | 不支持（这是 Wayland 工具）。|
| macOS / Windows / BSD | — | — | `libevdev` 仅 Linux。|

## `layer-shell` 设置

```ini
[general]
layer-shell=auto       # 可用时用 layer-shell，否则降级（默认）
# layer-shell=required # 用 layer-shell，不可用就退出
# layer-shell=off      # 始终用普通无边框窗口
```

或命令行：`--layer-shell auto|required|off`，或 `--no-layer-shell`（等同 `off`）。

`required` 适合你希望 seekey 在非 layer-shell 桌面上拒绝运行、而不是飘成浮窗的情况。

## 降级模式（GNOME / KDE / 没装 gtk4-layer-shell）

layer-shell 不可用时，seekey 跑成一个透明无边框窗口，并向 stderr 打印提示，列出此模式下无效的设置：

```
seekey: using fallback window: <reason>
seekey: NOTE: you are not running under a wlr-layer-shell compositor.
seekey:       The following settings have NO EFFECT in fallback mode:
seekey:         - style.align / margin / margin-horizontal (no edge anchoring)
seekey:         - saved window position (compositor controls placement)
seekey:       To enable them, use a layer-shell compositor (niri, Hyprland,
seekey:        Sway, river) or install gtk4-layer-shell on a supported system.
```

降级模式下窗口位置由合成器控制。要固定到角落 / 显示器，用窗口规则——见 [[Window-Position.zh-CN]]。

## 强制降级（用于调试）

即使在 layer-shell 合成器上也能强制降级：

```sh
./seekey --no-layer-shell
# 或在 seekey.ini 里设 layer-shell=off
```

适合复现降级行为，或在 wlroots 合成器上测试窗口规则配置。
