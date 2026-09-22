#ifndef SEEKEY_GUI_H
#define SEEKEY_GUI_H

#include "seekey.h"
#include "tui.h"

/* Run the graphical configuration menu. Tries a real fuzzel session first
 * (see fuzzel_menu.c) and falls back to the built-in GTK menu when fuzzel
 * is missing, too old, or misconfigured. When `first_desktop_launch` is
 * true, ask how future launches from the desktop entry should behave
 * first. */
gboolean seekey_config_gui_run(SeekeyConfig *config,
                               gboolean first_desktop_launch,
                               GError **error);

/* Run the fuzzel-driven menu. Returns TRUE when the menu completed
 * normally. Returns FALSE (usually without touching `error`) when fuzzel
 * is unavailable so the caller can fall back; a runtime failure inside the
 * menu sets `error` and also returns FALSE. */
gboolean seekey_fuzzel_menu_run(SeekeyConfig *config,
                                gboolean first_desktop_launch,
                                GError **error);

/* --- Shared menu helpers (implemented in gui.c) --- */

/* Whether a valid matugen colors.json can be loaded. */
gboolean seekey_menu_matugen_available(const SeekeyConfig *config);

/* Spawn a detached overlay process using the current config paths. */
gboolean seekey_menu_launch_overlay(const SeekeyConfig *config);

/* Ask the running overlay (dev.seekey) to quit and wait for it. */
gboolean seekey_menu_stop_overlay(GError **error);

/* Whether the key overlay is running. Conservative: returns TRUE when the
 * state cannot be determined. */
gboolean seekey_menu_overlay_running(void);

/* Persist the desktop-entry launch preference. */
void seekey_menu_save_desktop_preference(gboolean show_menu);

/* Save `config`, defaulting the path to <cwd>/seekey.ini. Prints errors. */
gboolean seekey_menu_save(SeekeyConfig *config);


#endif
