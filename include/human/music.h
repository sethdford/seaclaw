#ifndef HU_MUSIC_H
#define HU_MUSIC_H

#include "core/allocator.h"
#include "core/error.h"
#include <stdbool.h>
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────────────────
 * iTunes Search API client — song lookup + 30-second preview download
 *
 * Searches the public iTunes Search API (no auth required), parses the JSON
 * response, and optionally downloads the DRM-free 30-second .m4a preview to
 * a temp file suitable for sending as an iMessage audio attachment.
 * ────────────────────────────────────────────────────────────────────────── */

typedef struct hu_music_result {
    char *track_name;     /* "Bohemian Rhapsody" */
    char *artist_name;    /* "Queen" */
    char *album_name;     /* "A Night at the Opera" */
    char *preview_url;    /* https://audio-ssl.itunes.apple.com/... (.m4a) */
    char *track_view_url; /* https://music.apple.com/us/album/... */
    char *artwork_url;    /* https://is1-ssl.mzstatic.com/... (100x100) */
    char *genre;          /* "Rock" */
} hu_music_result_t;

/** Search iTunes for a song. Caller must free result with hu_music_result_free.
 *  Requires HU_HTTP_CURL at runtime; returns HU_ERR_NOT_SUPPORTED in test/no-curl builds.
 *  The query should be "artist title" or similar search terms. */
hu_error_t hu_music_search(hu_allocator_t *alloc, const char *query, size_t query_len,
                           hu_music_result_t *out);

/** Download the 30-second preview .m4a to a temp file.
 *  Writes the absolute path into path_out (up to path_cap bytes).
 *  Caller must unlink() the file when done.
 *  Requires HU_HTTP_CURL; returns HU_ERR_NOT_SUPPORTED otherwise. */
hu_error_t hu_music_download_preview(hu_allocator_t *alloc, const char *preview_url,
                                     char *path_out, size_t path_cap);

/** Free all allocated fields inside a result (does not free the struct itself). */
void hu_music_result_free(hu_allocator_t *alloc, hu_music_result_t *result);

/** Build a casual share message: "casual text https://music.apple.com/...".
 *  If casual_msg is NULL, generates "Artist - Title <url>".
 *  Returns bytes written (excluding NUL), or 0 on error. */
size_t hu_music_build_share_text(const hu_music_result_t *result, const char *casual_msg,
                                 size_t casual_msg_len, char *buf, size_t cap);

/* ── Exposed for testing (pure computation, no network) ──────────────── */

/** URL-encode a search query for the iTunes API (spaces → +, strips non-alnum). */
size_t hu_music_url_encode_query(const char *query, size_t query_len, char *out, size_t out_cap);

/** Parse an iTunes Search API JSON response into a result struct.
 *  Returns HU_ERR_NOT_FOUND if resultCount is 0 or results array is empty. */
hu_error_t hu_music_parse_search_response(hu_allocator_t *alloc, const char *json, size_t json_len,
                                          hu_music_result_t *out);

/** Parse an LLM music suggestion in "ARTIST - TITLE | casual message" format.
 *  Writes artist+title into query_out and casual message into msg_out.
 *  Returns true on success. Buffers must be at least query_cap / msg_cap bytes. */
bool hu_music_parse_suggestion(const char *suggestion, size_t suggestion_len, char *query_out,
                               size_t query_cap, char *msg_out, size_t msg_cap);

#endif
