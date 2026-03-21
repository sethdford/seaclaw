#include "human/voice/webrtc.h"
#include "human/core/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static hu_error_t generate_sdp_offer(hu_allocator_t *alloc, const hu_webrtc_config_t *config,
                                      char **out) {
    char buf[2048];
    bool audio = config ? config->audio_enabled : true;
    bool video = config ? config->video_enabled : false;

    int len = snprintf(buf, sizeof(buf),
        "v=0\r\n"
        "o=- %lld 2 IN IP4 127.0.0.1\r\n"
        "s=human-webrtc\r\n"
        "t=0 0\r\n"
        "a=group:BUNDLE%s%s\r\n"
        "a=ice-options:trickle\r\n"
        "a=fingerprint:sha-256 00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:"
        "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00\r\n"
        "a=setup:actpass\r\n"
        "a=ice-ufrag:hu%04x\r\n"
        "a=ice-pwd:hu%08x%08x\r\n"
        "%s%s",
        (long long)time(NULL),
        audio ? " audio" : "", video ? " video" : "",
        (unsigned)(time(NULL) & 0xFFFF),
        (unsigned)(time(NULL) ^ 0xDEADBEEF), (unsigned)(time(NULL) + 0x12345678),
        audio ?
            "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
            "c=IN IP4 0.0.0.0\r\n"
            "a=rtcp:9 IN IP4 0.0.0.0\r\n"
            "a=mid:audio\r\n"
            "a=sendrecv\r\n"
            "a=rtpmap:111 opus/48000/2\r\n"
            "a=fmtp:111 minptime=10;useinbandfec=1\r\n" : "",
        video ?
            "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
            "c=IN IP4 0.0.0.0\r\n"
            "a=rtcp:9 IN IP4 0.0.0.0\r\n"
            "a=mid:video\r\n"
            "a=sendrecv\r\n"
            "a=rtpmap:96 VP8/90000\r\n" : "");
    if (len <= 0 || (size_t)len >= sizeof(buf))
        return HU_ERR_INVALID_ARGUMENT;
    char *sdp = (char *)alloc->alloc(alloc->ctx, (size_t)len + 1);
    if (!sdp)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(sdp, buf, (size_t)len + 1);
    *out = sdp;
    return HU_OK;
}

hu_error_t hu_webrtc_session_create(hu_allocator_t *alloc, const hu_webrtc_config_t *config, hu_webrtc_session_t **out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
    hu_webrtc_session_t *s = alloc->alloc(alloc->ctx, sizeof(hu_webrtc_session_t));
    if (!s) return HU_ERR_OUT_OF_MEMORY;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    if (config) s->config = *config;
#ifdef HU_IS_TEST
    s->local_sdp = NULL;
#else
    {
        hu_error_t err = generate_sdp_offer(alloc, config, &s->local_sdp);
        if (err != HU_OK) {
            alloc->free(alloc->ctx, s, sizeof(hu_webrtc_session_t));
            return err;
        }
    }
#endif
    *out = s;
    return HU_OK;
}

hu_error_t hu_webrtc_connect(hu_webrtc_session_t *session, const char *remote_sdp) {
    if (!session || !remote_sdp) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    session->connected = true;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    (void)remote_sdp;
    if (!session->local_sdp || !session->local_sdp[0])
        return HU_ERR_INVALID_ARGUMENT;
    const char *url = session->config.signaling_endpoint;
    if (!url || !url[0]) {
#if !HU_IS_TEST
        fprintf(stderr, "webrtc: signaling_endpoint is required for HTTP signaling connect\n");
#endif
        return HU_ERR_INVALID_ARGUMENT;
    }

    char headers[512];
    size_t hlen = 0;
    if (session->config.api_key && session->config.api_key[0]) {
        int n = snprintf(headers, sizeof(headers),
                         "Content-Type: application/sdp\nAuthorization: Bearer %s",
                         session->config.api_key);
        if (n <= 0 || (size_t)n >= sizeof(headers))
            return HU_ERR_INVALID_ARGUMENT;
        hlen = (size_t)n;
    } else {
        hlen = (size_t)snprintf(headers, sizeof(headers), "Content-Type: application/sdp");
        if (hlen >= sizeof(headers))
            return HU_ERR_INVALID_ARGUMENT;
    }

    hu_http_response_t resp = {0};
    hu_error_t err = hu_http_request(session->alloc, url, "POST", headers,
                                     session->local_sdp, strlen(session->local_sdp), &resp);
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

    if (session->remote_sdp)
        session->alloc->free(session->alloc->ctx, session->remote_sdp,
                             strlen(session->remote_sdp) + 1);
    session->remote_sdp = sdp;
    session->connected = true;
    return HU_OK;
#else
    (void)remote_sdp;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_webrtc_send_audio(hu_webrtc_session_t *session, const void *data, size_t data_len) {
    if (!session || !data) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    (void)data_len;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    const char *url = session->config.audio_endpoint;
    if (!url || !url[0])
        return HU_ERR_NOT_SUPPORTED;

    const char *headers = "Content-Type: application/octet-stream";
    hu_http_response_t resp = {0};
    hu_error_t err = hu_http_request(session->alloc, url, "POST", headers,
                                     (const char *)data, data_len, &resp);
    if (resp.owned && resp.body)
        hu_http_response_free(session->alloc, &resp);
    return err;
#else
    (void)data_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

void hu_webrtc_session_destroy(hu_webrtc_session_t *session) {
    if (!session) return;
    hu_allocator_t *alloc = session->alloc;
    if (session->local_sdp) alloc->free(alloc->ctx, session->local_sdp, strlen(session->local_sdp)+1);
    if (session->remote_sdp) alloc->free(alloc->ctx, session->remote_sdp, strlen(session->remote_sdp)+1);
    alloc->free(alloc->ctx, session, sizeof(hu_webrtc_session_t));
}
