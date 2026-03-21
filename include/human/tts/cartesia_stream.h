#ifndef HU_CARTESIA_STREAM_H
#define HU_CARTESIA_STREAM_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct hu_cartesia_stream hu_cartesia_stream_t;

/**
 * Open a Cartesia TTS WebSocket (wss://api.cartesia.ai/tts/websocket).
 * Requires HU_HAS_TLS and HU_GATEWAY_POSIX when not in HU_IS_TEST.
 */
hu_error_t hu_cartesia_stream_open(hu_allocator_t *alloc, const char *api_key,
                                   const char *voice_id, const char *model_id,
                                   hu_cartesia_stream_t **out);

void hu_cartesia_stream_close(hu_cartesia_stream_t *s, hu_allocator_t *alloc);

/**
 * Send a generation request. transcript may be empty when is_continue is false to flush.
 */
hu_error_t hu_cartesia_stream_send_generation(hu_cartesia_stream_t *s, hu_allocator_t *alloc,
                                              const char *context_id, const char *transcript,
                                              bool is_continue);

hu_error_t hu_cartesia_stream_flush_context(hu_cartesia_stream_t *s, hu_allocator_t *alloc,
                                            const char *context_id);

hu_error_t hu_cartesia_stream_cancel_context(hu_cartesia_stream_t *s, hu_allocator_t *alloc,
                                             const char *context_id);

/**
 * Receive the next server message. On type "chunk", decodes base64 PCM f32le into *pcm_out.
 * *recv_done is true when type is "done" or "error" (caller should stop draining).
 */
hu_error_t hu_cartesia_stream_recv_next(hu_cartesia_stream_t *s, hu_allocator_t *alloc,
                                        void **pcm_out, size_t *pcm_len, bool *recv_done);

#endif /* HU_CARTESIA_STREAM_H */
