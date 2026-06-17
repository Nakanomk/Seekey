#include "seekey.h"

#include <linux/input-event-codes.h>

typedef struct {
    guint code;
    const char *name;
} KeyName;

static const KeyName KEY_NAMES[] = {
    {KEY_ESC, "Esc"},
    {KEY_1, "1"}, {KEY_2, "2"}, {KEY_3, "3"}, {KEY_4, "4"}, {KEY_5, "5"},
    {KEY_6, "6"}, {KEY_7, "7"}, {KEY_8, "8"}, {KEY_9, "9"}, {KEY_0, "0"},
    {KEY_MINUS, "-"}, {KEY_EQUAL, "="}, {KEY_BACKSPACE, "Backspace"},
    {KEY_TAB, "Tab"},
    {KEY_Q, "Q"}, {KEY_W, "W"}, {KEY_E, "E"}, {KEY_R, "R"}, {KEY_T, "T"},
    {KEY_Y, "Y"}, {KEY_U, "U"}, {KEY_I, "I"}, {KEY_O, "O"}, {KEY_P, "P"},
    {KEY_LEFTBRACE, "["}, {KEY_RIGHTBRACE, "]"}, {KEY_ENTER, "Enter"},
    {KEY_LEFTCTRL, "Ctrl"}, {KEY_RIGHTCTRL, "Ctrl"},
    {KEY_A, "A"}, {KEY_S, "S"}, {KEY_D, "D"}, {KEY_F, "F"}, {KEY_G, "G"},
    {KEY_H, "H"}, {KEY_J, "J"}, {KEY_K, "K"}, {KEY_L, "L"},
    {KEY_SEMICOLON, ";"}, {KEY_APOSTROPHE, "'"}, {KEY_GRAVE, "`"},
    {KEY_LEFTSHIFT, "Shift"}, {KEY_RIGHTSHIFT, "Shift"},
    {KEY_BACKSLASH, "\\"},
    {KEY_Z, "Z"}, {KEY_X, "X"}, {KEY_C, "C"}, {KEY_V, "V"}, {KEY_B, "B"},
    {KEY_N, "N"}, {KEY_M, "M"}, {KEY_COMMA, ","}, {KEY_DOT, "."},
    {KEY_SLASH, "/"}, {KEY_KPASTERISK, "Num *"},
    {KEY_LEFTALT, "Alt"}, {KEY_RIGHTALT, "AltGr"},
    {KEY_SPACE, "Space"}, {KEY_CAPSLOCK, "Caps Lock"},
    {KEY_F1, "F1"}, {KEY_F2, "F2"}, {KEY_F3, "F3"}, {KEY_F4, "F4"},
    {KEY_F5, "F5"}, {KEY_F6, "F6"}, {KEY_F7, "F7"}, {KEY_F8, "F8"},
    {KEY_F9, "F9"}, {KEY_F10, "F10"}, {KEY_NUMLOCK, "Num Lock"},
    {KEY_SCROLLLOCK, "Scroll Lock"}, {KEY_KP7, "Num 7"}, {KEY_KP8, "Num 8"},
    {KEY_KP9, "Num 9"}, {KEY_KPMINUS, "Num -"}, {KEY_KP4, "Num 4"},
    {KEY_KP5, "Num 5"}, {KEY_KP6, "Num 6"}, {KEY_KPPLUS, "Num +"},
    {KEY_KP1, "Num 1"}, {KEY_KP2, "Num 2"}, {KEY_KP3, "Num 3"},
    {KEY_KP0, "Num 0"}, {KEY_KPDOT, "Num ."},
    {KEY_F11, "F11"}, {KEY_F12, "F12"}, {KEY_KPENTER, "Num Enter"},
    {KEY_KPSLASH, "Num /"}, {KEY_SYSRQ, "Print"}, {KEY_HOME, "Home"},
    {KEY_UP, "Up"}, {KEY_PAGEUP, "Page Up"}, {KEY_LEFT, "Left"},
    {KEY_RIGHT, "Right"}, {KEY_END, "End"}, {KEY_DOWN, "Down"},
    {KEY_PAGEDOWN, "Page Down"}, {KEY_INSERT, "Insert"}, {KEY_DELETE, "Delete"},
    {KEY_MUTE, "Mute"}, {KEY_VOLUMEDOWN, "Volume Down"},
    {KEY_VOLUMEUP, "Volume Up"}, {KEY_POWER, "Power"},
    {KEY_PAUSE, "Pause"}, {KEY_LEFTMETA, "Super"}, {KEY_RIGHTMETA, "Super"},
    {KEY_COMPOSE, "Menu"}, {KEY_STOP, "Stop"}, {KEY_AGAIN, "Again"},
    {KEY_PROPS, "Props"}, {KEY_UNDO, "Undo"}, {KEY_FRONT, "Front"},
    {KEY_COPY, "Copy"}, {KEY_OPEN, "Open"}, {KEY_PASTE, "Paste"},
    {KEY_FIND, "Find"}, {KEY_CUT, "Cut"}, {KEY_HELP, "Help"},
    {KEY_CALC, "Calculator"}, {KEY_SLEEP, "Sleep"}, {KEY_WWW, "Web"},
    {KEY_BACK, "Back"}, {KEY_FORWARD, "Forward"}, {KEY_NEXTSONG, "Next"},
    {KEY_PLAYPAUSE, "Play/Pause"}, {KEY_PREVIOUSSONG, "Previous"},
    {KEY_STOPCD, "Stop"}, {KEY_REFRESH, "Refresh"}, {KEY_EDIT, "Edit"},
    {KEY_SCROLLUP, "Scroll Up"}, {KEY_SCROLLDOWN, "Scroll Down"},
};

const char *seekey_key_name(guint code)
{
    for (gsize i = 0; i < G_N_ELEMENTS(KEY_NAMES); i++) {
        if (KEY_NAMES[i].code == code) {
            return KEY_NAMES[i].name;
        }
    }

    static GPrivate unknown_key = G_PRIVATE_INIT(g_free);
    char *buffer = g_private_get(&unknown_key);
    if (buffer == NULL) {
        buffer = g_malloc0(32);
        g_private_set(&unknown_key, buffer);
    }
    g_snprintf(buffer, 32, "Key %u", code);
    return buffer;
}

const char *seekey_key_text(guint code, gboolean shifted)
{
    switch (code) {
    case KEY_A: return shifted ? "A" : "a";
    case KEY_B: return shifted ? "B" : "b";
    case KEY_C: return shifted ? "C" : "c";
    case KEY_D: return shifted ? "D" : "d";
    case KEY_E: return shifted ? "E" : "e";
    case KEY_F: return shifted ? "F" : "f";
    case KEY_G: return shifted ? "G" : "g";
    case KEY_H: return shifted ? "H" : "h";
    case KEY_I: return shifted ? "I" : "i";
    case KEY_J: return shifted ? "J" : "j";
    case KEY_K: return shifted ? "K" : "k";
    case KEY_L: return shifted ? "L" : "l";
    case KEY_M: return shifted ? "M" : "m";
    case KEY_N: return shifted ? "N" : "n";
    case KEY_O: return shifted ? "O" : "o";
    case KEY_P: return shifted ? "P" : "p";
    case KEY_Q: return shifted ? "Q" : "q";
    case KEY_R: return shifted ? "R" : "r";
    case KEY_S: return shifted ? "S" : "s";
    case KEY_T: return shifted ? "T" : "t";
    case KEY_U: return shifted ? "U" : "u";
    case KEY_V: return shifted ? "V" : "v";
    case KEY_W: return shifted ? "W" : "w";
    case KEY_X: return shifted ? "X" : "x";
    case KEY_Y: return shifted ? "Y" : "y";
    case KEY_Z: return shifted ? "Z" : "z";
    case KEY_1: return shifted ? "!" : "1";
    case KEY_2: return shifted ? "@" : "2";
    case KEY_3: return shifted ? "#" : "3";
    case KEY_4: return shifted ? "$" : "4";
    case KEY_5: return shifted ? "%" : "5";
    case KEY_6: return shifted ? "^" : "6";
    case KEY_7: return shifted ? "&" : "7";
    case KEY_8: return shifted ? "*" : "8";
    case KEY_9: return shifted ? "(" : "9";
    case KEY_0: return shifted ? ")" : "0";
    case KEY_MINUS: return shifted ? "_" : "-";
    case KEY_EQUAL: return shifted ? "+" : "=";
    case KEY_LEFTBRACE: return shifted ? "{" : "[";
    case KEY_RIGHTBRACE: return shifted ? "}" : "]";
    case KEY_BACKSLASH: return shifted ? "|" : "\\";
    case KEY_SEMICOLON: return shifted ? ":" : ";";
    case KEY_APOSTROPHE: return shifted ? "\"" : "'";
    case KEY_GRAVE: return shifted ? "~" : "`";
    case KEY_COMMA: return shifted ? "<" : ",";
    case KEY_DOT: return shifted ? ">" : ".";
    case KEY_SLASH: return shifted ? "?" : "/";
    case KEY_SPACE: return " ";
    default: return NULL;
    }
}

gboolean seekey_is_modifier(guint code)
{
    switch (code) {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
        return TRUE;
    default:
        return FALSE;
    }
}

int seekey_modifier_order(guint code)
{
    switch (code) {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        return 10;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
        return 20;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        return 30;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        return 40;
    default:
        return 100;
    }
}
