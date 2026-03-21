/* Internal WebRTC helpers — not part of the public ABI. */
#ifndef HU_WEBRTC_INTERNAL_H
#define HU_WEBRTC_INTERNAL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/voice/webrtc.h"

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

typedef enum hu_webrtc_lifecycle {
    HU_WEBRTC_LIFECYCLE_IDLE = 0,
    HU_WEBRTC_LIFECYCLE_ICE,
    HU_WEBRTC_LIFECYCLE_DTLS,
    HU_WEBRTC_LIFECYCLE_MEDIA,
    HU_WEBRTC_LIFECYCLE_CLOSED
} hu_webrtc_lifecycle_t;

typedef struct hu_webrtc_ice_state hu_webrtc_ice_state_t;
typedef struct hu_webrtc_dtls_state hu_webrtc_dtls_state_t;
typedef struct hu_webrtc_srtp_state hu_webrtc_srtp_state_t;

typedef struct hu_webrtc_dtls_srtp_material {
    uint8_t client_key[16];
    uint8_t client_salt[14];
    uint8_t server_key[16];
    uint8_t server_salt[14];
    bool valid;
} hu_webrtc_dtls_srtp_material_t;

hu_webrtc_ice_state_t *hu_webrtc_ice_create(hu_allocator_t *alloc);
void hu_webrtc_ice_destroy(hu_webrtc_ice_state_t *ice);
hu_error_t hu_webrtc_ice_gather(hu_webrtc_ice_state_t *ice, const hu_webrtc_config_t *cfg,
                                uint16_t bind_port);
hu_error_t hu_webrtc_ice_format_sdp_attributes(const hu_webrtc_ice_state_t *ice, char *buf, size_t cap,
                                               size_t *out_len);
hu_error_t hu_webrtc_ice_connect(hu_webrtc_ice_state_t *ice, const hu_webrtc_config_t *cfg,
                                 const char *remote_sdp, int *udp_fd, struct sockaddr_storage *peer,
                                 socklen_t *peer_len);

hu_error_t hu_webrtc_stun_build_binding_request(uint8_t *out, size_t cap, size_t *out_len,
                                                const uint8_t transaction_id[12]);
hu_error_t hu_webrtc_stun_parse_binding_response(const uint8_t *pkt, size_t pkt_len,
                                                 uint32_t *mapped_ipv4_be, uint16_t *mapped_port_be);
hu_error_t hu_webrtc_ice_parse_candidate_ipv4(const char *line, uint32_t *host_be, uint16_t *port,
                                              bool *has_use_candidate);

hu_webrtc_dtls_state_t *hu_webrtc_dtls_create(hu_allocator_t *alloc, char fingerprint_sha256[96]);
void hu_webrtc_dtls_destroy(hu_webrtc_dtls_state_t *dtls);
hu_error_t hu_webrtc_dtls_handshake(hu_webrtc_dtls_state_t *dtls, int udp_fd,
                                    const struct sockaddr *peer, socklen_t peer_len, bool role_is_active,
                                    hu_webrtc_dtls_srtp_material_t *material);

hu_webrtc_srtp_state_t *hu_webrtc_srtp_create(hu_allocator_t *alloc);
void hu_webrtc_srtp_destroy(hu_webrtc_srtp_state_t *s);
hu_error_t hu_webrtc_srtp_init_keys(hu_webrtc_srtp_state_t *s, const uint8_t tx_key[16],
                                    const uint8_t tx_salt[14], const uint8_t rx_key[16],
                                    const uint8_t rx_salt[14], uint32_t ssrc);
hu_error_t hu_webrtc_srtp_protect(hu_webrtc_srtp_state_t *s, const uint8_t *rtp, size_t rtp_len,
                                  uint8_t *out, size_t out_cap, size_t *out_len);
hu_error_t hu_webrtc_srtp_unprotect(hu_webrtc_srtp_state_t *s, const uint8_t *srtp, size_t srtp_len,
                                    uint8_t *out, size_t out_cap, size_t *out_len);

hu_error_t hu_webrtc_srtp_roundtrip_test(void);

hu_error_t hu_webrtc_sdp_extract_fingerprint_sha256(const char *sdp, char fingerprint[96]);
hu_error_t hu_webrtc_sdp_extract_setup_active(const char *sdp, bool *remote_is_active);
hu_error_t hu_webrtc_sdp_format_fingerprint(const uint8_t sha256[32], char fingerprint[96]);

#endif /* HU_WEBRTC_INTERNAL_H */
