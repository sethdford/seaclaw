#ifndef HU_WEBRTC_H
#define HU_WEBRTC_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

typedef struct hu_webrtc_config {
    char *stun_server;
    char *turn_server;
    char *turn_username;
    char *turn_password;
    uint16_t local_port;
    bool audio_enabled;
    bool video_enabled;
    char *signaling_endpoint;
    char *api_key;
    char *audio_endpoint;
} hu_webrtc_config_t;

typedef struct hu_webrtc_session {
    hu_allocator_t *alloc;
    hu_webrtc_config_t config;
    bool connected;
    char *local_sdp;
    char *remote_sdp;
    int udp_fd;
    uint8_t lifecycle;
    uint32_t rtp_ssrc;
    uint16_t rtp_seq_next;
    uint32_t rtp_ts;
    bool srtp_ready;
    struct sockaddr_storage remote_media;
    socklen_t remote_media_len;
    void *ice_state;
    void *dtls_state;
    void *srtp_state;
} hu_webrtc_session_t;

hu_error_t hu_webrtc_session_create(hu_allocator_t *alloc, const hu_webrtc_config_t *config,
                                    hu_webrtc_session_t **out);
hu_error_t hu_webrtc_connect(hu_webrtc_session_t *session, const char *remote_sdp);
hu_error_t hu_webrtc_send_audio(hu_webrtc_session_t *session, const void *data, size_t data_len);
hu_error_t hu_webrtc_recv_audio(hu_webrtc_session_t *session, void *out_buf, size_t out_cap, size_t *out_len);
void hu_webrtc_session_destroy(hu_webrtc_session_t *session);

#endif
