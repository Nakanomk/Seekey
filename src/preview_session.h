#ifndef SEEKEY_PREVIEW_SESSION_H
#define SEEKEY_PREVIEW_SESSION_H

#include "seekey.h"

typedef struct SeekeyPreviewSession SeekeyPreviewSession;

/* Query D-Bus without claiming the dev.seekey application ID. Returns FALSE
 * only when the session-bus query itself fails. */
gboolean seekey_overlay_query_running(gboolean *running, GError **error);

/* Start the single user-scoped style preview fed by a temporary config. */
SeekeyPreviewSession *seekey_preview_session_start(const SeekeyConfig *config,
                                                   GError **error);

/* Restart the overlay when the effective in-memory configuration changed. */
gboolean seekey_preview_session_sync(SeekeyPreviewSession *session,
                                     const SeekeyConfig *config,
                                     GError **error);

/* Stop the overlay child and remove its temporary configuration. */
void seekey_preview_session_free(SeekeyPreviewSession *session);

#endif
