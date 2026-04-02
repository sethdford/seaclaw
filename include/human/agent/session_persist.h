#ifndef HU_SESSION_PERSIST_H
#define HU_SESSION_PERSIST_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Session Persistence — save/load/list/delete conversation sessions
 *
 * Sessions are stored as JSON files in a session directory (default:
 * ~/.human/sessions/).  Each file is named <session_id>.json and contains
 * schema_version, metadata, and the full message history.
 *
 * Writes are atomic: write to a temp file, then rename.
 * ────────────────────────────────────────────────────────────────────────── */

#define HU_SESSION_SCHEMA_VERSION 1
#define HU_SESSION_ID_MAX 64
#define HU_SESSION_MAX_FILE_SIZE ((size_t)1024 * 1024 * 1024) /* 1 GB */

typedef struct hu_session_metadata {
    char id[HU_SESSION_ID_MAX];
    time_t created_at;
    char *model_name;      /* owned */
    size_t model_name_len;
    char *workspace_dir;   /* owned */
    size_t workspace_dir_len;
    size_t message_count;
} hu_session_metadata_t;

/* Forward declaration — full definition in agent.h */
typedef struct hu_agent hu_agent_t;

/* Generate a session ID like "session_YYYYMMDD_HHMMSS". */
void hu_session_generate_id(char *buf, size_t buf_size);

/* Save agent's conversation history to <session_dir>/<id>.json.
 * If session_id_out is non-NULL, the generated ID is written there
 * (must be at least HU_SESSION_ID_MAX bytes). */
hu_error_t hu_session_persist_save(hu_allocator_t *alloc, const hu_agent_t *agent,
                           const char *session_dir, char *session_id_out);

/* Load session from <session_dir>/<session_id>.json into agent history.
 * Clears existing history before loading. */
hu_error_t hu_session_persist_load(hu_allocator_t *alloc, hu_agent_t *agent,
                           const char *session_dir, const char *session_id);

/* List all sessions in session_dir.  Caller must free each metadata entry's
 * owned strings and then free the array itself. */
hu_error_t hu_session_persist_list(hu_allocator_t *alloc, const char *session_dir,
                           hu_session_metadata_t **out, size_t *out_count);

/* Delete session file <session_dir>/<session_id>.json. */
hu_error_t hu_session_persist_delete(const char *session_dir, const char *session_id);

/* Free a metadata array returned by hu_session_persist_list. */
void hu_session_metadata_free(hu_allocator_t *alloc, hu_session_metadata_t *arr, size_t count);

/* Return the default session directory (~/.human/sessions/). Caller frees. */
char *hu_session_default_dir(hu_allocator_t *alloc);

#endif /* HU_SESSION_PERSIST_H */
