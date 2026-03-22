#ifndef HU_CHANNELS_TEAMS_H
#define HU_CHANNELS_TEAMS_H

#include "human/channel.h"
#include "human/channel_loop.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/* graph_access_token / team_id: optional Microsoft Graph credentials for channel message history;
 * both may be NULL / zero length. */
hu_error_t hu_teams_create(hu_allocator_t *alloc, const char *webhook_url, size_t webhook_url_len,
                           const char *graph_access_token, size_t graph_access_token_len,
                           const char *team_id, size_t team_id_len, hu_channel_t *out);

hu_error_t hu_teams_on_webhook(void *channel_ctx, hu_allocator_t *alloc, const char *body,
                               size_t body_len);

hu_error_t hu_teams_poll(void *channel_ctx, hu_allocator_t *alloc, hu_channel_loop_msg_t *msgs,
                         size_t max_msgs, size_t *out_count);

bool hu_teams_is_configured(hu_channel_t *ch);

void hu_teams_destroy(hu_channel_t *ch);

#if HU_IS_TEST
hu_error_t hu_teams_test_inject_mock(hu_channel_t *ch, const char *session_key,
                                     size_t session_key_len, const char *content,
                                     size_t content_len);
const char *hu_teams_test_get_last_message(hu_channel_t *ch, size_t *out_len);
#endif

#endif /* HU_CHANNELS_TEAMS_H */
