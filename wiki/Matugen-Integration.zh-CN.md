# Matugen 集成

**语言：** [English](Matugen-Integration) · **简体中文**

[Matugen](https://github.com/InioX/matugen) 能从一张壁纸生成 Material You 配色。seekey 可以在启动时通过在 `seekey.ini` 里引用这些颜色来采用它们。

## 用法

在任何颜色字段里用 `@matugen:<role>` 引用 matugen 角色：

```ini
[style]
foreground=@matugen:on_surface
background=@matugen:surface@0.86
border-color=@matugen:outline
placeholder-foreground=@matugen:on_surface@0.74
```

`@matugen:<role>` 语法会被解析成 matugen `colors.json` 里的 hex 值。可选的 `@<float>` 后缀把结果包进 `alpha(<hex>, <float>)`：

```text
@matugen:surface         → #1a1a1a
@matugen:surface@0.86    → alpha(#1a1a1a, 0.86)
@matugen:missing_key     → 原样保留（让你一眼看到拼错）
```

未知角色会原样保留——你会在气泡里看到字面的 `@matugen:missing_key`，拼错一目了然。

## seekey 去哪找 `colors.json`

解析顺序（先匹配的优先）：

1. 命令行的 `--matugen <path>`
2. `$MATUGEN_COLORS` 环境变量
3. `$XDG_CACHE_HOME/matugen/colors.json`（matugen 默认）
4. `~/.cache/matugen/colors.json`（回退）

## 换壁纸后重新加载

seekey **不会**自动监视 JSON。对一张新壁纸跑完 `matugen` 后，重启 seekey 应用新配色：

```sh
pkill seekey && seekey &
```

## JSON 加载失败

文件缺失或无效时，seekey 向 stderr 打一条警告，然后原样使用 `seekey.ini` 里的值。matugen 集成是完全可选的——没装 matugen seekey 照样跑。
