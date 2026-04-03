#ifndef HU_CHANNELS_VOICE_CHANNEL_H
#define HU_CHANNELS_VOICE_CHANNEL_H

#include "human/channel.h"
#include "human/channel_loop.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Sonata Voice Channel — native on-device STT/TTS via Sonata pipeline.
 *
 * When HU_HAS_SONATA is defined, this channel uses the Rust Sonata pipeline
 * for native speech-to-text and text-to-speech. Otherwise, it falls back
 * to the cloud-based hu_voice_stt/tts API.
 */

/** Callback invoked when TTS audio is ready for playback. */
typedef void (*hu_voice_audio_callback_t)(const float *audio, size_t samples, void *user_data);

/** Callback invoked with text that needs external TTS synthesis.
 * Used by cloud fallback when Sonata is not available. */
typedef void (*hu_voice_text_callback_t)(const char *text, size_t len, void *user_data);

/** Callback to poll for audio input (microphone). Returns true if audio available.
 * Fills buf with up to max_samples; sets *out_samples to actual count. */
typedef bool (*hu_voice_audio_input_callback_t)(float *buf, size_t max_samples, size_t *out_samples,
                                                void *user_data);

typedef enum hu_voice_mode {
    HU_VOICE_MODE_SONATA = 0,   /* Default: Sonata TTS/STT */
    HU_VOICE_MODE_REALTIME,     /* OpenAI Realtime API (full-duplex) */
    HU_VOICE_MODE_WEBRTC,       /* WebRTC-based voice */
    HU_VOICE_MODE_GEMINI_LIVE,  /* Gemini Live (Multimodal Live API over WebSocket) */
} hu_voice_mode_t;

typedef struct hu_channel_voice_config {
    hu_voice_mode_t mode;                      /* Sonata (default), Realtime, or WebRTC */
    const char *codec_model_path;              /* Path to codec weights */
    const char *stt_model_path;               /* Path to STT weights */
    const char *tts_model_path;               /* Path to TTS weights */
    const char *cam_model_path;               /* Path to CAM++ weights */
    const char *cfm_model_path;               /* Path to CFM weights */
    const char *speaker_id;                   /* Speaker identity for TTS */
    float emotion_exaggeration;               /* 0.0 - 2.0, default 1.0 */
    bool emotion_exaggeration_set;            /* true if explicitly configured */
    uint32_t sample_rate;                     /* Default: 24000 */
    const char *api_key; /* OpenAI API key for Realtime mode */
    const char *model;   /* Model name, e.g. "gpt-4o-realtime-preview" */
    const char *voice;   /* Voice name, e.g. "alloy" */
    bool enable_full_duplex;                  /* Enable overlapping speech */
    bool enable_backchanneling;               /* Enable hmm/right/oh responses */
    hu_voice_audio_callback_t on_audio_ready; /* Callback for generated audio */
    hu_voice_text_callback_t on_text_ready;   /* Cloud fallback: text for external TTS */
    hu_voice_audio_input_callback_t on_audio_input_request; /* Optional: poll for mic input */
    void *callback_user_data;                 /* User data for callbacks */
} hu_channel_voice_config_t;

/**
 * Create a voice channel.
 *
 * @param alloc  Allocator for memory management
 * @param config Voice channel configuration (NULL for defaults)
 * @param out    Output channel handle
 * @return HU_OK on success
 */
hu_error_t hu_channel_voice_create(hu_allocator_t *alloc, const hu_channel_voice_config_t *config,
                                   hu_channel_t *out);

/**
 * Destroy a voice channel and release resources.
 */
void hu_channel_voice_destroy(hu_channel_t *ch);

/**
 * Poll for voice input (STT). Used by channel loop when voice is configured
 * with on_audio_input_request. Returns transcribed messages.
 */
hu_error_t hu_voice_poll(void *channel_ctx, hu_allocator_t *alloc,
                         hu_channel_loop_msg_t *msgs, size_t max_msgs, size_t *out_count);

#endif /* HU_CHANNELS_VOICE_CHANNEL_H */
