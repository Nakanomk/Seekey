# Translations

Seekey uses GNU gettext. The Makefile compiles `po/*.po` into
`locale/<lang>/LC_MESSAGES/seekey.mo`, and `install.sh` installs them under
`$(PREFIX)/share/locale/<lang>/`.

## Current languages

| Language | Status | File |
|---|---|---|
| English (`en`) | Source language | n/a |
| 简体中文 (`zh-CN`) | ✓ | `po/zh-CN.po` |

## Add a new translation

```sh
# 1. Add the language code to po/LINGUAS
echo "ja" >> po/LINGUAS

# 2. Extract the source strings (needs xgettext)
make po/seekey.pot

# 3. Initialize a new .po file (needs msginit)
msginit -l ja -o po/ja.po -i po/seekey.pot

# 4. Translate po/ja.po with your editor of choice

# 5. Build and test
make
LANG=ja ./seekey
```

## How strings are marked

Source strings use the `_()` macro (and `N_()` for static initializers),
e.g. `g_print(_("Wrote config to %s\n"), path)`. `make pot` runs `xgettext`
over `src/*.c` with `--keyword=_ --keyword=N_` and
`--add-comments=TRANSLATORS:` so translator-facing comments are preserved.

## Notes

- The gettext domain is `seekey`; `LOCALEDIR` defaults to
  `/usr/local/share/locale` and is overridable at build time
  (`-DLOCALEDIR=...` in the Makefile).
- If no `.mo` is installed for your locale, seekey falls back to the English
  source strings — it still works.
- A translated README follows the existing `README.zh-CN.md` template.
