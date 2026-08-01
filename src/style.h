#ifndef SEEKEY_STYLE_H
#define SEEKEY_STYLE_H

#include "seekey.h"

/* Build the GTK CSS shared by the overlay and the GUI preview. */
char *seekey_style_build_css(const SeekeyConfig *config);

/* Install the current overlay CSS for the default display. */
void seekey_style_install(const SeekeyConfig *config);

#endif
