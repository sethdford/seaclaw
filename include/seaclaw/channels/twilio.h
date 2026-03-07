#ifndef SC_CHANNELS_TWILIO_H
#define SC_CHANNELS_TWILIO_H

#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stdbool.h>
#include <stddef.h>

sc_error_t sc_twilio_create(sc_allocator_t *alloc, const char *account_sid, size_t account_sid_len,
                            const char *auth_token, size_t auth_token_len, const char *from_number,
                            size_t from_number_len, const char *to_number, size_t to_number_len,
                            sc_channel_t *out);

sc_error_t sc_twilio_on_webhook(void *channel_ctx, sc_allocator_t *alloc, const char *body,
                                size_t body_len);

sc_error_t sc_twilio_poll(void *channel_ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs,
                          size_t max_msgs, size_t *out_count);

bool sc_twilio_is_configured(sc_channel_t *ch);

void sc_twilio_destroy(sc_channel_t *ch);

#if SC_IS_TEST
sc_error_t sc_twilio_test_inject_mock(sc_channel_t *ch, const char *session_key,
                                      size_t session_key_len, const char *content,
                                      size_t content_len);
const char *sc_twilio_test_get_last_message(sc_channel_t *ch, size_t *out_len);
#endif

#endif /* SC_CHANNELS_TWILIO_H */
