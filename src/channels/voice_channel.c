#include "human/channels/voice_channel.h"
#include "human/core/string.h"
#include "human/voice/realtime.h"
#include "human/voice/webrtc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Voice channel with configurable mode:
 *   HU_VOICE_MODE_SONATA (default) — Sonata TTS/STT pipeline
 *   HU_VOICE_MODE_REALTIME — OpenAI Realtime API (full-duplex WebSocket)
 *   HU_VOICE_MODE_WEBRTC — WebRTC voice (SDP exchange + audio streaming) */

#ifdef HU_HAS_SONATA
/* FFI declarations for Sonata Rust pipeline.
 * Rust FFI uses *const u8 / *mut u8 for byte buffers, which maps to uint8_t
 * in C (not char, which may be signed on some platforms). */
extern int32_t sonata_pipeline_init(const uint8_t *config_json, size_t config_len);
extern int32_t sonata_stt(const float *audio, size_t samples, uint8_t *text, size_t *text_len);
extern int32_t sonata_tts(const uint8_t *text, size_t text_len, const uint8_t *speaker_id,
                          float emotion_exag, float *audio, size_t *audio_len);
extern int32_t sonata_speaker_encode(const float *audio, size_t samples, float *embedding);
extern void sonata_pipeline_deinit(void);
#endif

#define VOICE_MAX_SAMPLES (24000u * 10u) /* 10s at 24kHz */

typedef struct hu_voice_ctx {
    hu_allocator_t *alloc;
    hu_channel_voice_config_t config;
    bool initialized;
    bool running;
    hu_voice_rt_session_t *rt_session;
    hu_webrtc_session_t *webrtc_session;
} hu_voice_ctx_t;

static hu_error_t voice_start(void *ctx) {
    hu_voice_ctx_t *v = (hu_voice_ctx_t *)ctx;
    if (!v)
        return HU_ERR_INVALID_ARGUMENT;

    if (v->config.mode == HU_VOICE_MODE_REALTIME) {
#if HU_IS_TEST
        v->running = true;
        return HU_OK;
#else
        hu_voice_rt_config_t rt_cfg = {0};
        rt_cfg.sample_rate = (int)v->config.sample_rate;
        hu_error_t err = hu_voice_rt_session_create(v->alloc, &rt_cfg, &v->rt_session);
        if (err != HU_OK)
            return err;
        err = hu_voice_rt_connect(v->rt_session);
        if (err != HU_OK) {
            hu_voice_rt_session_destroy(v->rt_session);
            v->rt_session = NULL;
            return err;
        }
        v->running = true;
        return HU_OK;
#endif
    }

    if (v->config.mode == HU_VOICE_MODE_WEBRTC) {
#if HU_IS_TEST
        v->running = true;
        return HU_OK;
#else
        hu_webrtc_config_t wc_cfg = {0};
        wc_cfg.audio_enabled = true;
        hu_error_t err = hu_webrtc_session_create(v->alloc, &wc_cfg, &v->webrtc_session);
        if (err != HU_OK)
            return err;
        v->running = true;
        return HU_OK;
#endif
    }

#ifdef HU_HAS_SONATA
    if (!v->initialized) {
        int32_t err = sonata_pipeline_init(NULL, 0);
        if (err != 0)
            return HU_ERR_CHANNEL_START;
        v->initialized = true;
    }
#endif

    v->running = true;
    return HU_OK;
}

static void voice_stop(void *ctx) {
    hu_voice_ctx_t *v = (hu_voice_ctx_t *)ctx;
    if (!v)
        return;
    v->running = false;

    if (v->rt_session) {
        hu_voice_rt_session_destroy(v->rt_session);
        v->rt_session = NULL;
    }
    if (v->webrtc_session) {
        hu_webrtc_session_destroy(v->webrtc_session);
        v->webrtc_session = NULL;
    }

#ifdef HU_HAS_SONATA
    if (v->initialized) {
        sonata_pipeline_deinit();
        v->initialized = false;
    }
#endif
}

static hu_error_t voice_send(void *ctx, const char *target, size_t target_len, const char *message,
                             size_t message_len, const char *const *media, size_t media_count) {
    hu_voice_ctx_t *v = (hu_voice_ctx_t *)ctx;
    if (!v || !message)
        return HU_ERR_INVALID_ARGUMENT;

    (void)target;
    (void)target_len;
    (void)media;
    (void)media_count;

    if (v->config.mode == HU_VOICE_MODE_REALTIME && v->rt_session) {
        return hu_voice_rt_send_audio(v->rt_session, message, message_len);
    }
    if (v->config.mode == HU_VOICE_MODE_WEBRTC && v->webrtc_session) {
        return hu_webrtc_send_audio(v->webrtc_session, message, message_len);
    }

    (void)message_len;

#ifdef HU_HAS_SONATA
    const size_t buf_samples = VOICE_MAX_SAMPLES;
    const size_t buf_bytes = buf_samples * sizeof(float);
    float *audio_buf = (float *)v->alloc->alloc(v->alloc->ctx, buf_bytes);
    if (!audio_buf) {
#if !HU_IS_TEST
        fprintf(stderr, "voice_channel: failed to allocate audio buffer (%zu bytes)\n", buf_bytes);
#endif
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t audio_len = buf_samples;
    float exag = v->config.emotion_exaggeration_set ? v->config.emotion_exaggeration : 1.0f;

    int32_t err = sonata_tts((const uint8_t *)message, message_len,
                             (const uint8_t *)v->config.speaker_id, exag, audio_buf, &audio_len);
    if (err != 0) {
#if !HU_IS_TEST
        fprintf(stderr, "voice_channel: sonata_tts failed (err=%d)\n", err);
#endif
        v->alloc->free(v->alloc->ctx, audio_buf, buf_bytes);
        return HU_ERR_CHANNEL_SEND;
    }

    if (v->config.on_audio_ready) {
        v->config.on_audio_ready(audio_buf, audio_len, v->config.callback_user_data);
    }

    v->alloc->free(v->alloc->ctx, audio_buf, buf_bytes);
    return HU_OK;
#else
    /* Cloud TTS fallback: deliver text as-is to the audio callback.
     * Callers that provide on_audio_ready can use a cloud TTS service
     * (e.g., Google Cloud TTS, AWS Polly) to synthesize the text.
     * Without Sonata or a callback, voice output is not available. */
    if (v->config.on_audio_ready) {
        v->config.on_audio_ready((const float *)message, message_len, v->config.callback_user_data);
        return HU_OK;
    }
#if HU_IS_TEST
    return HU_OK;
#else
    fprintf(stderr, "voice_channel: no TTS backend available (Sonata not built, no audio callback)\n");
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

static const char *voice_name(void *ctx) {
    (void)ctx;
    return "voice";
}

static bool voice_health_check(void *ctx) {
    hu_voice_ctx_t *v = (hu_voice_ctx_t *)ctx;
    return v && v->running;
}

hu_error_t hu_voice_poll(void *channel_ctx, hu_allocator_t *alloc,
                         hu_channel_loop_msg_t *msgs, size_t max_msgs, size_t *out_count) {
    hu_voice_ctx_t *v = (hu_voice_ctx_t *)channel_ctx;
    if (!v || !alloc || !msgs || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;

    if (!v->running)
        return HU_OK;

#if HU_IS_TEST
    (void)max_msgs;
    return HU_OK;
#else
#ifdef HU_HAS_SONATA
    if (!v->config.on_audio_input_request)
        return HU_OK;

    float audio_buf[VOICE_MAX_SAMPLES];
    size_t samples = 0;
    if (!v->config.on_audio_input_request(audio_buf, VOICE_MAX_SAMPLES, &samples,
                                          v->config.callback_user_data) ||
        samples == 0)
        return HU_OK;

    uint8_t text_buf[4096];
    size_t text_len = sizeof(text_buf);
    int32_t stt_err = sonata_stt(audio_buf, samples, text_buf, &text_len);
    if (stt_err != 0 || text_len == 0)
        return HU_OK;

    if (max_msgs == 0)
        return HU_OK;

    memset(&msgs[0], 0, sizeof(msgs[0]));
    if (text_len >= sizeof(msgs[0].content))
        text_len = sizeof(msgs[0].content) - 1;
    memcpy(msgs[0].content, text_buf, text_len);
    msgs[0].content[text_len] = '\0';
    memcpy(msgs[0].session_key, "voice-user", 10);
    msgs[0].session_key[10] = '\0';
    msgs[0].is_group = false;
    msgs[0].message_id = -1;
    *out_count = 1;
#else
    /* Cloud STT fallback: if an audio input callback is provided,
     * request audio and pass it through as raw text for external
     * STT processing. Without Sonata, the caller is responsible
     * for running speech recognition on the raw audio. */
    if (v->config.on_audio_input_request && max_msgs > 0) {
        float audio_buf[VOICE_MAX_SAMPLES];
        size_t samples = 0;
        if (v->config.on_audio_input_request(audio_buf, VOICE_MAX_SAMPLES, &samples,
                                              v->config.callback_user_data) &&
            samples > 0) {
            memset(&msgs[0], 0, sizeof(msgs[0]));
            int n = snprintf(msgs[0].content, sizeof(msgs[0].content),
                             "[voice:%zu samples, needs external STT]", samples);
            if (n > 0)
                msgs[0].content[(size_t)n < sizeof(msgs[0].content) ? (size_t)n : sizeof(msgs[0].content) - 1] = '\0';
            memcpy(msgs[0].session_key, "voice-user", 10);
            msgs[0].session_key[10] = '\0';
            msgs[0].is_group = false;
            msgs[0].message_id = -1;
            *out_count = 1;
            return HU_OK;
        }
    }
    (void)max_msgs;
#endif
    return HU_OK;
#endif
}

static const hu_channel_vtable_t voice_vtable = {
    .start = voice_start,
    .stop = voice_stop,
    .send = voice_send,
    .name = voice_name,
    .health_check = voice_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
};

hu_error_t hu_channel_voice_create(hu_allocator_t *alloc, const hu_channel_voice_config_t *config,
                                   hu_channel_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_voice_ctx_t *v = (hu_voice_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*v));
    if (!v)
        return HU_ERR_OUT_OF_MEMORY;
    memset(v, 0, sizeof(*v));

    v->alloc = alloc;

    if (config) {
        v->config = *config;
    }

    /* Apply defaults */
    if (v->config.sample_rate == 0)
        v->config.sample_rate = 24000;

    out->ctx = v;
    out->vtable = &voice_vtable;
    return HU_OK;
}

void hu_channel_voice_destroy(hu_channel_t *ch) {
    if (ch && ch->ctx) {
        hu_voice_ctx_t *v = (hu_voice_ctx_t *)ch->ctx;
        hu_allocator_t *a = v->alloc;
        if (v->running)
            voice_stop(v);
        a->free(a->ctx, v, sizeof(*v));
        ch->ctx = NULL;
        ch->vtable = NULL;
    }
}
