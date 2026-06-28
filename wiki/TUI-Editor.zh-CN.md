# TUI 编辑器

**语言：** [English](TUI-Editor) · **简体中文**

`./seekey --config-tui` 打开一个终端 UI，可以浏览并编辑所有设置，不用手编 ini 文件。

**顶部**显示当前配置文件路径，有未提交改动时会出现 `[Unsaved]` 标记。

设置按 4 个 tab 分组：**General / Layout / Appearance / Placeholder**。

## 键位

| 键 | 作用 |
|---|---|
| `Up` / `Down` 或 `k` / `j` | 在当前 tab 内选择字段 |
| `Left` / `Right` 或 `h` / `l` | 调整数值 / 选项 / 颜色 / 布尔 |
| `Tab` / `Shift-Tab` | 下一个 / 上一个 tab |
| `g` | 跳到第一个 tab；`G` 跳到最后一个 tab |
| `Enter` | 编辑（数字 / 字符串 / 颜色）或打开选择器（选项 / 主题 / 颜色）|
| `s` | 保存到当前路径 |
| `S` | 另存为新路径 |
| `r` | 当前字段重置为默认值 |
| `R` | 全部字段重置为默认值（先确认）|
| `L` | 从磁盘重新加载（丢弃未保存改动，先确认）|
| `W` | 重置窗口状态（忘记保存的显示器）|
| `?` | 显示完整帮助 |
| `q` / `Esc` | 退出（有未保存改动时先问是否保存）|

## 字段类型

- **`TUI_CHOICE`** 字段（`align`、`disappear`、`layer-shell`、`theme`）—— `Enter` 打开列表选择器，不用记选项名。
- **颜色字段** —— `Enter` 打开 24 色调色板；按 `c` 输入自定义 `#rrggbb` 值。非法 hex 会被拒绝并提示错误。
- **数字 / 字符串 / 布尔** —— 用 `Left`/`Right` 调整，或 `Enter` 输入值。

## 窗口状态重置（`W`）

多显示器时，seekey 会记住上次在哪台显示器（存在 `$XDG_STATE_HOME/seekey/window.ini`）。按 `W` 清除，下次启动用当前聚焦的输出。见 [[Window-Position.zh-CN]]。
