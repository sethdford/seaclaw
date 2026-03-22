#include "human/voice/webrtc.h"
#include "webrtc_internal.h"
#include "human/core/http.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
#include <openssl/rand.h>
#endif

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <unistd.h>
#endif

hu_error_t hu_webrtc_sdp_format_fingerprint(const uint8_t sha256[32], char fingerprint[96]) {
    if (!sha256 || !fingerprint)
        return HU_ERR_INVALID_ARGUMENT;
    static const char *hex = "0123456789ABCDEF";
    size_t o = 0;
    for (int i = 0; i < 32; i++) {
        if (o + 2 > 95)
            return HU_ERR_IO;
        fingerprint[o++] = hex[(sha256[i] >> 4) & 0x0F];
        fingerprint[o++] = hex[sha256[i] & 0x0F];
        if (i < 31) {
            if (o + 1 > 95)
                return HU_ERR_IO;
            fingerprint[o++] = ':';
        }
    }
    fingerprint[o] = '\0';
    return HU_OK;
}

hu_error_t hu_webrtc_sdp_extract_fingerprint_sha256(const char *sdp, char fingerprint[96]) {
    if (!sdp || !fingerprint)
        return HU_ERR_INVALID_ARGUMENT;
    const char *p = strstr(sdp, "a=fingerprint:sha-256");
    if (!p)
        p = strstr(sdp, "a=fingerprint:SHA-256");
    if (!p)
        return HU_ERR_NOT_FOUND;
    const char *q = strstr(p, "sha-256");
    if (!q)
        q = strstr(p, "SHA-256");
    if (!q)
        return HU_ERR_PARSE;
    q += 7;
    while (*q == ' ' || *q == '\t')
        q++;
    size_t o = 0;
    while (*q && *q != '\r' && *q != '\n' && o + 1 < 96) {
        if (isxdigit((unsigned char)*q))
            fingerprint[o++] = (char)toupper((unsigned char)*q);
        else if (*q == ':' || *q == ' ')
            ;
        else
            break;
        q++;
    }
    fingerprint[o] = '\0';
    return o > 0 ? HU_OK : HU_ERR_PARSE;
}

hu_error_t hu_webrtc_sdp_extract_setup_active(const char *sdp, bool *remote_is_active) {
    if (!sdp || !remote_is_active)
        return HU_ERR_INVALID_ARGUMENT;
    *remote_is_active = false;
    const char *p = strstr(sdp, "a=setup:");
    if (!p)
        return HU_ERR_NOT_FOUND;
    p += 8;
    if (strncmp(p, "active", 6) == 0)
        *remote_is_active = true;
    return HU_OK;
}

#if !defined(HU_IS_TEST) || !HU_IS_TEST
static void hu_webrtc_random_bytes(uint8_t *out, size_t n) {
#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
    if (RAND_bytes(out, (int)n) != 1) {
        for (size_t i = 0; i < n; i++)
            out[i] = (uint8_t)(rand() & 0xff);
    }
#else
    for (size_t i = 0; i < n; i++)
        out[i] = (uint8_t)(rand() & 0xff);
#endif
}
#endif

#ifndef HU_IS_TEST
static hu_error_t hu_webrtc_build_sdp_offer(hu_allocator_t *alloc, const hu_webrtc_config_t *config,
                                            const char *fingerprint_sha256, const char *ice_attr_block,
                                            char **out) {
    bool audio = config ? config->audio_enabled : true;
    bool video = config ? config->video_enabled : false;
    uint8_t ufrag_raw[4];
    uint8_t pwd_raw[16];
    hu_webrtc_random_bytes(ufrag_raw, sizeof(ufrag_raw));
    hu_webrtc_random_bytes(pwd_raw, sizeof(pwd_raw));
    char ufrag[12];
    char pwd[40];
    snprintf(ufrag, sizeof(ufrag), "%02X%02X%02X%02X", ufrag_raw[0], ufrag_raw[1], ufrag_raw[2], ufrag_raw[3]);
    pwd[0] = '\0';
    static const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < 12 && i < sizeof(pwd_raw); i++) {
        size_t pl = strlen(pwd);
        snprintf(pwd + pl, sizeof(pwd) - pl, "%c%c", hex[pwd_raw[i] >> 4], hex[pwd_raw[i] & 0x0F]);
    }

    char buf[4096];
    int len = snprintf(
        buf, sizeof(buf),
        "v=0\r\n"
        "o=- %lld 2 IN IP4 127.0.0.1\r\n"
        "s=human-webrtc\r\n"
        "t=0 0\r\n"
        "a=group:BUNDLE%s%s\r\n"
        "a=ice-options:trickle\r\n"
        "a=fingerprint:sha-256 %s\r\n"
        "a=setup:actpass\r\n"
        "a=ice-ufrag:%s\r\n"
        "a=ice-pwd:%s\r\n"
        "%s"
        "%s%s",
        (long long)time(NULL), audio ? " audio" : "", video ? " video" : "", fingerprint_sha256, ufrag, pwd,
        ice_attr_block ? ice_attr_block : "",
        audio ? "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
                "c=IN IP4 0.0.0.0\r\n"
                "a=rtcp-mux\r\n"
                "a=mid:audio\r\n"
                "a=sendrecv\r\n"
                "a=rtpmap:111 opus/48000/2\r\n"
                "a=fmtp:111 minptime=10;useinbandfec=1\r\n"
              : "",
        video ? "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
                "c=IN IP4 0.0.0.0\r\n"
                "a=rtcp-mux\r\n"
                "a=mid:video\r\n"
                "a=sendrecv\r\n"
                "a=rtpmap:96 VP8/90000\r\n"
              : "");
    if (len <= 0 || (size_t)len >= sizeof(buf))
        return HU_ERR_INVALID_ARGUMENT;
    char *sdp = (char *)alloc->alloc(alloc->ctx, (size_t)len + 1);
    if (!sdp)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(sdp, buf, (size_t)len + 1);
    *out = sdp;
    return HU_OK;
}

static void hu_webrtc_close_sock(int fd) {
    if (fd < 0)
        return;
#if defined(_WIN32)
    closesocket((SOCKET)fd);
#else
    close(fd);
#endif
}
#endif /* !HU_IS_TEST */

hu_error_t hu_webrtc_session_create(hu_allocator_t *alloc, const hu_webrtc_config_t *config,
                                    hu_webrtc_session_t **out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    hu_webrtc_session_t *s = (hu_webrtc_session_t *)alloc->alloc(alloc->ctx, sizeof(*s));
    if (!s)
        return HU_ERR_OUT_OF_MEMORY;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    if (config)
        s->config = *config;
    s->udp_fd = -1;
    s->lifecycle = HU_WEBRTC_LIFECYCLE_IDLE;
#ifdef HU_IS_TEST
    *out = s;
    return HU_OK;
#else
    s->ice_state = hu_webrtc_ice_create(alloc);
    if (!s->ice_state) {
        alloc->free(alloc->ctx, s, sizeof(*s));
        return HU_ERR_OUT_OF_MEMORY;
    }
    char fp[96];
    s->dtls_state = hu_webrtc_dtls_create(alloc, fp);
    if (!s->dtls_state) {
        hu_webrtc_ice_destroy((hu_webrtc_ice_state_t *)s->ice_state);
        alloc->free(alloc->ctx, s, sizeof(*s));
        return HU_ERR_OUT_OF_MEMORY;
    }
    if (hu_webrtc_ice_gather((hu_webrtc_ice_state_t *)s->ice_state, config, config ? config->local_port : 0) !=
        HU_OK) {
        hu_webrtc_dtls_destroy((hu_webrtc_dtls_state_t *)s->dtls_state);
        hu_webrtc_ice_destroy((hu_webrtc_ice_state_t *)s->ice_state);
        alloc->free(alloc->ctx, s, sizeof(*s));
        return HU_ERR_IO;
    }
    char ice_attr[2048];
    size_t ice_len = 0;
    if (hu_webrtc_ice_format_sdp_attributes((hu_webrtc_ice_state_t *)s->ice_state, ice_attr, sizeof(ice_attr),
                                            &ice_len) != HU_OK) {
        hu_webrtc_dtls_destroy((hu_webrtc_dtls_state_t *)s->dtls_state);
        hu_webrtc_ice_destroy((hu_webrtc_ice_state_t *)s->ice_state);
        alloc->free(alloc->ctx, s, sizeof(*s));
        return HU_ERR_IO;
    }
    hu_error_t err = hu_webrtc_build_sdp_offer(alloc, config, fp, ice_attr, &s->local_sdp);
    if (err != HU_OK) {
        hu_webrtc_dtls_destroy((hu_webrtc_dtls_state_t *)s->dtls_state);
        hu_webrtc_ice_destroy((hu_webrtc_ice_state_t *)s->ice_state);
        alloc->free(alloc->ctx, s, sizeof(*s));
        return err;
    }
    s->lifecycle = HU_WEBRTC_LIFECYCLE_ICE;
    *out = s;
    return HU_OK;
#endif
}

#ifndef HU_IS_TEST
static hu_error_t hu_webrtc_fetch_remote_sdp_http(hu_webrtc_session_t *session, char **remote_out) {
#if defined(HU_HTTP_CURL) && !defined(HU_IS_TEST)
    const char *url = session->config.signaling_endpoint;
    if (!url || !url[0])
        return HU_ERR_INVALID_ARGUMENT;
    if (!session->local_sdp || !session->local_sdp[0])
        return HU_ERR_INVALID_ARGUMENT;
    char headers[512];
    if (session->config.api_key && session->config.api_key[0]) {
        int n = snprintf(headers, sizeof(headers),
                         "Content-Type: application/sdp\nAuthorization: Bearer %s",
                         session->config.api_key);
        if (n <= 0 || (size_t)n >= sizeof(headers))
            return HU_ERR_INVALID_ARGUMENT;
    } else {
        if (snprintf(headers, sizeof(headers), "Content-Type: application/sdp") >= (int)sizeof(headers))
            return HU_ERR_INVALID_ARGUMENT;
    }
    hu_http_response_t resp = {0};
    hu_error_t err = hu_http_request(session->alloc, url, "POST", headers, session->local_sdp,
                                      strlen(session->local_sdp), &resp);
    if (err != HU_OK)
        return err;
    if (resp.status_code < 200 || resp.status_code >= 300) {
        if (resp.owned && resp.body)
            hu_http_response_free(session->alloc, &resp);
        return HU_ERR_IO;
    }
    if (!resp.body || resp.body_len == 0) {
        if (resp.owned && resp.body)
            hu_http_response_free(session->alloc, &resp);
        return HU_ERR_IO;
    }
    size_t len = resp.body_len;
    char *sdp = (char *)session->alloc->alloc(session->alloc->ctx, len + 1);
    if (!sdp) {
        hu_http_response_free(session->alloc, &resp);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(sdp, resp.body, len);
    sdp[len] = '\0';
    hu_http_response_free(session->alloc, &resp);
    *remote_out = sdp;
    return HU_OK;
#else
    (void)session;
    (void)remote_out;
    return HU_ERR_NOT_SUPPORTED;
#endif
}
#endif /* !HU_IS_TEST */

hu_error_t hu_webrtc_connect(hu_webrtc_session_t *session, const char *remote_sdp_in) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    (void)remote_sdp_in;
    session->connected = true;
    session->lifecycle = HU_WEBRTC_LIFECYCLE_MEDIA;
    return HU_OK;
#else
    char *remote_sdp = NULL;
    if (remote_sdp_in && remote_sdp_in[0]) {
        size_t rl = strlen(remote_sdp_in);
        remote_sdp = (char *)session->alloc->alloc(session->alloc->ctx, rl + 1);
        if (!remote_sdp)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(remote_sdp, remote_sdp_in, rl + 1);
    } else if (session->config.signaling_endpoint && session->config.signaling_endpoint[0]) {
        hu_error_t fe = hu_webrtc_fetch_remote_sdp_http(session, &remote_sdp);
        if (fe != HU_OK)
            return fe;
    } else
        return HU_ERR_INVALID_ARGUMENT;

    if (session->remote_sdp)
        session->alloc->free(session->alloc->ctx, session->remote_sdp, strlen(session->remote_sdp) + 1);
    session->remote_sdp = remote_sdp;

    bool remote_active = false;
    (void)hu_webrtc_sdp_extract_setup_active(remote_sdp, &remote_active);
    bool dtls_as_client = !remote_active;

    if (!session->ice_state || !session->dtls_state)
        return HU_ERR_INTERNAL;

    hu_error_t ie = hu_webrtc_ice_connect((hu_webrtc_ice_state_t *)session->ice_state, &session->config,
                                          remote_sdp, &session->udp_fd, &session->remote_media,
                                          &session->remote_media_len);
    if (ie != HU_OK) {
        session->connected = false;
        return ie;
    }
    session->lifecycle = HU_WEBRTC_LIFECYCLE_DTLS;

    hu_webrtc_dtls_srtp_material_t mat;
    memset(&mat, 0, sizeof(mat));
#if !defined(_WIN32)
    if (session->udp_fd >= 0 && session->remote_media_len > 0) {
        hu_error_t de =
            hu_webrtc_dtls_handshake((hu_webrtc_dtls_state_t *)session->dtls_state, session->udp_fd,
                                     (struct sockaddr *)&session->remote_media, session->remote_media_len,
                                     dtls_as_client, &mat);
        if (de != HU_OK) {
            hu_webrtc_close_sock(session->udp_fd);
            session->udp_fd = -1;
            session->connected = false;
            return de;
        }
    }
#else
    (void)dtls_as_client;
#endif

    if (!session->srtp_state)
        session->srtp_state = hu_webrtc_srtp_create(session->alloc);
    if (!session->srtp_state) {
        hu_webrtc_close_sock(session->udp_fd);
        session->udp_fd = -1;
        return HU_ERR_OUT_OF_MEMORY;
    }
    session->srtp_ready = false;
    if (mat.valid) {
        const uint8_t *txk = dtls_as_client ? mat.client_key : mat.server_key;
        const uint8_t *txs = dtls_as_client ? mat.client_salt : mat.server_salt;
        const uint8_t *rxk = dtls_as_client ? mat.server_key : mat.client_key;
        const uint8_t *rxs = dtls_as_client ? mat.server_salt : mat.client_salt;
        hu_webrtc_random_bytes((uint8_t *)&session->rtp_ssrc, sizeof(session->rtp_ssrc));
        if (session->rtp_ssrc == 0)
            session->rtp_ssrc = 0x12345678U;
        session->rtp_seq_next = 1;
        session->rtp_ts = 0;
        if (hu_webrtc_srtp_init_keys((hu_webrtc_srtp_state_t *)session->srtp_state, txk, txs, rxk, rxs,
                                     session->rtp_ssrc) != HU_OK) {
            hu_webrtc_close_sock(session->udp_fd);
            session->udp_fd = -1;
            return HU_ERR_INTERNAL;
        }
        session->srtp_ready = true;
    }
    session->connected = true;
    session->lifecycle = HU_WEBRTC_LIFECYCLE_MEDIA;
    return HU_OK;
#endif
}

#ifndef HU_IS_TEST
static hu_error_t hu_webrtc_build_rtp_opus(uint8_t *rtp, size_t cap, uint16_t seq, uint32_t ts, uint32_t ssrc,
                                           const void *opus, size_t opus_len) {
    size_t need = 12 + opus_len;
    if (cap < need)
        return HU_ERR_INVALID_ARGUMENT;
    rtp[0] = 0x80;
    rtp[1] = 111;
    rtp[2] = (uint8_t)(seq >> 8);
    rtp[3] = (uint8_t)seq;
    rtp[4] = (uint8_t)(ts >> 24);
    rtp[5] = (uint8_t)(ts >> 16);
    rtp[6] = (uint8_t)(ts >> 8);
    rtp[7] = (uint8_t)ts;
    rtp[8] = (uint8_t)(ssrc >> 24);
    rtp[9] = (uint8_t)(ssrc >> 16);
    rtp[10] = (uint8_t)(ssrc >> 8);
    rtp[11] = (uint8_t)ssrc;
    memcpy(rtp + 12, opus, opus_len);
    return HU_OK;
}
#endif /* !HU_IS_TEST */

hu_error_t hu_webrtc_send_audio(hu_webrtc_session_t *session, const void *data, size_t data_len) {
    if (!session || !data)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    (void)data_len;
    return HU_OK;
#else
    if (session->udp_fd >= 0 && session->srtp_ready && session->srtp_state && session->remote_media_len > 0) {
        uint8_t rtp[2048];
        session->rtp_ts += (uint32_t)(data_len * 48U);
        if (hu_webrtc_build_rtp_opus(rtp, sizeof(rtp), session->rtp_seq_next++, session->rtp_ts,
                                     session->rtp_ssrc, data, data_len) != HU_OK)
            return HU_ERR_INVALID_ARGUMENT;
        uint8_t srtp_pkt[2048];
        size_t slen = 0;
        if (hu_webrtc_srtp_protect((hu_webrtc_srtp_state_t *)session->srtp_state, rtp, 12 + data_len,
                                   srtp_pkt, sizeof(srtp_pkt), &slen) != HU_OK)
            return HU_ERR_CRYPTO_ENCRYPT;
#if defined(_WIN32)
        int sn = sendto((SOCKET)session->udp_fd, (const char *)srtp_pkt, (int)slen, 0,
                        (const struct sockaddr *)&session->remote_media, (int)session->remote_media_len);
#else
        ssize_t sn = sendto(session->udp_fd, srtp_pkt, slen, 0, (const struct sockaddr *)&session->remote_media,
                            session->remote_media_len);
#endif
        if (sn < 0)
            return HU_ERR_IO;
        return HU_OK;
    }
    if (session->config.audio_endpoint && session->config.audio_endpoint[0]) {
#if defined(HU_HTTP_CURL)
        const char *headers = "Content-Type: application/octet-stream";
        hu_http_response_t resp = {0};
        hu_error_t err = hu_http_request(session->alloc, session->config.audio_endpoint, "POST", headers,
                                         (const char *)data, data_len, &resp);
        if (resp.owned && resp.body)
            hu_http_response_free(session->alloc, &resp);
        return err;
#else
        (void)data_len;
        return HU_ERR_NOT_SUPPORTED;
#endif
    }
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_webrtc_recv_audio(hu_webrtc_session_t *session, void *out_buf, size_t out_cap, size_t *out_len) {
    if (!session || !out_buf || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = 0;
#ifdef HU_IS_TEST
    (void)out_cap;
    return HU_ERR_NOT_FOUND;
#else
    if (session->udp_fd < 0 || !session->srtp_state || !session->srtp_ready)
        return HU_ERR_NOT_FOUND;
    uint8_t raw[2048];
#if defined(_WIN32)
    int rn = recv((SOCKET)session->udp_fd, (char *)raw, (int)sizeof(raw), 0);
#else
    ssize_t rn = recv(session->udp_fd, raw, sizeof(raw), 0);
#endif
    if (rn < 12)
        return HU_ERR_IO;
    uint8_t plain[2048];
    size_t plen = 0;
    if (hu_webrtc_srtp_unprotect((hu_webrtc_srtp_state_t *)session->srtp_state, raw, (size_t)rn, plain,
                                 sizeof(plain), &plen) != HU_OK)
        return HU_ERR_CRYPTO_DECRYPT;
    if (plen < 12 || plen - 12 > out_cap)
        return HU_ERR_IO;
    memcpy(out_buf, plain + 12, plen - 12);
    *out_len = plen - 12;
    return HU_OK;
#endif
}

void hu_webrtc_session_destroy(hu_webrtc_session_t *session) {
    if (!session)
        return;
    hu_allocator_t *alloc = session->alloc;
#ifndef HU_IS_TEST
    hu_webrtc_close_sock(session->udp_fd);
    session->udp_fd = -1;
    if (session->srtp_state)
        hu_webrtc_srtp_destroy((hu_webrtc_srtp_state_t *)session->srtp_state);
    if (session->dtls_state)
        hu_webrtc_dtls_destroy((hu_webrtc_dtls_state_t *)session->dtls_state);
    if (session->ice_state)
        hu_webrtc_ice_destroy((hu_webrtc_ice_state_t *)session->ice_state);
#endif
    if (session->local_sdp)
        alloc->free(alloc->ctx, session->local_sdp, strlen(session->local_sdp) + 1);
    if (session->remote_sdp)
        alloc->free(alloc->ctx, session->remote_sdp, strlen(session->remote_sdp) + 1);
    alloc->free(alloc->ctx, session, sizeof(*session));
}
