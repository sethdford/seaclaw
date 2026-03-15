#include "human/voice/realtime.h"
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
#else
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_voice_rt_send_audio(hu_voice_rt_session_t *session, const void *data, size_t data_len) {
    if (!session || !data) return HU_ERR_INVALID_ARGUMENT;
    (void)data_len;
#ifdef HU_IS_TEST
    return HU_OK;
#else
    return HU_ERR_NOT_SUPPORTED;
#endif
}

void hu_voice_rt_session_destroy(hu_voice_rt_session_t *session) {
    if (!session) return;
    hu_allocator_t *alloc = session->alloc;
    if (session->session_id) alloc->free(alloc->ctx, session->session_id, strlen(session->session_id)+1);
    alloc->free(alloc->ctx, session, sizeof(hu_voice_rt_session_t));
}
