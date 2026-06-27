# 安装

**语言：** [English](Installation) · **简体中文**

仓库自带一个 `install.sh`，一条命令搞定全部：识别发行版、装编译依赖、编译 seekey、安装二进制，并配好输入权限，让它能读 `/dev/input/event*`。

## 快速安装

```sh
./install.sh                # 用户安装（默认）-> ~/.local/bin
./install.sh --system       # 系统安装 -> /usr/local/bin（用 sudo）
```

用户安装后，如果它把你加进了 `input` 组，**注销再登录**一次才生效（脚本会提示你）。

## 选项

| 选项 | 作用 |
|---|---|
| `--user` | 安装到 `$HOME/.local`（默认）|
| `--system` | 安装到 `/usr/local`（用 sudo）|
| `--no-input` | 跳过 udev 规则 + `input` 组配置 |
| `--no-deps` | 不尝试安装编译依赖 |
| `--force` | 覆盖已存在的 udev 规则 |
| `--dry-run` | 只打印将要做什么，不实际执行 |
| `--uninstall` | 卸载一次之前的安装 |
| `-h`、`--help` | 显示帮助 |

## 它一步步做了什么

1. **识别包管理器** —— pacman / dnf / apt / zypper / apk / xbps-install。识别不到的话会打印依赖清单让你手动装。
2. **装编译依赖** —— gtk4、libevdev、ncursesw、json-glib、pkg-config、gcc、make（外加可选的 `gtk4-layer-shell`，见 [[从源码编译]]）。
3. **编译** `make`。
4. **安装** 二进制到 `$PREFIX/bin/seekey`，示例配置到 `$PREFIX/share/seekey/seekey.ini.example`。
5. **配置输入权限**（除非加了 `--no-input`）：
   - 写 `/etc/udev/rules.d/99-seekey.rules`，让 `input` 组能读 `/dev/input/event*`（权限 `0640`）。
   - 不存在 `input` 组就创建，并把当前用户加进去。
   - 重新加载 udev 规则。
6. **打印合成器提示** —— 识别你的桌面，若不支持 `wlr-layer-shell`（GNOME / KDE）会提醒你设 `layer-shell=off`。

> 安装脚本**不会**加开机自启项。见 [[开机自启]]。

## 卸载

```sh
./install.sh --uninstall
```

会删除二进制和数据文件。udev 规则会保留（它无害，且其他输入工具可能共用）——想删就手动删：

```sh
sudo rm /etc/udev/rules.d/99-seekey.rules
```

你的配置和 `input` 组成员资格会保留不动。

## 输入权限（为什么第一次运行没有气泡）

Seekey 直接读内核输入设备。`/dev/input/event*` 节点默认只有 root 能读。安装脚本通过下面两步解决：

- 给 `input` 组读权限（权限 `0640`），并
- 把当前用户加进 `input` 组。

如果你跳过了这步（`--no-input`）或手动编译，有两个选择：

```sh
# 方案 A：和安装脚本做的一样（推荐）
sudo groupadd input                       # 如果组不存在
sudo usermod -aG input "$USER"            # 然后注销再登录
# 再按 install.sh::install_udev_rule 里的内容写 udev 规则

# 方案 B：快但脏（不够安全）
sudo chmod 644 /dev/input/event*
```

强烈推荐方案 A。更多见 [[问题排查]]。
