# 配置项参考

**语言：** [English](Configuration-Reference) · **简体中文**

每个配置键、所属段、类型、允许范围和默认值。提取自 `src/config.c` 和 `seekey.ini.example`。

注释：以 `#` 或 `;` 开头的行被忽略。布尔值是 `true` / `false`。颜色是 GTK CSS 值（`#rrggbb`、命名颜色、或 `alpha(#rrggbb, 0.86)`）。

## `[general]`

### 常用

| 键 | 类型 | 范围 | 默认值 | 含义 |
|---|---|---|---|---|
| `duration-ms` | uint | 100–10000 | `1200` | 气泡可见多久（毫秒）|
| `typing-idle-ms` | uint | 100–5000 | `650` | 下一个字符开新文字气泡前的停顿 |
| `fade-ms` | uint | 0–3000 | `180` | 淡出时长（0 = 无淡出，立即消失）|
| `margin` | uint | 0–1000 | `0`* | layer-shell 模式下的底边距（像素）|
| `margin-horizontal` | uint | 0–1000 | `0` | 侧边距（`align=center` 时忽略）|
| `max-items` | uint | 1–20 | `5` | 同时最多气泡数；超出的旧气泡被裁掉 |
| `layer-shell` | choice | `auto` / `required` / `off` | `auto` | layer-shell 模式（见 [[合成器兼容性]]）|
| `theme` | string | 预设名 | `default` | 内置主题（见 [[主题与图标]]）|
| `merge-repeats` | bool | — | `true` | 重复相同气泡堆成 "x3" |
| `merge-modifiers` | bool | — | `true` | 有更多按键到来时扩展修饰键气泡 |
| `show-mouse` | bool | — | `false` | 把鼠标点击和滚动显示成气泡 |

\* 代码默认值是 `0`；`seekey.ini.example` 建议 `96` 这个合理值。按需设置，避开你的 dock / 任务栏。

### 窗口（仅降级模式）

仅当 layer-shell 不可用时使用（GNOME / KDE / 没装 gtk4-layer-shell）。

| 键 | 类型 | 范围 | 默认值 | 含义 |
|---|---|---|---|---|
| `window-width` | uint | 240–3000 | `720` | 降级窗口宽度（像素）|
| `window-height` | uint | 80–1000 | `160` | 降级窗口高度（像素）|

## `[style]`

### 布局

| 键 | 类型 | 范围 | 默认值 | 含义 |
|---|---|---|---|---|
| `align` | choice | `left` / `center` / `right` | `right` | 气泡行对齐 |
| `disappear` | choice | `instant` / `fade` | `fade` | 气泡移除动画 |
| `spacing` | uint | 0–80 | `7` | 气泡间距（像素）|
| `overlay-padding` | uint | 0–80 | `12` | 气泡行的外边距 |
| `key-min-width` | uint | 0–300 | `0` | 气泡最小宽度（0 = 按内容）|

### 气泡外观

| 键 | 类型 | 范围 | 默认值 | 含义 |
|---|---|---|---|---|
| `key-padding-x` | uint | 0–80 | `14` | 气泡内水平内边距 |
| `key-padding-y` | uint | 0–80 | `8` | 气泡内垂直内边距 |
| `key-radius` | uint | 0–80 | `12` | 圆角半径 |
| `key-border-width` | uint | 0–20 | `1` | 边框粗细 |
| `key-font-px` | uint | 8–96 | `20` | 字号 |
| `key-font-weight` | uint | 100–1000 | `700` | 字重（CSS 数值）|
| `typing-max-width` | uint | 80–2000 | `480` | 打字气泡最大宽度；超出加 "…"（0 = 无上限）|

### 颜色（覆盖主题预设）

不写某个键（或注释掉）就回退到预设值。

| 键 | 类型 | 默认值（`default` 主题）| 含义 |
|---|---|---|---|
| `foreground` | color | `#f7f7f2` | 气泡文字色 |
| `background` | color | `alpha(#111318, 0.86)` | 气泡背景 |
| `border-color` | color | `alpha(#ffffff, 0.18)` | 气泡边框 |
| `shadow` | shadow | `0 7px 22px alpha(#000000, 0.30)` | GTK CSS box-shadow |
| `placeholder-text` | string | `Seekey` | 空闲占位气泡的文字 |
| `placeholder-foreground` | color | `alpha(#f7f7f2, 0.74)` | 占位文字色 |
| `placeholder-background` | color | `alpha(#111318, 0.56)` | 占位背景 |
| `placeholder-border-color` | color | `alpha(#ffffff, 0.14)` | 占位边框 |

任何颜色字段都能用 `@matugen:<role>` 或 `@matugen:<role>@<alpha>` —— 见 [[Matugen 集成]]。

## `[icons]`（可选）

覆盖某个按键显示的字形。键名匹配气泡标签（`Backspace`、`Enter`、`Space`、`Tab`、`Esc`、`Delete`、`Up`、`Down`、`Left`、`Right` 等）。删掉整段（或注释掉所有键）即用内置默认值。

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

最多支持 64 个图标覆盖（`SEEKEY_MAX_ICON_OVERRIDES`）。

## 主题预设

| 主题 | foreground | background |
|---|---|---|
| `default` | `#f7f7f2` | `alpha(#111318, 0.86)` |
| `light` | `#1a1a2e` | `alpha(#fafafa, 0.90)` |
| `nord` | `#d8dee9` | `alpha(#2e3440, 0.90)` |
| `dracula` | `#f8f8f2` | `alpha(#282a36, 0.90)` |
| `catppuccin` | `#cdd6f4` | `alpha(#1e1e2e, 0.90)` |
| `monokai` | `#f8f8f2` | `alpha(#272822, 0.90)` |

每个预设还设置了边框、阴影、占位颜色。完整色板和覆盖与预设的关系见 [[主题与图标]]。
