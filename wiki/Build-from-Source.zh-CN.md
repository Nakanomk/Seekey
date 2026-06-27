# 从源码编译

**语言：** [English](Build-from-Source) · **简体中文**

不想用 `install.sh`、想自己手动编译？这里是步骤。

## 1. 安装依赖

| 发行版 | 命令 |
|---|---|
| Arch | `sudo pacman -S gtk4 libevdev json-glib ncurses pkgconf gcc make` |
| Fedora | `sudo dnf install gtk4-devel libevdev-devel json-glib-devel ncurses-devel pkgconf-pkg-config gcc make` |
| Debian / Ubuntu | `sudo apt install libgtk-4-dev libevdev-dev libjson-glib-dev libncursesw5-dev pkg-config build-essential` |
| openSUSE | `sudo zypper install gtk4-devel libevdev-devel ncurses-devel json-glib-devel pkg-config gcc make` |
| Alpine | `sudo apk add gtk4-dev libevdev-dev ncurses-dev json-glib-dev pkgconf gcc make musl-dev` |
| Void | `sudo xbps-install gtk4-devel libevdev-devel ncurses-devel json-glib-devel pkg-config gcc make` |

### 可选：`gtk4-layer-shell`

它让 seekey 能锚定在屏幕底边、切工作区也不动（niri / Hyprland / Sway / river / Wayfire / labwc）。没有它，seekey 会降级成一个普通透明窗口（见 [[合成器兼容性]]）。

```sh
# Arch
sudo pacman -S gtk4-layer-shell
# Fedora
sudo dnf install gtk4-layer-shell-devel
```

Debian/Ubuntu 暂无官方包——见
[gtk4-layer-shell](https://github.com/wmww/gtk4-layer-shell) 项目的编译说明。没有它 seekey 也能跑（降级模式）。

## 2. 编译

```sh
make                # 编译 seekey
```

Makefile 会通过 pkg-config 自动检测 `gtk4-layer-shell`（同时探测 `gtk4-layer-shell` 和 `gtk4-layer-shell-0` 两个 pc 文件名），并把它链接在 **GTK4 之前**。这个顺序是必须的：gtk4-layer-shell 注册的静态构造函数必须在 libwayland-client 初始化前运行，否则 `gtk_layer_init_for_window()` 会失败。

## 3. 运行

```sh
./seekey                       # 从编译目录运行
./seekey --init-config         # 用默认值生成 ./seekey.ini
./seekey --config-tui          # 交互式改设置
```

## 4.（可选）手动安装

```sh
make install        # 安装到 PREFIX（默认 /usr/local）
make uninstall      # 删除已安装文件
```

改安装前缀：

```sh
make install PREFIX=$HOME/.local
```

`make install` 会把二进制放到 `$PREFIX/bin/seekey`、示例配置放到 `$PREFIX/share/seekey/seekey.ini.example`、翻译放到 `$PREFIX/share/locale/<lang>/`。

输入权限仍需自己配——见 [[安装#输入权限]]。

## Makefile 目标

| 目标 | 作用 |
|---|---|
| `make` / `make all` | 编译 `seekey` + 编译翻译（`.mo`）|
| `make check` | 编译并运行单元测试 |
| `make install` | 安装二进制、配置示例、翻译 |
| `make uninstall` | 删除已安装文件 |
| `make pot` | 从源码重新生成 `po/seekey.pot`（需要 `xgettext`）|
| `make mo` | 把 `.po` 编译成 `.mo`，放到 `locale/` |
| `make format` | 对 `src/` 和 `tests/` 跑 `clang-format`（若已安装）|
| `make clean` | 清理编译产物 + `locale/` |

测试见 [[测试]]，i18n 流程见 [[多语言]]。
