#include "human/voice/webrtc.h"
#include <string.h>
#include <stdlib.h>

hu_error_t hu_webrtc_session_create(hu_allocator_t *alloc, const hu_webrtc_config_t *config, hu_webrtc_session_t **out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
    hu_webrtc_session_t *s = alloc->alloc(alloc->ctx, sizeof(hu_webrtc_session_t));
    if (!s) return HU_ERR_OUT_OF_MEMORY;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    if (config) s->config = *config;
#ifdef HU_IS_TEST
    s->local_sdp = NULL;
#endif
    *out = s;
    return HU_OK;
}

hu_error_t hu_webrtc_connect(hu_webrtc_session_t *session, const char *remote_sdp) {
    if (!session || !remote_sdp) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
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
