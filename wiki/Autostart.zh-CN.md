# 开机自启

**语言：** [English](Autostart) · **简体中文**

`./install.sh` **不会**装开机自启项——不同 Wayland 合成器有不同启动机制，正确选择取决于你的工作流。按你的桌面挑一段。

## GNOME / KDE Plasma / Cinnamon / XFCE

创建 `~/.config/autostart/seekey.desktop`：

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

## Hyprland

加到 `~/.config/hypr/hyprland.conf`：

```ini
exec-once = seekey
```

## Sway

加到 `~/.config/sway/config`：

```
exec seekey
```

## niri

加到 `~/.config/niri/config.kdl`：

```kdl
spawn-at-startup "seekey"
```

## river

加到 `~/.config/river/init`：

```sh
riverctl spawn seekey
```

## Wayfire

加到 `~/.config/wayfire.ini`：

```ini
[autostart]
seekey = seekey
```

## labwc

加到 `~/.config/labwc/autostart`（加可执行权限）：

```sh
#!/bin/sh
seekey &
```

## systemd 用户服务（与合成器无关）

创建 `~/.config/systemd/user/seekey.service`：

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

> **提示：** `~/.local/bin`（`install.sh` 默认前缀）在某些登录会话里不在 `$PATH` 上。systemd 单元里用绝对路径（如 `ExecStart=%h/.local/bin/seekey`），或把 `~/.local/bin` 加到 shell rc。

## 启动时传配置 / 参数

按需把命令行参数（见 [[配置#命令行覆盖]]）追加到 `Exec`/`ExecStart`/`spawn`，例如：

```ini
ExecStart=/usr/local/bin/seekey --config /home/you/.config/seekey/config.ini
```
