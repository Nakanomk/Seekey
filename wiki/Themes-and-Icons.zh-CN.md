# 主题与图标

**语言：** [English](Themes-and-Icons) · **简体中文**

## 内置主题

在 `[general]` 里把 `theme` 设成下面之一：

`default` · `light` · `nord` · `dracula` · `catppuccin` · `monokai`

一个主题一次设七个颜色：foreground、background、border、shadow 和三个 placeholder 颜色。`[style]` 里的单个颜色键**覆盖**预设——不写某个键（或注释掉）就回退到预设值。

### 完整色板

| 主题 | foreground | background | border | shadow |
|---|---|---|---|---|
| `default` | `#f7f7f2` | `alpha(#111318, 0.86)` | `alpha(#ffffff, 0.18)` | `0 7px 22px alpha(#000000, 0.30)` |
| `light` | `#1a1a2e` | `alpha(#fafafa, 0.90)` | `alpha(#cccccc, 0.40)` | `0 7px 22px alpha(#000000, 0.12)` |
| `nord` | `#d8dee9` | `alpha(#2e3440, 0.90)` | `alpha(#88c0d0, 0.25)` | `0 7px 22px alpha(#000000, 0.38)` |
| `dracula` | `#f8f8f2` | `alpha(#282a36, 0.90)` | `alpha(#bd93f9, 0.24)` | `0 7px 22px alpha(#000000, 0.38)` |
| `catppuccin` | `#cdd6f4` | `alpha(#1e1e2e, 0.90)` | `alpha(#cba6f7, 0.22)` | `0 7px 22px alpha(#000000, 0.38)` |
| `monokai` | `#f8f8f2` | `alpha(#272822, 0.90)` | `alpha(#a6e22e, 0.22)` | `0 7px 22px alpha(#000000, 0.38)` |

| 主题 | placeholder-fg | placeholder-bg | placeholder-border |
|---|---|---|---|
| `default` | `alpha(#f7f7f2, 0.74)` | `alpha(#111318, 0.56)` | `alpha(#ffffff, 0.14)` |
| `light` | `alpha(#1a1a2e, 0.60)` | `alpha(#e8e8e8, 0.70)` | `alpha(#bbbbbb, 0.35)` |
| `nord` | `alpha(#d8dee9, 0.70)` | `alpha(#2e3440, 0.58)` | `alpha(#88c0d0, 0.16)` |
| `dracula` | `alpha(#f8f8f2, 0.70)` | `alpha(#282a36, 0.58)` | `alpha(#bd93f9, 0.16)` |
| `catppuccin` | `alpha(#cdd6f4, 0.72)` | `alpha(#1e1e2e, 0.58)` | `alpha(#cba6f7, 0.14)` |
| `monokai` | `alpha(#f8f8f2, 0.72)` | `alpha(#272822, 0.58)` | `alpha(#a6e22e, 0.14)` |

## 自定义颜色

`[style]` 里的任何颜色键接受 GTK CSS 颜色值：

```ini
[style]
foreground=#f7f7f2
background=alpha(#111318, 0.86)      # 带 alpha 的 hex
border-color=alpha(#ffffff, 0.18)
shadow=0 7px 22px rgba(0, 0, 0, 0.30)   # 完整 CSS box-shadow
```

可接受的形式：`#rrggbb`、命名 CSS 颜色（`red`、`white`、…）、或 `alpha(#rrggbb, 0.86)`。要跟随壁纸变色，用 `@matugen:<role>`（见 [[Matugen 集成]]）。

## 自定义按键图标

在 `[icons]` 段覆盖某个按键显示的字形。键名匹配气泡标签。最多 64 个覆盖。

```ini
[icons]
Backspace="⌫"
Enter="↵"
Space="␣"
Tab="↹"
Esc="⎋"
Delete="⌦"
Up="↑"
Down="↓"
Left="←"
Right="→"
```

无覆盖时使用的内置默认标签包括可读名称，如 `Backspace`、`Enter`、`Space`、`Tab`、`Esc`、`Delete`、`Caps Lock`、`Num Lock`、`Page Up`、`Volume Up`、`Super`、`AltGr`、方向键、F 键、小键盘键（`Num 7`、`Num +`、…）、媒体键（`Mute`、`Volume Down`、…）、编辑键（`Copy`、`Paste`、`Undo`、`Cut`、…）。完整表见 `src/keynames.c`。

## 从 TUI 选主题

`./seekey --config-tui` 里 `theme` 字段是 `TUI_CHOICE`——按 `Enter` 打开列表选择器实时选预设。见 [[TUI 编辑器]]。
