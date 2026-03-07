#ifndef SC_CHANNELS_MAIXCAM_H
#define SC_CHANNELS_MAIXCAM_H

#include "seaclaw/channel.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stddef.h>
#include <stdint.h>

sc_error_t sc_maixcam_create(sc_allocator_t *alloc, const char *host, size_t host_len,
                             uint16_t port, sc_channel_t *out);
void sc_maixcam_destroy(sc_channel_t *ch);

#if SC_IS_TEST
sc_error_t sc_maixcam_test_inject_mock(sc_channel_t *ch, const char *session_key,
                                       size_t session_key_len, const char *content,
                                       size_t content_len);
const char *sc_maixcam_test_get_last_message(sc_channel_t *ch, size_t *out_len);
#endif

#endif /* SC_CHANNELS_MAIXCAM_H */
