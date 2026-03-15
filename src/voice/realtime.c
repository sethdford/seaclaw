#include "human/voice/realtime.h"
#include "human/multimodal.h"
#include "human/websocket/websocket.h"
#include <stdio.h>
#include <string.h>

hu_error_t hu_voice_rt_session_create(hu_allocator_t *alloc, const hu_voice_rt_config_t *config, hu_voice_rt_session_t **out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
    hu_voice_rt_session_t *s = alloc->alloc(alloc->ctx, sizeof(hu_voice_rt_session_t));
    if (!s) return HU_ERR_OUT_OF_MEMORY;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    if (config) s->config = *config;
    *out = s;
    return HU_OK;
}

hu_error_t hu_voice_rt_connect(hu_voice_rt_session_t *session) {
    if (!session) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    session->connected = true;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    const char *base = session->config.base_url && session->config.base_url[0]
                          ? session->config.base_url
                          : "wss://api.openai.com/v1/realtime";
    char url[512];
    int n;
    if (session->config.model && session->config.model[0])
        n = snprintf(url, sizeof(url), "%s?model=%s", base, session->config.model);
    else
        n = snprintf(url, sizeof(url), "%s", base);
    if (n <= 0 || (size_t)n >= sizeof(url))
        return HU_ERR_INVALID_ARGUMENT;

    hu_ws_client_t *ws = NULL;
    hu_error_t err = hu_ws_connect(session->alloc, url, &ws);
    if (err != HU_OK || !ws)
        return err;

    session->ws_client = ws;
    session->connected = true;
    return HU_OK;
#else
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_voice_rt_send_audio(hu_voice_rt_session_t *session, const void *data, size_t data_len) {
    if (!session || !data) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    (void)data_len;
    return HU_OK;
#elif defined(HU_HTTP_CURL)
    if (!session->connected || !session->ws_client)
        return HU_ERR_IO;

    char *b64 = NULL;
    size_t b64_len = 0;
    hu_error_t err = hu_multimodal_encode_base64(session->alloc, data, data_len, &b64, &b64_len);
    if (err != HU_OK || !b64)
        return err;

    size_t json_cap = 64 + b64_len * 2;
    char *json = (char *)session->alloc->alloc(session->alloc->ctx, json_cap);
    if (!json) {
        session->alloc->free(session->alloc->ctx, b64, b64_len + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    int n = snprintf(json, json_cap, "{\"type\":\"input_audio_buffer.append\",\"audio\":\"%.*s\"}",
                     (int)b64_len, b64);
    session->alloc->free(session->alloc->ctx, b64, b64_len + 1);
    if (n <= 0 || (size_t)n >= json_cap) {
        session->alloc->free(session->alloc->ctx, json, json_cap);
        return HU_ERR_INVALID_ARGUMENT;
    }

    err = hu_ws_send((hu_ws_client_t *)session->ws_client, json, (size_t)n);
    session->alloc->free(session->alloc->ctx, json, json_cap);
    return err;
#else
    (void)data_len;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

void hu_voice_rt_session_destroy(hu_voice_rt_session_t *session) {
    if (!session) return;
    hu_allocator_t *alloc = session->alloc;
#if defined(HU_HTTP_CURL) && !HU_IS_TEST
    if (session->ws_client) {
        hu_ws_client_free((hu_ws_client_t *)session->ws_client, alloc);
        session->ws_client = NULL;
    }
#endif
    if (session->session_id) alloc->free(alloc->ctx, session->session_id, strlen(session->session_id)+1);
    alloc->free(alloc->ctx, session, sizeof(hu_voice_rt_session_t));
}
