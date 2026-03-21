#include "human/voice/session.h"
#include <string.h>
#include <time.h>

static int64_t voice_session_now_ms(void) {
#if HU_IS_TEST
    return 1;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
#endif
}

hu_error_t hu_voice_session_start(hu_allocator_t *alloc, hu_voice_session_t *session,
                                  const char *channel_name, size_t channel_name_len,
                                  const hu_config_t *config) {
    if (!alloc || !session || !config)
        return HU_ERR_INVALID_ARGUMENT;
    if (channel_name_len > 0 && !channel_name)
        return HU_ERR_INVALID_ARGUMENT;

    memset(session, 0, sizeof(*session));
    hu_duplex_session_init(&session->duplex);

    size_t copy_len = channel_name_len;
    if (copy_len >= sizeof(session->channel_name))
        copy_len = sizeof(session->channel_name) - 1;
    if (channel_name && copy_len > 0)
        memcpy(session->channel_name, channel_name, copy_len);
    session->channel_name[copy_len] = '\0';

#if HU_IS_TEST
    session->started_at = voice_session_now_ms();
    session->active = true;
    return HU_OK;
#else
    session->started_at = voice_session_now_ms();
    session->active = true;

    const char *tp = config->voice.tts_provider;
    const char *rt_model =
        (config->voice.realtime_model && config->voice.realtime_model[0])
            ? config->voice.realtime_model
            : config->voice.tts_model;
    const char *rt_voice =
        (config->voice.realtime_voice && config->voice.realtime_voice[0])
            ? config->voice.realtime_voice
            : config->voice.tts_voice;
    bool realtime_mode = (config->voice.mode && strcmp(config->voice.mode, "realtime") == 0) ||
                         (tp && strcmp(tp, "realtime") == 0);
    if (realtime_mode) {
        const char *key = hu_config_get_provider_key(config, "openai");
        if (key && key[0]) {
            hu_voice_rt_config_t rtc = {.model = rt_model,
                                        .voice = rt_voice,
                                        .api_key = key,
                                        .sample_rate = 24000,
                                        .vad_enabled = true};
            hu_voice_rt_session_t *rt = NULL;
            if (hu_voice_rt_session_create(alloc, &rtc, &rt) == HU_OK && rt) {
                if (hu_voice_rt_connect(rt) == HU_OK)
                    session->rt = rt;
                else
                    hu_voice_rt_session_destroy(rt);
            }
        }
    }
    return HU_OK;
#endif
}

hu_error_t hu_voice_session_stop(hu_voice_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    if (session->rt) {
        hu_voice_rt_session_destroy(session->rt);
        session->rt = NULL;
    }
    (void)hu_duplex_session_init(&session->duplex);
    session->active = false;
    session->started_at = 0;
    session->last_audio_ms = 0;
    return HU_OK;
}

hu_error_t hu_voice_session_send_audio(hu_voice_session_t *session, const uint8_t *pcm16,
                                       size_t pcm16_len) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    if (!session->active)
        return HU_ERR_INVALID_ARGUMENT;
    if (pcm16_len > 0 && !pcm16)
        return HU_ERR_INVALID_ARGUMENT;

    int64_t now = voice_session_now_ms();
    session->last_audio_ms = now;
    hu_error_t err = hu_duplex_handle_input(&session->duplex, now);
    if (err != HU_OK)
        return err;

    if (pcm16_len == 0)
        return HU_OK;

    if (session->rt && session->rt->connected)
        return hu_voice_rt_send_audio(session->rt, pcm16, pcm16_len);
    return HU_OK;
}

hu_error_t hu_voice_session_on_interrupt(hu_voice_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    if (!session->active)
        return HU_OK;

    session->duplex.interrupt_detected = true;
    (void)hu_duplex_cancel_output(&session->duplex);

    if (session->rt && session->rt->connected)
        return hu_voice_rt_response_cancel(session->rt);
    return HU_OK;
}
