<div align="center">

# ⌨️ Seekey

**一个轻量的 Wayland 按键可视化工具。**

把最近的按键以悬浮气泡的形式锚定在屏幕底部——一眼看清快捷键、
修饰键和正在输入的文字，又不会打断你的节奏。

![image-20260627131530605](https://img.nkns.cc/2026/06/0de85ce2c0f23714d6743a3e4696fbca.png)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Language: C](https://img.shields.io/badge/Language-C-00599C.svg)
![Platform: Linux](https://img.shields.io/badge/Platform-Linux%20%2F%20Wayland-FCC624.svg)
![GTK4](https://img.shields.io/badge/GTK-4-7FE7FE.svg)
![Version](https://img.shields.io/badge/version-0.2.1-44cc11.svg)

**[English](README.md)** · **[简体中文](README.zh-CN.md)** · **[📖 Wiki](https://github.com/Nakanomk/Seekey/wiki)**

</div>

---

## ✨ 这是什么？

Seekey 是一个 Linux Wayland 下的键盘可视化工具。它监听你的键盘
（以及可选的鼠标），把最近几次按键画成圆角小气泡，贴在屏幕底边。

> 适合用来 **直播 / 录屏**、**现场教学**，或者只是发现你自己误触的
> 修饰键和打错的字。

它**不是**键盘记录器——只临时显示最近几个按键，直接从内核读取输入
（`/dev/input/event*`），并且永远不会抢占键盘焦点。浮层是**点击穿透**
的，不会挡住下面的任何东西。

---

## 🚀 快速上手

```sh
make                       # 编译
./seekey --init-config     # 生成一个 ./seekey.ini 起点（可选）
./seekey --config-gui      # 打开图形配置菜单
./seekey --config-tui      # 在终端里改设置（可选）
./seekey                   # 运行
```

随便按几个键——气泡会出现在屏幕底部。就这样。

> ⚠️ **第一次运行没有气泡？** 多半是没权限读 `/dev/input/event*`。
> 最简单的办法是跑 `./install.sh`（它会自动配好 udev 规则和 `input`
> 用户组）。详见
> [问题排查](https://github.com/Nakanomk/Seekey/wiki/Troubleshooting)。

键盘和鼠标事件节点会在运行时持续监视。输入采集成功启动后，重新插入 USB
键盘、切换扩展坞或更换输入设备都不需要重启 Seekey。

---

## 🌟 亮点

| | |
|---|---|
| 🖥️ **任何 Wayland 合成器都能跑** | 通过 `libevdev` 读输入，不走 Wayland 键盘协议——不需要为每个桌面单独适配。 |
| 📌 **贴边不挡事** | 在 niri / Hyprland / Sway / river / Wayfire / labwc 上用 `gtk4-layer-shell` 钉在底边，切工作区也不动。还会记住上次在哪台显示器。 |
| 🪟 **优雅降级** | 在 GNOME / KDE 上跑成一个透明窗口，并明确告诉你哪些设置在这种模式下不生效。 |
| 🖱️ **点击穿透** | 点击会直接穿透到下面的应用，键盘焦点也永远不会被抢。 |
| 🎨 **开箱即好看** | 七个内置主题（default / nord / dracula / catppuccin / monokai / light / matugen）+ 自定义颜色 + 自定义按键图标。 |
| 🖼️ **认识 Matugen** | 在 GUI/TUI 里选择 Matugen，或直接写 `@matugen:<role>`；配色跟随壁纸，缺失角色会安全回退。 |
| ⚙️ **启动器式 GUI** | 按 fuzzel 布局实现的紧凑可搜索菜单，并用真实 GTK/CSS 布局实时预览样式。 |
| ⌨️ **TUI 配置编辑器** | 在终端里浏览、修改和保存所有设置，并始终保留一个屏上浮层。 |
| 🔒 **输入隐私模式** | 可正常显示打字、用一个固定标签代替整段输入，或隐藏普通字符而保留快捷键。 |

---

## 📦 安装

最省事的是用自带的安装脚本——它会识别发行版、装编译依赖、编译、
安装到 `~/.local/bin`，并配好输入权限：

```sh
./install.sh                # 用户安装（默认）
./install.sh --system       # 系统安装到 /usr/local（需 sudo）
./install.sh --uninstall    # 卸载
```

安装时还会创建 Seekey 桌面入口。第一次从应用菜单启动时，可以选择以后
默认打开设置菜单，或直接启动按键浮层。`seekey --config-gui` 始终会打开设置。

菜单会在存在时读取 `~/.config/fuzzel/fuzzel.ini` 的布局与配色，包括写入
该文件的 Matugen 颜色。无需安装 fuzzel；文件缺失或损坏时会使用安全的内置
默认值。绝对路径和 `~/` 形式的 `include=` 会递归读取，循环引用和过深嵌套
会被安全忽略。

存在 `~/.cache/matugen/colors.json` 时，GUI 根菜单还会提供**使用 Matugen
配色**，用于设置按键浮层本身。显式指定的 `--matugen <path>` 会继续传给实时
预览和由 GUI 启动的浮层，不会退回默认缓存。

编辑器始终只保留一个可见的按键渲染面。真实输入浮层已经运行时，会继续显示它
并跳过样例预览；没有真实浮层时才启动实时样例。GUI 中关闭真实浮层后会立即补上
样例，启动真实浮层前则先销毁样例，不再出现两层显示框。

演示密码框或其他敏感输入时，可以在 GUI/TUI 或配置文件中设置
`typing-display`：

```ini
[general]
typing-display=masked  # full、masked 或 off
```

`masked` 始终只显示一个固定的 `<若干字符>` 气泡，不泄露文字内容或字符
数量；`off` 完全隐藏普通字符，但快捷键组合和非文字按键仍会显示。启用隐私
模式后，`--debug-input` 也不会输出普通字符对应的调试行。

文字拼接目前使用内置的美式 evdev 键位映射。其他布局下的按键名和快捷键仍可
正常显示，但拼接文字可能与非美式布局或输入法最终输入的内容不同；详见
[问题排查](https://github.com/Nakanomk/Seekey/wiki/Troubleshooting#拼接文字与键盘布局不一致)。

想自己手动编译？看 wiki 里的
[从源码编译](https://github.com/Nakanomk/Seekey/wiki/Build-from-Source)。

---

## 📚 接下来去哪

README 故意写得很短，其余内容都在
**[📖 Wiki](https://github.com/Nakanomk/Seekey/wiki)** 里：

**入门**
- [安装](https://github.com/Nakanomk/Seekey/wiki/Installation) —— `install.sh` 选项、各发行版依赖、输入权限
- [从源码编译](https://github.com/Nakanomk/Seekey/wiki/Build-from-Source) —— 手动编译、依赖、Makefile 目标
- [配置](https://github.com/Nakanomk/Seekey/wiki/Configuration) —— 配置文件、查找顺序、常用设置
- [配置项参考](https://github.com/Nakanomk/Seekey/wiki/Configuration-Reference) —— 每个键的类型、范围和默认值
- [GUI 编辑器](https://github.com/Nakanomk/Seekey/wiki/GUI-Editor) —— 图形菜单和 desktop 启动行为
- [TUI 编辑器](https://github.com/Nakanomk/Seekey/wiki/TUI-Editor) —— `--config-tui` 的快捷键

**行为与兼容性**
- [合成器兼容性](https://github.com/Nakanomk/Seekey/wiki/Compositor-Compatibility) —— 各桌面的 layer-shell 与降级情况
- [窗口位置](https://github.com/Nakanomk/Seekey/wiki/Window-Position) —— 锚定、多显示器、点击穿透、GNOME/KDE 固定
- [开机自启](https://github.com/Nakanomk/Seekey/wiki/Autostart) —— 让 seekey 跟你的合成器一起启动

**外观**
- [主题与图标](https://github.com/Nakanomk/Seekey/wiki/Themes-and-Icons) —— 预设、自定义颜色、自定义按键字形
- [Matugen 集成](https://github.com/Nakanomk/Seekey/wiki/Matugen-Integration) —— 跟随壁纸的配色

**给贡献者**
- [架构](https://github.com/Nakanomk/Seekey/wiki/Architecture) —— 代码是怎么组织的 *（看不懂源码就先看这个）*
- [测试](https://github.com/Nakanomk/Seekey/wiki/Testing) —— 跑测试、理解单元测试
- [多语言](https://github.com/Nakanomk/Seekey/wiki/Translations) —— 添加新翻译

**参考**
- [问题排查](https://github.com/Nakanomk/Seekey/wiki/Troubleshooting) —— 输入权限、降级模式、没有气泡等

---

<div align="center">

## 📄 许可证

MIT —— 见 [LICENSE](LICENSE)。

</div>
