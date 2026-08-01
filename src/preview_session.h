#ifndef SEEKEY_PREVIEW_SESSION_H
#define SEEKEY_PREVIEW_SESSION_H

#include "seekey.h"

typedef struct SeekeyPreviewSession SeekeyPreviewSession;

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
