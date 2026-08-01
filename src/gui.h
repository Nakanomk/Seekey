#ifndef SEEKEY_GUI_H
#define SEEKEY_GUI_H

#include "seekey.h"

/* Run the graphical configuration menu. When `first_desktop_launch` is true,
 * ask how future launches from the desktop entry should behave first. */
gboolean seekey_config_gui_run(SeekeyConfig *config,
                               gboolean first_desktop_launch,
                               GError **error);

#endif
