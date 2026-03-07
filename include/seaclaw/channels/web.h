#ifndef SC_CHANNELS_WEB_H
#define SC_CHANNELS_WEB_H

#include "seaclaw/channel.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stddef.h>

sc_error_t sc_web_create(sc_allocator_t *alloc, sc_channel_t *out);

sc_error_t sc_web_create_with_token(sc_allocator_t *alloc, const char *auth_token,
                                    size_t auth_token_len, sc_channel_t *out);

bool sc_web_validate_token(const sc_channel_t *ch, const char *candidate, size_t candidate_len);

void sc_web_destroy(sc_channel_t *ch);

#if SC_IS_TEST
sc_error_t sc_web_test_inject_mock(sc_channel_t *ch, const char *session_key,
                                   size_t session_key_len, const char *content, size_t content_len);
const char *sc_web_test_get_last_message(sc_channel_t *ch, size_t *out_len);
#endif

#endif /* SC_CHANNELS_WEB_H */
