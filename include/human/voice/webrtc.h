#ifndef HU_WEBRTC_H
#define HU_WEBRTC_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct hu_webrtc_config { char *stun_server; char *turn_server; char *turn_username; char *turn_password; uint16_t local_port; bool audio_enabled; bool video_enabled; } hu_webrtc_config_t;
typedef struct hu_webrtc_session { hu_allocator_t *alloc; hu_webrtc_config_t config; bool connected; char *local_sdp; char *remote_sdp; } hu_webrtc_session_t;
hu_error_t hu_webrtc_session_create(hu_allocator_t *alloc, const hu_webrtc_config_t *config, hu_webrtc_session_t **out);
hu_error_t hu_webrtc_connect(hu_webrtc_session_t *session, const char *remote_sdp);
hu_error_t hu_webrtc_send_audio(hu_webrtc_session_t *session, const void *data, size_t data_len);
void hu_webrtc_session_destroy(hu_webrtc_session_t *session);
#endif
