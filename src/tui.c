#include "seekey.h"
#include "tui.h"
#include "config.h"
#include "window_state.h"

#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef SEEKEY_TEST
#include <ncurses.h>
#endif

/* ------------------------------------------------------------------ */
/* Field model                                                         */
/* ------------------------------------------------------------------ */

static const char *ALIGN_CHOICES[]    = {"left", "center", "right"};
static const char *DISAPPEAR_CHOICES[] = {"instant", "fade"};

/* ------------------------------------------------------------------ */
/* Color palette (used by picker and preview)                          */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *hex;
    short r, g, b;
} TuiPaletteEntry;

static const TuiPaletteEntry TUI_PALETTE[] = {
    {"#ffffff", 255, 255, 255}, {"#cccccc", 204, 204, 204},
    {"#999999", 153, 153, 153}, {"#666666", 102, 102, 102},
    {"#333333",  51,  51,  51}, {"#111318",  17,  19,  24},
    {"#f7f7f2", 247, 247, 242}, {"#1a1a2e",  26,  26,  46},
    {"#ff5555", 255,  85,  85}, {"#ee5a24", 238,  90,  36},
    {"#f0932b", 240, 147,  43}, {"#f9ca24", 249, 202,  36},
    {"#a6e22e", 166, 226,  46}, {"#2ecc71",  46, 204, 113},
    {"#1abc9c",  26, 188, 156}, {"#74b9ff", 116, 185, 255},
    {"#0984e3",   9, 132, 227}, {"#5e81ac",  94, 129, 172},
    {"#a29bfe", 162, 155, 254}, {"#bd93f9", 189, 147, 249},
    {"#cba6f7", 203, 166, 247}, {"#d8dee9", 216, 222, 233},
    {"#88c0d0", 136, 192, 208}, {"#cdd6f4", 205, 214, 244},
};

#define TUI_PALETTE_COLS  6
#define TUI_PALETTE_ROWS  ((int)(sizeof(TUI_PALETTE) / sizeof(TUI_PALETTE[0]) / TUI_PALETTE_COLS))
#define TUI_COLOR_PAIR_BASE  32

/* ------------------------------------------------------------------ */
/* Pure functions (testable, no ncurses)                               */
/* ------------------------------------------------------------------ */

int tui_nearest_color_index(const char *hex);

guint tui_current_choice_index(const TuiField *field)
{
    for (guint i = 0; i < field->choice_count; i++) {
        if (g_strcmp0(field->string_target, field->choices[i]) == 0) {
            return i;
        }
    }
    return 0;
}

void tui_field_value(const TuiField *field, char *buffer, gsize size)
{
    switch (field->type) {
    case TUI_UINT:
        g_snprintf(buffer, size, "%u", *field->uint_target);
        break;
    case TUI_BOOL:
        g_strlcpy(buffer, *field->bool_target ? "yes" : "no", size);
        break;
    case TUI_COLOR:
    case TUI_STRING:
    case TUI_CHOICE:
        g_strlcpy(buffer, field->string_target, size);
        break;
    }
}

void tui_adjust_field(TuiField *field, int direction)
{
    if (field->type == TUI_UINT) {
        guint value = *field->uint_target;
        guint step = field->step > 0 ? field->step : 1;
        if (direction < 0) {
            value = value > field->min + step ? value - step : field->min;
        } else {
            value = value + step < field->max ? value + step : field->max;
        }
        *field->uint_target = value;
        return;
    }

    if (field->type == TUI_BOOL) {
        *field->bool_target = !*field->bool_target;
        return;
    }

    if (field->type == TUI_CHOICE) {
        guint index = tui_current_choice_index(field);
        if (direction < 0) {
            index = index == 0 ? field->choice_count - 1 : index - 1;
        } else {
            index = (index + 1) % field->choice_count;
        }
        g_strlcpy(field->string_target, field->choices[index], field->string_size);
        return;
    }

    if (field->type == TUI_COLOR) {
        int idx = tui_nearest_color_index(field->string_target);
        int count = (int)(sizeof(TUI_PALETTE) / sizeof(TUI_PALETTE[0]));
        idx = (idx + direction + count) % count;
        g_strlcpy(field->string_target, TUI_PALETTE[idx].hex, field->string_size);
    }
}

int tui_nearest_color_index(const char *hex)
{
    if (hex == NULL) {
        return 0;
    }
    for (gsize i = 0; i < sizeof(TUI_PALETTE) / sizeof(TUI_PALETTE[0]); i++) {
        if (g_ascii_strcasecmp(hex, TUI_PALETTE[i].hex) == 0) {
            return (int)i;
        }
    }
    guint rr = 128, gg = 128, bb = 128;
    if (hex[0] == '#' && strlen(hex) == 7) {
        unsigned long v = g_ascii_strtoull(hex + 1, NULL, 16);
        rr = (v >> 16) & 0xff;
        gg = (v >> 8) & 0xff;
        bb = v & 0xff;
    }
    int best = 0, best_dist = 0x7fffffff;
    for (gsize i = 0; i < sizeof(TUI_PALETTE) / sizeof(TUI_PALETTE[0]); i++) {
        int dr = (int)rr - (int)TUI_PALETTE[i].r;
        int dg = (int)gg - (int)TUI_PALETTE[i].g;
        int db = (int)bb - (int)TUI_PALETTE[i].b;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < best_dist) {
            best_dist = dist;
            best = (int)i;
        }
    }
    return best;
}

void tui_reset_field(TuiField *field)
{
    switch (field->type) {
    case TUI_UINT:
        *field->uint_target = field->default_uint;
        break;
    case TUI_BOOL:
        *field->bool_target = field->default_bool;
        break;
    case TUI_STRING:
    case TUI_CHOICE:
    case TUI_COLOR:
        g_strlcpy(field->string_target, field->default_string, field->string_size);
        break;
    }
}

void tui_build_fields(TuiField *out, size_t *out_count, SeekeyConfig *config)
{
    static const char *LAYER_CHOICES[] = {"auto", "required", "off"};
    static const char *THEME_CHOICES[] = {"default", "nord", "dracula",
                                          "catppuccin", "monokai", "light"};

    TuiField *f = out;
    size_t i = 0;

#define UINT_FIELD(LABEL, HELP, HINT, TARGET, MIN, MAX, STEP)        \
    do { f[i] = (TuiField){0};                                       \
         f[i].label = (LABEL); f[i].help = (HELP);                   \
         f[i].input_hint = (HINT);                                   \
         f[i].type = TUI_UINT;                                       \
         f[i].uint_target = &(config->TARGET);                       \
         f[i].min = (MIN); f[i].max = (MAX); f[i].step = (STEP);     \
         f[i].default_uint = config->TARGET;                         \
         i++; } while (0)

#define BOOL_FIELD(LABEL, HELP, HINT, TARGET)                        \
    do { f[i] = (TuiField){0};                                       \
         f[i].label = (LABEL); f[i].help = (HELP);                   \
         f[i].input_hint = (HINT);                                   \
         f[i].type = TUI_BOOL;                                       \
         f[i].bool_target = &(config->TARGET);                       \
         f[i].default_bool = config->TARGET;                         \
         i++; } while (0)

#define CHOICE_FIELD(LABEL, HELP, HINT, TARGET, SZ, CHOICES, COUNT)  \
    do { f[i] = (TuiField){0};                                       \
         f[i].label = (LABEL); f[i].help = (HELP);                   \
         f[i].input_hint = (HINT);                                   \
         f[i].type = TUI_CHOICE;                                     \
         f[i].string_target = (TARGET); f[i].string_size = (SZ);     \
         f[i].choices = (CHOICES); f[i].choice_count = (COUNT);      \
         g_strlcpy(f[i].default_string, (TARGET), sizeof(f[i].default_string)); \
         i++; } while (0)

#define STRING_FIELD(LABEL, HELP, HINT, TARGET, SZ)                  \
    do { f[i] = (TuiField){0};                                       \
         f[i].label = (LABEL); f[i].help = (HELP);                   \
         f[i].input_hint = (HINT);                                   \
         f[i].type = TUI_STRING;                                     \
         f[i].string_target = (TARGET); f[i].string_size = (SZ);     \
         g_strlcpy(f[i].default_string, (TARGET), sizeof(f[i].default_string)); \
         i++; } while (0)

#define COLOR_FIELD(LABEL, HELP, HINT, TARGET, SZ)                   \
    do { f[i] = (TuiField){0};                                       \
         f[i].label = (LABEL); f[i].help = (HELP);                   \
         f[i].input_hint = (HINT);                                   \
         f[i].type = TUI_COLOR;                                      \
         f[i].string_target = (TARGET); f[i].string_size = (SZ);     \
         g_strlcpy(f[i].default_string, (TARGET), sizeof(f[i].default_string)); \
         i++; } while (0)

    UINT_FIELD("duration-ms", "How long key bubbles stay visible.",
               "integer 100..10000", duration_ms, 100, 10000, 100);
    UINT_FIELD("typing-idle-ms", "Pause before typing starts a new bubble.",
               "integer 100..5000", typing_idle_ms, 100, 5000, 50);
    UINT_FIELD("fade-ms", "Fade animation length. 0 disables fade delay.",
               "integer 0..3000", fade_ms, 0, 3000, 50);
    UINT_FIELD("margin", "Bottom layer-shell margin.",
               "integer 0..1000", margin_px, 0, 1000, 8);
    UINT_FIELD("margin-horizontal", "Horizontal margin for left/right anchor.",
               "integer 0..1000", margin_horizontal_px, 0, 1000, 8);
    UINT_FIELD("max-items", "Maximum bubbles visible at once.",
               "integer 1..20", max_items, 1, 20, 1);
    UINT_FIELD("window-width", "Fallback window width.",
               "integer 240..3000", window_width, 240, 3000, 20);
    UINT_FIELD("window-height", "Fallback window height.",
               "integer 80..1000", window_height, 80, 1000, 10);

    CHOICE_FIELD("layer-shell",
                 "auto falls back to a window; required exits if unavailable.",
                 "pick: auto / required / off",
                 config->layer_shell, sizeof(config->layer_shell),
                 LAYER_CHOICES,
                 (guint)(sizeof(LAYER_CHOICES) / sizeof(LAYER_CHOICES[0])));
    CHOICE_FIELD("theme",
                 "Color preset (overridable per key in [style]).",
                 "pick a theme preset",
                 config->theme, sizeof(config->theme),
                 THEME_CHOICES,
                 (guint)(sizeof(THEME_CHOICES) / sizeof(THEME_CHOICES[0])));

    BOOL_FIELD("merge-repeats", "Show repeated keys as Key xN.",
               "toggle yes/no", merge_repeats);
    BOOL_FIELD("merge-modifiers", "Update modifier bubble when combo extends.",
               "toggle yes/no", merge_modifiers);
    BOOL_FIELD("show-mouse", "Show mouse button clicks as bubbles.",
               "toggle yes/no", show_mouse);

    CHOICE_FIELD("align", "Bubble row alignment.",
                 "pick: left / center / right",
                 config->align, sizeof(config->align),
                 ALIGN_CHOICES,
                 (guint)(sizeof(ALIGN_CHOICES) / sizeof(ALIGN_CHOICES[0])));
    CHOICE_FIELD("disappear", "Bubble removal mode.",
                 "pick: instant / fade",
                 config->disappear, sizeof(config->disappear),
                 DISAPPEAR_CHOICES,
                 (guint)(sizeof(DISAPPEAR_CHOICES) / sizeof(DISAPPEAR_CHOICES[0])));

    UINT_FIELD("spacing", "Space between bubbles.",
               "integer 0..80", box_spacing, 0, 80, 1);
    UINT_FIELD("overlay-padding", "Outer padding around the bubble row.",
               "integer 0..80", overlay_padding, 0, 80, 1);
    UINT_FIELD("key-min-width", "Minimum bubble width (0 = content-based).",
               "integer 0..300", key_min_width, 0, 300, 1);
    UINT_FIELD("key-padding-x", "Horizontal key padding.",
               "integer 0..80", key_padding_x, 0, 80, 1);
    UINT_FIELD("key-padding-y", "Vertical key padding.",
               "integer 0..80", key_padding_y, 0, 80, 1);
    UINT_FIELD("key-radius", "Bubble corner radius.",
               "integer 0..80", key_radius, 0, 80, 1);
    UINT_FIELD("key-border-width", "Bubble border width.",
               "integer 0..20", key_border_width, 0, 20, 1);
    UINT_FIELD("key-font-px", "Bubble font size.",
               "integer 8..96", key_font_px, 8, 96, 1);
    UINT_FIELD("key-font-weight", "Bubble font weight.",
               "integer 100..1000", key_font_weight, 100, 1000, 100);
    UINT_FIELD("typing-max-width", "Max width for grouped typing bubbles (≈chars; triggers ellipsize).",
               "integer 80..2000", typing_max_width, 80, 2000, 20);

    COLOR_FIELD("foreground", "GTK CSS text color.",
                "GTK CSS color: #rrggbb, named, or alpha(#rrggbb, A)",
                config->foreground, sizeof(config->foreground));
    COLOR_FIELD("background", "GTK CSS background.",
                "GTK CSS color: #rrggbb, named, or alpha(#rrggbb, A)",
                config->background, sizeof(config->background));
    COLOR_FIELD("border-color", "GTK CSS border color.",
                "GTK CSS color: #rrggbb, named, or alpha(#rrggbb, A)",
                config->border_color, sizeof(config->border_color));
    STRING_FIELD("shadow", "GTK CSS box-shadow.",
                 "GTK CSS box-shadow, e.g. '0 7px 22px rgba(0,0,0,0.30)'",
                 config->shadow, sizeof(config->shadow));
    STRING_FIELD("placeholder-text", "Startup placeholder text.",
                 "Any short text shown when no keys are pressed",
                 config->placeholder_text, sizeof(config->placeholder_text));
    COLOR_FIELD("placeholder-foreground", "Placeholder text color.",
                "GTK CSS color: #rrggbb, named, or alpha(#rrggbb, A)",
                config->placeholder_foreground,
                sizeof(config->placeholder_foreground));
    COLOR_FIELD("placeholder-background", "Placeholder background.",
                "GTK CSS color: #rrggbb, named, or alpha(#rrggbb, A)",
                config->placeholder_background,
                sizeof(config->placeholder_background));
    COLOR_FIELD("placeholder-border-color", "Placeholder border color.",
                "GTK CSS color: #rrggbb, named, or alpha(#rrggbb, A)",
                config->placeholder_border_color,
                sizeof(config->placeholder_border_color));

#undef UINT_FIELD
#undef BOOL_FIELD
#undef CHOICE_FIELD
#undef STRING_FIELD
#undef COLOR_FIELD

    *out_count = i;
    g_assert(i == TUI_FIELD_COUNT);
}

/* ------------------------------------------------------------------ */
/* ncurses-based interactive UI (compiled in only outside test build)  */
/* ------------------------------------------------------------------ */

#ifndef SEEKEY_TEST

static void tui_init_colors(void)
{
    if (!has_colors() || !can_change_color()) return;
    start_color();
    for (gsize i = 0; i < sizeof(TUI_PALETTE) / sizeof(TUI_PALETTE[0]); i++) {
        short r = (short)(TUI_PALETTE[i].r * 1000 / 255);
        short g = (short)(TUI_PALETTE[i].g * 1000 / 255);
        short b = (short)(TUI_PALETTE[i].b * 1000 / 255);
        init_color((short)(TUI_COLOR_PAIR_BASE + i), r, g, b);
        init_pair((short)(TUI_COLOR_PAIR_BASE + i),
                  (short)(TUI_COLOR_PAIR_BASE + i),
                  (short)(TUI_COLOR_PAIR_BASE + i));
    }
}

static void tui_draw_color_swatch(int y, int x, const char *hex, int w)
{
    int idx = tui_nearest_color_index(hex);
    if (has_colors()) {
        attron(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
        for (int i = 0; i < w; i++) mvprintw(y, x + i, " ");
        attroff(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
    } else {
        mvprintw(y, x, "[#]");
    }
}

typedef struct {
    SeekeyConfig *config;
    TuiField *fields;
    size_t field_count;
    int selected;
    int scroll;
    gboolean running;
    gboolean dirty;
    char saved_path[512];
    gboolean show_help;
    char status_line[160];
    int status_ttl;
} TuiState;

static void tui_set_status(TuiState *st, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    g_vsnprintf(st->status_line, sizeof(st->status_line), fmt, ap);
    va_end(ap);
    st->status_ttl = 4;  /* redraws */
}

static gboolean tui_prompt_string(const char *label, const char *hint,
                                  char *out, gsize out_size, const char *current)
{
    echo();
    curs_set(1);
    int row = LINES - 4;
    move(row, 0); clrtoeol();
    if (hint != NULL && hint[0] != '\0') {
        mvprintw(row, 0, "%s\n  %s\n  New value (Enter=keep \"%s\"): ",
                 label, hint, current ? current : "");
    } else {
        mvprintw(row, 0, "%s\n  New value (Enter=keep \"%s\"): ",
                 label, current ? current : "");
    }
    char buf[512] = {0};
    getnstr(buf, (int)sizeof(buf) - 1);
    noecho();
    curs_set(0);
    if (buf[0] == '\0') {
        return FALSE;
    }
    g_strlcpy(out, buf, out_size);
    return TRUE;
}

static gboolean tui_prompt_uint(const char *label, guint min, guint max,
                                guint current, guint *out)
{
    echo();
    curs_set(1);
    int row = LINES - 3;
    move(row, 0); clrtoeol();
    mvprintw(row, 0, "%s (integer %u..%u, current %u): ",
             label, min, max, current);
    char buf[64] = {0};
    getnstr(buf, (int)sizeof(buf) - 1);
    noecho();
    curs_set(0);
    if (buf[0] == '\0') {
        return FALSE;
    }
    char *end = NULL;
    unsigned long v = strtoul(buf, &end, 10);
    if (*buf == '\0' || *end != '\0' || v < min || v > max) {
        return FALSE;
    }
    *out = (guint)v;
    return TRUE;
}

static gboolean valid_hex(const char *s)
{
    if (s == NULL || s[0] != '#' || strlen(s) != 7) return FALSE;
    for (int i = 1; i < 7; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean tui_prompt_color(const char *label, const char *hint,
                                 const char *current,
                                 char *out, gsize out_size)
{
    echo();
    curs_set(1);
    int row = LINES - 4;
    move(row, 0); clrtoeol();
    if (hint != NULL && hint[0]) {
        mvprintw(row, 0, "%s\n  %s\n  GTK CSS color (current %s): ",
                 label, hint, current ? current : "");
    } else {
        mvprintw(row, 0, "%s\n  GTK CSS color (current %s): ",
                 label, current ? current : "");
    }
    char buf[256] = {0};
    getnstr(buf, (int)sizeof(buf) - 1);
    noecho();
    curs_set(0);
    if (buf[0] == '\0') {
        return FALSE;
    }
    g_strlcpy(out, buf, out_size);
    return TRUE;
}

static gboolean tui_choice_picker(TuiState *st, TuiField *field)
{
    const char *const *choices = field->choices;
    guint count = field->choice_count;
    int sel = (int)tui_current_choice_index(field);
    gboolean done = FALSE, picked = FALSE;
    while (!done) {
        erase();
        mvprintw(0, 2, "Pick %s (arrows, enter, esc)", field->label);
        for (guint i = 0; i < count; i++) {
            int y = 3 + (int)i;
            if ((int)i == sel) attron(A_REVERSE);
            mvprintw(y, 2, " %c %s", (int)i == sel ? '>' : ' ', choices[i]);
            if ((int)i == sel) attroff(A_REVERSE);
        }
        refresh();
        int ch = getch();
        switch (ch) {
        case KEY_UP: case 'k':
            sel = (sel - 1 + (int)count) % (int)count; break;
        case KEY_DOWN: case 'j':
            sel = (sel + 1) % (int)count; break;
        case '\n': case '\r': case KEY_ENTER:
            g_strlcpy(field->string_target, choices[sel], field->string_size);
            g_strlcpy(field->default_string, choices[sel],
                      sizeof(field->default_string));
            picked = TRUE; done = TRUE; break;
        case 27: case 'q':
            done = TRUE; break;
        }
    }
    if (picked) {
        st->dirty = TRUE;
    }
    return picked;
}

static gboolean tui_theme_picker(TuiState *st, SeekeyConfig *config)
{
    static const char *THEME_FG[]   = {"#f7f7f2","#d8dee9","#f8f8f2","#cdd6f4","#f8f8f2","#1a1a2e"};
    static const char *THEME_BG[]   = {"#111318","#2e3440","#282a36","#1e1e2e","#272822","#fafafa"};
    static const char *THEME_BD[]   = {"#ffffff","#88c0d0","#bd93f9","#cba6f7","#a6e22e","#cccccc"};
    gsize n = seekey_config_theme_count();
    int sel = 0;
    for (gsize i = 0; i < n; i++) {
        if (g_strcmp0(config->theme, seekey_config_theme_at(i)->name) == 0) {
            sel = (int)i; break;
        }
    }
    gboolean done = FALSE, picked = FALSE;
    while (!done) {
        erase();
        mvprintw(0, 2, "Pick a theme (arrows, enter, esc)");
        for (gsize i = 0; i < n; i++) {
            int y = 3 + (int)i;
            if ((int)i == sel) attron(A_REVERSE);
            mvprintw(y, 2, " %c %-12s", (int)i == sel ? '>' : ' ',
                     seekey_config_theme_at(i)->name);
            if ((int)i == sel) attroff(A_REVERSE);
            tui_draw_color_swatch(y + 1,  5, THEME_FG[i], 4);
            tui_draw_color_swatch(y + 1, 10, THEME_BG[i], 4);
            tui_draw_color_swatch(y + 1, 15, THEME_BD[i], 4);
            mvprintw(y + 1, 18, "fg / bg / border");
        }
        refresh();
        int ch = getch();
        switch (ch) {
        case KEY_UP: case 'k':
            sel = (sel - 1 + (int)n) % (int)n; break;
        case KEY_DOWN: case 'j':
            sel = (sel + 1) % (int)n; break;
        case '\n': case '\r': case KEY_ENTER: {
            const char *name = seekey_config_theme_at(sel)->name;
            g_strlcpy(config->theme, name, sizeof(config->theme));
            seekey_config_apply_theme(config, name);
            /* Resync the theme field's default_string. */
            for (size_t j = 0; j < st->field_count; j++) {
                if (g_strcmp0(st->fields[j].label, "theme") == 0) {
                    g_strlcpy(st->fields[j].default_string, name,
                              sizeof(st->fields[j].default_string));
                    break;
                }
            }
            picked = TRUE; done = TRUE;
            break;
        }
        case 27: case 'q':
            done = TRUE; break;
        }
    }
    if (picked) st->dirty = TRUE;
    return picked;
}

static gboolean tui_color_picker(TuiState *st, TuiField *field)
{
    int cur_idx = tui_nearest_color_index(field->string_target);
    int sel_row = cur_idx / TUI_PALETTE_COLS;
    int sel_col = cur_idx % TUI_PALETTE_COLS;
    gboolean done = FALSE, picked = FALSE;
    char status[64] = {0};

    while (!done) {
        erase();
        mvprintw(0, 2, "Pick color for: %s", field->label);
        mvprintw(1, 2, "Arrows: navigate  Enter: pick  c: custom hex  Esc: cancel");
        for (int row = 0; row < TUI_PALETTE_ROWS; row++) {
            for (int col = 0; col < TUI_PALETTE_COLS; col++) {
                int idx = row * TUI_PALETTE_COLS + col;
                int x = 4 + col * 6;
                int y = 4 + row * 2;
                if (has_colors()) {
                    attron(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
                    mvprintw(y, x, "    ");
                    attroff(COLOR_PAIR(TUI_COLOR_PAIR_BASE + idx));
                }
                if (row == sel_row && col == sel_col) {
                    mvprintw(y, x - 2, "[");
                    mvprintw(y, x + 4, "]");
                }
                mvprintw(y + 1, x, "%-4s", TUI_PALETTE[idx].hex + 4);
            }
        }
        int info_y = 4 + TUI_PALETTE_ROWS * 2 + 1;
        int cur = sel_row * TUI_PALETTE_COLS + sel_col;
        mvprintw(info_y, 2, "Selected: ");
        if (has_colors()) {
            attron(COLOR_PAIR(TUI_COLOR_PAIR_BASE + cur));
            mvprintw(info_y, 12, "    ");
            attroff(COLOR_PAIR(TUI_COLOR_PAIR_BASE + cur));
        }
        mvprintw(info_y, 17, " %s", TUI_PALETTE[cur].hex);
        if (status[0]) {
            attron(A_BOLD);
            mvprintw(info_y + 2, 2, "%s", status);
            attroff(A_BOLD);
        }
        refresh();

        int ch = getch();
        status[0] = '\0';
        switch (ch) {
        case KEY_UP: case 'k':
            sel_row = (sel_row - 1 + TUI_PALETTE_ROWS) % TUI_PALETTE_ROWS; break;
        case KEY_DOWN: case 'j':
            sel_row = (sel_row + 1) % TUI_PALETTE_ROWS; break;
        case KEY_LEFT: case 'h':
            sel_col = (sel_col - 1 + TUI_PALETTE_COLS) % TUI_PALETTE_COLS; break;
        case KEY_RIGHT: case 'l':
            sel_col = (sel_col + 1) % TUI_PALETTE_COLS; break;
        case 'c': case 'C': {
            char new_color[64];
            g_strlcpy(new_color, field->string_target, sizeof(new_color));
            if (tui_prompt_color(field->label, field->input_hint,
                                 field->string_target,
                                 new_color, sizeof(new_color))) {
                if (valid_hex(new_color)) {
                    g_strlcpy(field->string_target, new_color,
                              field->string_size);
                    g_strlcpy(field->default_string, new_color,
                              sizeof(field->default_string));
                    picked = TRUE;
                    done = TRUE;
                } else {
                    g_strlcpy(status, "Invalid hex, try #rrggbb", sizeof(status));
                }
            }
            break;
        }
        case '\n': case '\r': case KEY_ENTER:
            g_strlcpy(field->string_target, TUI_PALETTE[cur].hex,
                      field->string_size);
            g_strlcpy(field->default_string, TUI_PALETTE[cur].hex,
                      sizeof(field->default_string));
            picked = TRUE; done = TRUE;
            break;
        case 27: case 'q':
            done = TRUE; break;
        }
    }
    if (picked) st->dirty = TRUE;
    return picked;
}

static void tui_draw_preview(const SeekeyConfig *config)
{
    int top = LINES - 8;
    if (top < 10) return;

    char sample[160];
    g_snprintf(sample, sizeof(sample), "[ %s ]  [ Ctrl + C x3 ]",
               config->placeholder_text);

    int x = 2;
    int width = (int)strlen(sample);
    if (g_strcmp0(config->align, "center") == 0) {
        x = MAX(2, (COLS - width) / 2);
    } else if (g_strcmp0(config->align, "right") == 0) {
        x = MAX(2, COLS - width - 3);
    }

    mvhline(top, 0, ACS_HLINE, COLS);
    mvprintw(top + 1, 2, "Preview");
    attron(A_BOLD);
    mvprintw(top + 3, x, "%s", sample);
    attroff(A_BOLD);
    mvprintw(top + 5, 2,
             "Disappear: %s, duration %ums, fade %ums",
             config->disappear, config->duration_ms, config->fade_ms);
}

static void tui_draw_help(void)
{
    erase();
    mvprintw(0, 2, "Seekey TUI — key bindings");
    mvprintw(2, 2, "Navigation");
    mvprintw(3, 4, "Up/Down or j/k      select field");
    mvprintw(4, 4, "Left/Right or h/l   adjust numeric/choice/color/bool");
    mvprintw(5, 4, "Enter               edit / pick (depends on field type)");
    mvprintw(6, 4, "Esc or q            back / cancel in pickers");
    mvprintw(8, 2, "Actions");
    mvprintw(9, 4, "s                   save to current path");
    mvprintw(10, 4, "S                   save as (new path)");
    mvprintw(11, 4, "r                   reset current field to default");
    mvprintw(12, 4, "R                   reset all fields to defaults");
    mvprintw(13, 4, "L                   reload from disk (discard changes)");
    mvprintw(14, 4, "W                   reset window state (forget saved monitor)");
    mvprintw(15, 4, "?                   this help");
    mvprintw(16, 4, "q / Esc             quit (asks to save if dirty)");
    mvprintw(18, 2, "Press any key to return");
    refresh();
    getch();
}

static void tui_redraw(TuiState *st)
{
    erase();
    mvprintw(0, 2, "Seekey config TUI");
    const char *path = st->saved_path[0] ? st->saved_path : st->config->config_path;
    if (path[0]) {
        mvprintw(1, 2, "Config: %s", path);
    } else {
        char *def = seekey_default_save_path();
        mvprintw(1, 2, "Config: %s (default; S for another path)", def);
        g_free(def);
    }
    if (st->dirty) {
        mvprintw(2, 2, "[Unsaved]");
    } else {
        mvprintw(2, 2, "          ");
    }
    mvprintw(3, 2, "Keys: \x18\x19 select  \x1b\x1a adjust  Enter edit/pick"
                   "  s save  S save as  r reset field  R reset all"
                   "  L reload  W reset window state  ? help  q quit");

    int list_top = 5;
    int list_bottom = LINES - 10;
    int visible = MAX(1, list_bottom - list_top);
    if (st->selected < st->scroll) {
        st->scroll = st->selected;
    } else if (st->selected >= st->scroll + visible) {
        st->scroll = st->selected - visible + 1;
    }

    for (int row = 0; row < visible; row++) {
        int index = st->scroll + row;
        if (index >= (int)st->field_count) break;
        char value[512];
        tui_field_value(&st->fields[index], value, sizeof(value));
        if (index == st->selected) attron(A_REVERSE);
        mvprintw(list_top + row, 2, "%-28s %s", st->fields[index].label, value);
        if (index == st->selected) attroff(A_REVERSE);
        if (st->fields[index].type == TUI_COLOR) {
            tui_draw_color_swatch(list_top + row, 56, value, 4);
        }
    }

    if (st->selected < (int)st->field_count) {
        mvprintw(LINES - 9, 2, "%s", st->fields[st->selected].help);
    }
    if (st->status_ttl > 0 && st->status_line[0]) {
        attron(A_BOLD);
        mvprintw(LINES - 9, COLS - 40, "%s", st->status_line);
        attroff(A_BOLD);
    }
    tui_draw_preview(st->config);
    refresh();
}

static gboolean tui_ask_yn(const char *question)
{
    echo();
    curs_set(1);
    int row = LINES - 3;
    move(row, 0); clrtoeol();
    mvprintw(row, 0, "%s [y/n]: ", question);
    char buf[8] = {0};
    getnstr(buf, (int)sizeof(buf) - 1);
    noecho();
    curs_set(0);
    char c = buf[0];
    return c == 'y' || c == 'Y';
}

static gboolean tui_save_as(TuiState *st, GError **error)
{
    char path[512];
    g_strlcpy(path, st->config->config_path[0] ? st->config->config_path
                                              : "seekey.ini",
              sizeof(path));
    if (!tui_prompt_string("Save as", "Path to the new ini file",
                           path, sizeof(path), path)) {
        return TRUE;  /* user kept current value */
    }
    g_strlcpy(st->config->config_path, path, sizeof(st->config->config_path));
    if (!seekey_config_save(st->config, error)) {
        return FALSE;
    }
    g_strlcpy(st->saved_path, path, sizeof(st->saved_path));
    st->dirty = FALSE;
    tui_set_status(st, "Saved to %s", path);
    return TRUE;
}

static gboolean tui_save(TuiState *st, GError **error)
{
    if (st->config->config_path[0] == '\0') {
        /* Default to <cwd>/seekey.ini without prompting. The user can
         * still pick a different path with capital S. */
        char *path = seekey_default_save_path();
        g_strlcpy(st->config->config_path, path,
                  sizeof(st->config->config_path));
        g_free(path);
    }
    if (!seekey_config_save(st->config, error)) {
        return FALSE;
    }
    g_strlcpy(st->saved_path, st->config->config_path, sizeof(st->saved_path));
    st->dirty = FALSE;
    tui_set_status(st, "Saved to %s", st->config->config_path);
    return TRUE;
}

static void tui_reload(TuiState *st)
{
    if (st->config->config_path[0] == '\0') return;
    GError *err = NULL;
    seekey_config_set_defaults(st->config);
    if (!seekey_config_load(st->config, &err)) {
        tui_set_status(st, "Reload failed: %s", err->message);
        g_clear_error(&err);
        return;
    }
    /* Rebuild fields to update defaults. */
    tui_build_fields(st->fields, &st->field_count, st->config);
    st->dirty = FALSE;
    tui_set_status(st, "Reloaded from %s", st->config->config_path);
}

static void tui_reset_all(TuiState *st)
{
    for (size_t i = 0; i < st->field_count; i++) {
        tui_reset_field(&st->fields[i]);
    }
    /* Theme application also runs. */
    seekey_config_apply_theme(st->config, st->config->theme);
    st->dirty = TRUE;
    tui_set_status(st, "Reset all fields to defaults");
}

gboolean seekey_tui_run(SeekeyConfig *config, GError **error)
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    tui_init_colors();

    TuiField fields[TUI_FIELD_COUNT];
    size_t field_count = 0;
    tui_build_fields(fields, &field_count, config);
    g_assert(field_count == TUI_FIELD_COUNT);

    TuiState st = {
        .config = config,
        .fields = fields,
        .field_count = field_count,
        .selected = 0,
        .scroll = 0,
        .running = TRUE,
        .dirty = FALSE,
        .saved_path = {0},
        .show_help = FALSE,
        .status_line = {0},
        .status_ttl = 0,
    };
    g_strlcpy(st.saved_path, config->config_path, sizeof(st.saved_path));

    while (st.running) {
        tui_redraw(&st);
        if (st.status_ttl > 0) st.status_ttl--;
        int ch = getch();
        switch (ch) {
        case KEY_UP: case 'k':
            st.selected = st.selected > 0
                          ? st.selected - 1
                          : (int)st.field_count - 1;
            break;
        case KEY_DOWN: case 'j':
            st.selected = st.selected + 1 < (int)st.field_count
                          ? st.selected + 1 : 0;
            break;
        case KEY_LEFT: case 'h': {
            TuiField *f = &fields[st.selected];
            gboolean was_theme = g_strcmp0(f->label, "theme") == 0;
            tui_adjust_field(f, -1);
            if (was_theme || (f->type == TUI_CHOICE &&
                              g_strcmp0(f->label, "theme") == 0)) {
                seekey_config_apply_theme(config, config->theme);
            }
            if (f->type == TUI_BOOL || f->type == TUI_CHOICE ||
                f->type == TUI_COLOR || f->type == TUI_UINT) {
                st.dirty = TRUE;
            }
            break;
        }
        case KEY_RIGHT: case 'l': {
            TuiField *f = &fields[st.selected];
            tui_adjust_field(f, 1);
            if (g_strcmp0(f->label, "theme") == 0) {
                seekey_config_apply_theme(config, config->theme);
            }
            if (f->type == TUI_BOOL || f->type == TUI_CHOICE ||
                f->type == TUI_COLOR || f->type == TUI_UINT) {
                st.dirty = TRUE;
            }
            break;
        }
        case '\n': case '\r': case KEY_ENTER: {
            TuiField *f = &fields[st.selected];
            switch (f->type) {
            case TUI_BOOL:
                /* Direct toggle; never falls through to text input.
                 * Fixes the "dest != NULL" GLib assertion. */
                *f->bool_target = !*f->bool_target;
                st.dirty = TRUE;
                break;
            case TUI_CHOICE:
                if (g_strcmp0(f->label, "theme") == 0) {
                    tui_theme_picker(&st, config);
                } else {
                    tui_choice_picker(&st, f);
                }
                break;
            case TUI_COLOR:
                tui_color_picker(&st, f);
                break;
            case TUI_STRING: {
                char new_val[256];
                g_strlcpy(new_val, f->string_target, sizeof(new_val));
                if (tui_prompt_string(f->label, f->input_hint,
                                      new_val, sizeof(new_val),
                                      f->string_target)) {
                    g_strlcpy(f->string_target, new_val, f->string_size);
                    g_strlcpy(f->default_string, new_val,
                              sizeof(f->default_string));
                    st.dirty = TRUE;
                }
                break;
            }
            case TUI_UINT: {
                guint new_val = *f->uint_target;
                if (tui_prompt_uint(f->label, f->min, f->max,
                                    *f->uint_target, &new_val)) {
                    *f->uint_target = new_val;
                    f->default_uint = new_val;
                    st.dirty = TRUE;
                }
                break;
            }
            }
            break;
        }
        case 'r': {
            tui_reset_field(&fields[st.selected]);
            if (g_strcmp0(fields[st.selected].label, "theme") == 0) {
                seekey_config_apply_theme(config, config->theme);
            }
            st.dirty = TRUE;
            tui_set_status(&st, "Reset '%s' to default",
                           fields[st.selected].label);
            break;
        }
        case 'R':
            if (tui_ask_yn("Reset ALL fields to defaults?")) {
                tui_reset_all(&st);
            }
            break;
        case 'L':
            if (!st.dirty || tui_ask_yn("Discard current changes and reload?")) {
                tui_reload(&st);
            }
            break;
        case 'W': case 'w':
            seekey_window_state_clear();
            tui_set_status(&st, "Window state cleared; next launch uses focused monitor");
            break;
        case 'S':
            if (!tui_save_as(&st, error)) goto done;
            break;
        case 's':
            if (!tui_save(&st, error)) goto done;
            break;
        case '?':
            tui_draw_help();
            break;
        case 'q': case 'Q': case 27: {
            if (st.dirty) {
                if (!tui_ask_yn("Save changes before quitting?")) {
                    /* User chose n (no): discard */
                    break;
                }
                /* User chose y: save first */
                if (!tui_save(&st, error)) goto done;
            }
            st.running = FALSE;
            break;
        }
        }
    }
done:
    endwin();
    return *error == NULL;
}

#else  /* SEEKEY_TEST stub */

gboolean seekey_tui_run(SeekeyConfig *config, GError **error)
{
    (void)config;
    (void)error;
    return TRUE;
}

#endif
