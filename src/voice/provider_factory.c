/*
 * Voice provider factory — create a provider from hu_config_t + mode string.
 * Centralizes API key lookup, model/voice defaults, and backend config construction.
 */
#include "human/config.h"
#include "human/voice/gemini_live.h"
#include "human/voice/provider.h"
#include <string.h>

hu_error_t hu_voice_provider_create_from_config(hu_allocator_t *alloc, const hu_config_t *config,
                                                const char *mode,
                                                const hu_voice_provider_extras_t *extras,
                                                hu_voice_provider_t *out) {
    if (!alloc || !config || !mode || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    const char *rt_model = (config->voice.realtime_model && config->voice.realtime_model[0])
                               ? config->voice.realtime_model
                               : config->voice.tts_model;
    const char *rt_voice = (config->voice.realtime_voice && config->voice.realtime_voice[0])
                               ? config->voice.realtime_voice
                               : config->voice.tts_voice;

    if (strcmp(mode, "gemini_live") == 0) {
        const char *key = hu_config_get_provider_key(config, "google");
        if (!key || !key[0])
            key = hu_config_get_provider_key(config, "gemini");
        if (!key || !key[0])
            return HU_ERR_INVALID_ARGUMENT;

        if (!rt_model || !rt_model[0])
            rt_model = "gemini-3.1-flash-live-preview";
        if (!rt_voice || !rt_voice[0])
            rt_voice = "Puck";

        hu_gemini_live_config_t glc = {
            .api_key = key,
            .access_token = config->voice.vertex_access_token,
            .model = rt_model,
            .voice = rt_voice,
            .region = config->voice.vertex_region,
            .project_id = config->voice.vertex_project,
            .transcribe_input = true,
            .transcribe_output = true,
            .affective_dialog = true,
            .manual_vad = true,
            .enable_session_resumption = true,
            .thinking_level = HU_GL_THINKING_MINIMAL,
        };
        if (extras) {
            glc.system_instruction = extras->system_instruction;
            glc.tools_json = extras->tools_json;
        }
        return hu_voice_provider_gemini_live_create(alloc, &glc, out);
    }

    if (strcmp(mode, "openai_realtime") == 0 || strcmp(mode, "realtime") == 0) {
        const char *key = hu_config_get_provider_key(config, "openai");
        if (!key || !key[0])
            return HU_ERR_INVALID_ARGUMENT;

        if (!rt_model || !rt_model[0])
            rt_model = "gpt-4o-realtime-preview";
        if (!rt_voice || !rt_voice[0])
            rt_voice = "alloy";

        hu_voice_rt_config_t rtc = {
            .api_key = key,
            .model = rt_model,
            .voice = rt_voice,
            .sample_rate = 24000,
            .vad_enabled = true,
        };
        return hu_voice_provider_openai_create(alloc, &rtc, out);
    }

    return HU_ERR_NOT_SUPPORTED;
}
