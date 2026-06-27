# 多语言

**语言：** [English](Translations) · **简体中文**

seekey 用 GNU gettext。Makefile 把 `po/*.po` 编译成 `locale/<lang>/LC_MESSAGES/seekey.mo`，`install.sh` 安装到 `$(PREFIX)/share/locale/<lang>/`。

## 当前语言

| 语言 | 状态 | 文件 |
|---|---|---|
| English (`en`) | 源语言 | 无 |
| 简体中文 (`zh-CN`) | ✓ | `po/zh-CN.po` |

## 添加新翻译

```sh
# 1. 把语言码加到 po/LINGUAS
echo "ja" >> po/LINGUAS

# 2. 从源码提取字符串（需要 xgettext）
make po/seekey.pot

# 3. 初始化新的 .po 文件（需要 msginit）
msginit -l ja -o po/ja.po -i po/seekey.pot

# 4. 用你顺手的编辑器翻译 po/ja.po

# 5. 编译并测试
make
LANG=ja ./seekey
```

## 字符串怎么标记

源字符串用 `_()` 宏（静态初始化用 `N_()`），比如 `g_print(_("Wrote config to %s\n"), path)`。`make pot` 用 `--keyword=_ --keyword=N_` 和 `--add-comments=TRANSLATORS:` 跑 `xgettext` 扫 `src/*.c`，所以面向翻译者的注释会保留。

## 注意

- gettext 域是 `seekey`；`LOCALEDIR` 默认 `/usr/local/share/locale`，构建时可覆盖（Makefile 里 `-DLOCALEDIR=...`）。
- 你的 locale 没装 `.mo` 时，seekey 回退到英文源字符串——照样能跑。
- 翻译版 README 照着现有 `README.zh-CN.md` 模板写。
