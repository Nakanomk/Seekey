# 配置

**语言：** [English](Configuration) · **简体中文**

Seekey 读单个 INI 文件。设置也可以在命令行覆盖。大多数人只会动几个键。

## 配置从哪来

查找顺序（先匹配的优先）：

1. 命令行的 `--config <path>`（最高优先级）
2. `<cwd>/seekey.ini` —— 你运行 seekey 的目录下的项目级文件
3. 内置默认值（不读也不写文件）

**没有**隐式的 `~/.config/` 回退。配置就贴在你运行的二进制旁边，每个项目/目录各自独立。想要全局配置就用 `--config` 指向一个，或者把 `./seekey.ini` 软链过去。

> 想一次性引导一个 XDG 配置？`./seekey --init-config --xdg` 会写一次 `~/.config/seekey/config.ini`。

## 管理配置文件

```sh
./seekey --print-config         # 显示生效配置 + 来源
./seekey --validate-config      # 校验配置并退出
./seekey --init-config          # 用当前设置生成 ./seekey.ini
./seekey --init-config --force  # 覆盖已有文件
./seekey --init-config --xdg    # 一次性引导 ~/.config/seekey/config.ini
./seekey --config-tui           # 在终端里交互式编辑
```

`--print-config` 会打印头部显示配置来源：

```ini
# source: project
# path: /home/you/project/seekey.ini
[general]
...
```

仓库里有完整注释的示例：[`seekey.ini.example`](https://github.com/Nakanomk/Seekey/blob/main/seekey.ini.example)。

## 命令行覆盖

大多数设置可以在命令行设置（覆盖文件值）。完整列表运行 `./seekey --help`。常用项：

| 参数 | 作用 |
|---|---|
| `--config <path>` | 用这个配置文件 |
| `--matugen <path>` | 用这个 matugen `colors.json` |
| `--no-layer-shell` | 强制降级窗口模式 |
| `--debug-input` | 把原始输入事件打到 stderr |
| `--duration <ms>` | 气泡可见时长（100–10000）|
| `--typing-idle <ms>` | 结束一段打字的停顿（100–5000）|
| `--fade-ms <ms>` | 淡出时长（0–3000；0 = 立即）|
| `--margin <px>` | 底边距 |
| `--margin-horizontal <px>` | 侧边距（align=center 时忽略）|
| `--max-items <n>` | 屏上最大气泡数（1–20）|
| `--align left\|center\|right` | 气泡行对齐 |
| `--disappear instant\|fade` | 移除动画 |
| `--layer-shell auto\|required\|off` | layer-shell 模式 |
| `--theme <name>` | 主题预设 |
| `--merge-repeats` / `--no-merge-repeats` | 重复键堆成 "x3" |
| `--merge-modifiers` / `--no-merge-modifiers` | 合并修饰键气泡 |
| `--show-mouse` / `--no-mouse` | 显示鼠标点击/滚动 |
| `-V`、`--version` | 打印版本 |
| `-h`、`--help` | 打印帮助 |

## 常用设置（大多数人只需要这些）

```ini
[general]
duration-ms=1200          # 气泡可见多久
typing-idle-ms=650        # 下一个字符开新气泡前的停顿
fade-ms=180               # 淡出时长（0 = 立即）
margin=96                 # 底边距（layer-shell 模式）
margin-horizontal=0       # 侧边距
max-items=5               # 同时最多气泡数
layer-shell=auto          # auto / required / off
theme=default             # default, nord, dracula, catppuccin, monokai, light
merge-repeats=true        # "Ctrl+C x3" 而不是三个单独气泡
merge-modifiers=true      # "Ctrl" → "Ctrl+C" 而不是两个气泡
show-mouse=false          # 鼠标气泡默认关

[style]
align=right               # right / center / left
disappear=fade            # fade / instant
```

每个键的类型、范围、默认值见 [[Configuration-Reference.zh-CN]]。颜色、主题、按键图标见 [[Themes-and-Icons.zh-CN]]。

## 按键气泡怎么分组

两个行为决定你看到的样式：

- **`merge-repeats`** —— 重复按同一个组合会堆进一个气泡并计数：`[ Ctrl+C ]` → `[ Ctrl+C x3 ]`，而不是三个气泡。
- **`merge-modifiers`** —— 只含修饰键的气泡（`[ Ctrl ]`）在你接着按非修饰键时会被原地扩展成 `[ Ctrl+C ]`，而不是新开一个气泡。
- **打字分组** —— 连续字符键会收进一个文字气泡（`[ hello world ]`）。停顿超过 `typing-idle-ms` 后，下一个字符开新气泡。

## 文件格式注意

- 以 `#` 或 `;` 开头的行是注释。
- 段落是 `[general]`、`[style]`、`[icons]`。
- 布尔值是 `true` / `false`。
- 颜色接受 GTK CSS：`#rrggbb`、命名 CSS 颜色、或 `alpha(#rrggbb, 0.86)`。
- 任何颜色字段都能用 `@matugen:<role>` —— 见 [[Matugen-Integration.zh-CN]]。
