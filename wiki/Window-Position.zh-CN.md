# 窗口位置

**语言：** [English](Window-Position) · **简体中文**

seekey 怎么决定出现在哪、怎么记住你的显示器、点击穿透怎么实现，以及在 GNOME / KDE 上怎么固定窗口。

## layer-shell：由配置固定

在 layer-shell 合成器上（niri / Hyprland / Sway / river / Wayfire / labwc），位置完全由三个配置键决定：

- `style.align` —— `left` / `center` / `right` 选水平贴边
- `general.margin` —— 底边距（像素）
- `general.margin-horizontal` —— 左右边距（`align=center` 时忽略）

```ini
[style]
align=right
[general]
margin=96
margin-horizontal=0
```

它们在给定显示器上给出可复现的位置。单显示器开箱即稳。哪些合成器支持 layer-shell 见 [[合成器兼容性]]。

## 多显示器：跨会话记忆

多显示器时，seekey 会记住上次在哪台显示器：

- 启动时读 `$XDG_STATE_HOME/seekey/window.ini`（回退到 `~/.local/state/seekey/window.ini`），用 `gtk_layer_set_monitor` 钉到那台显示器。
- 关闭时（以及首次 surface map 之后，这样即使被强杀也会留下状态）写入当前显示器的连接器名（如 `HDMI-A-1`、`DP-1`、`eDP-1`）。
- 保存的显示器已断开时，seekey 静默回退到当前聚焦的输出，下次保存更新状态。

清除保存的显示器：

```sh
rm ~/.local/state/seekey/window.ini
```

或在 TUI 编辑器（`--config-tui`）里按 `W`。

## 点击穿透

seekey 纯粹是可视化浮层。窗口在 `GdkSurface` 上设置了**空输入区域**，所有指针事件都穿透到下面的应用：

- 点击气泡下方的 dock / 任务栏 / 窗口控件——不会被拦截。
- 下方应用鼠标悬停 / 聚焦 / 拖拽不受影响。
- 键盘输入独立：seekey 通过 `libevdev` 读，永远不抢键盘焦点（layer-shell 键盘模式是 `KEYBOARD_MODE_NONE`）。

layer-shell 模式和降级模式都一样。

## 在 GNOME / KDE 上固定窗口（降级模式）

seekey 设了稳定的 app id `dev.seekey`，窗口规则可以按名匹配。（KDE/KWin 上 class 显示为 `dev_seekey`。）

### KDE Plasma（KWin）

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

重载 KWin 规则：

```sh
qdbus org.kde.KWin /KWin reconfigure
```

（或注销再登录）。用 `kscreen-doctor -o` 查目标显示器分辨率，据此设 `position`。

### GNOME（Mutter）

GNOME 没有一等公民的逐应用窗口规则编辑器。两条实用路径：

1. **扩展——"Auto Move Windows"**（推荐）。从 <https://extensions.gnome.org> 安装，加一条 app id 为 `dev.seekey` 的规则，选位置 + 显示器，完成。
2. **`dconf` 直接改**（进阶，有限）：
   ```sh
   dconf write /org/gnome/mutter/wayland/x-window-rules \
     "['seekey:app-id:dev.seekey']"
   ```

### Sway / Hyprland（在 layer-shell 合成器上强制降级）

如果你在 wlroots 合成器上设了 `layer-shell=off`（比如调试），用合成器自己的规则固定降级窗口：

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
