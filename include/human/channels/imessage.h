#ifndef HU_CHANNELS_IMESSAGE_H
#define HU_CHANNELS_IMESSAGE_H

#include "human/channel.h"
#include "human/channel_loop.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

hu_error_t hu_imessage_create(hu_allocator_t *alloc, const char *default_target,
                              size_t default_target_len, const char *const *allow_from,
                              size_t allow_from_count, hu_channel_t *out);
void hu_imessage_destroy(hu_channel_t *ch);

/** Returns true if default target (phone/email) is configured. */
bool hu_imessage_is_configured(hu_channel_t *ch);

/** Poll ~/Library/Messages/chat.db for new inbound messages (macOS only). */
hu_error_t hu_imessage_poll(void *channel_ctx, hu_allocator_t *alloc, hu_channel_loop_msg_t *msgs,
                            size_t max_msgs, size_t *out_count);

/** Maps hu_reaction_type_t to iMessage tapback name (love, like, dislike, etc.).
 * Returns NULL for HU_REACTION_NONE or unknown. */
const char *hu_imessage_reaction_to_tapback_name(hu_reaction_type_t reaction);

/** Build tapback context string for recent reactions on our messages from this contact.
 * Returns allocated string like "[REACTIONS on your recent messages: 2 hearts, 1 like]" or NULL.
 * Caller owns. Stub returns NULL on non-macOS or when SQLite unavailable. */
hu_error_t hu_imessage_build_tapback_context(hu_allocator_t *alloc, const char *contact_id,
                                             size_t contact_id_len, char **out, size_t *out_len);

hu_error_t hu_imessage_build_read_receipt_context(hu_allocator_t *alloc, const char *contact_id,
                                                  size_t contact_id_len, char **out,
                                                  size_t *out_len);

/** Count positive tapbacks (love/like/laugh/emphasis) on our GIF messages from this
 * contact in the last 24 hours. Uses direct SQL on chat.db associated_message_type.
 * Returns 0 on non-macOS or when SQLite unavailable. */
int hu_imessage_count_recent_gif_tapbacks(const char *contact_id, size_t contact_id_len);

#ifndef HU_IS_TEST
/** Check if the real user sent a message to `handle` within the last
 * `within_seconds` seconds.  Queries chat.db for is_from_me=1 rows.
 * Returns true if the user responded recently (Human should stay silent). */
bool hu_imessage_user_responded_recently(void *channel_ctx, const char *handle, size_t handle_len,
                                         int within_seconds);

/** Query the attachment path for a given message ROWID from chat.db.
 * Returns the attachment file path or NULL if not found. Caller owns. */
char *hu_imessage_get_attachment_path(hu_allocator_t *alloc, int64_t message_id);

/** Get the attachment path for the most recent message with an attachment from
 * the given contact. Returns path or NULL. Caller owns. */
char *hu_imessage_get_latest_attachment_path(hu_allocator_t *alloc, const char *contact_id,
                                             size_t contact_id_len);
#endif

/** Look up the text of a message by its GUID from chat.db.
 * Used for inline reply context — when a message has thread_originator_guid,
 * look up what they're replying to. Writes into out_text buffer. */
hu_error_t hu_imessage_lookup_message_by_guid(hu_allocator_t *alloc, const char *guid,
                                              size_t guid_len, char *out_text, size_t out_cap,
                                              size_t *out_len);

/** Extract plain text from an NSAttributedString (NSKeyedArchiver) blob.
 * macOS 15+ stores iMessage text in attributedBody instead of the text column.
 * Pure byte parsing — no platform dependencies. Returns extracted length, or 0. */
size_t hu_imessage_extract_attributed_body(const unsigned char *blob, size_t blob_len, char *out,
                                           size_t out_cap);

/** Enable or disable the imsg CLI for send/react (runtime toggle).
 * When enabled, imsg CLI is tried first with AppleScript fallback.
 * Default: false. Set via config.json channels.imessage.use_imsg_cli. */
void hu_imessage_set_use_imsg_cli(hu_channel_t *ch, bool use_imsg);

/** Extract a JSON string value by key from a raw JSON fragment.
 * Used by the GIF search to parse Tenor API responses. Exposed for testing. */
size_t hu_imessage_gif_json_extract(const char *json, size_t json_len, const char *key, char *out,
                                    size_t cap);

/** Search Tenor for a GIF matching the query and download to a temp file.
 * Returns the local path to the downloaded GIF (caller owns, free with alloc).
 * Returns NULL on failure (no API key, network error, no results).
 * Requires HU_ENABLE_CURL. api_key is the Tenor API v2 key. */
char *hu_imessage_fetch_gif(hu_allocator_t *alloc, const char *query, size_t query_len,
                            const char *api_key, size_t api_key_len);

#if HU_IS_TEST
hu_error_t hu_imessage_test_inject_mock(hu_channel_t *ch, const char *session_key,
                                        size_t session_key_len, const char *content,
                                        size_t content_len);
hu_error_t hu_imessage_test_inject_mock_ex(hu_channel_t *ch, const char *session_key,
                                           size_t session_key_len, const char *content,
                                           size_t content_len, bool has_attachment);
hu_error_t hu_imessage_test_inject_mock_ex2(hu_channel_t *ch, const char *session_key,
                                            size_t session_key_len, const char *content,
                                            size_t content_len, bool has_attachment,
                                            bool has_video);
const char *hu_imessage_test_get_last_message(hu_channel_t *ch, size_t *out_len);
void hu_imessage_test_get_last_reaction(hu_channel_t *ch, hu_reaction_type_t *out_reaction,
                                        int64_t *out_message_id);
#endif

#endif /* HU_CHANNELS_IMESSAGE_H */
