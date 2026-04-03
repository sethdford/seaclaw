#include "human/tools/send_voice_message.h"
#include "human/agent/tool_context.h"
#include "human/core/json.h"
#include "human/tool.h"
#include <stdlib.h>
#include <string.h>

#define TOOL_NAME "send_voice_message"
#define TOOL_DESC                                                                               \
    "Request that the current response be delivered as a voice message using your persona's "    \
    "cloned voice via Cartesia TTS. Best for emotional, comforting, or personal messages where " \
    "hearing your voice would be more impactful than reading text. Requires a configured "       \
    "voice_id on the persona and voice_enabled on the channel. Optionally provide a custom "     \
    "transcript (otherwise the full response text is spoken) and an emotion override. "          \
    "Can only be called once per turn."
#define TOOL_PARAMS                                                                                \
    "{\"type\":\"object\",\"properties\":{"                                                        \
    "\"transcript\":{\"type\":\"string\",\"description\":\"Custom text to speak. If omitted, the " \
    "full response text is used.\"},"                                                              \
    "\"emotion\":{\"type\":\"string\",\"description\":\"Cartesia emotion override "                \
    "(e.g. sympathetic, excited, content, calm, sad, contemplative). Auto-detected if omitted.\"}" \
    "}}"

#define MAX_TRANSCRIPT_LEN 4000

static const char *const VALID_EMOTIONS[] = {
    "neutral",      "angry",         "mad",           "outraged",
    "frustrated",   "agitated",      "threatened",    "disgusted",
    "contempt",     "envious",       "sarcastic",     "ironic",
    "excited",      "content",       "happy",         "enthusiastic",
    "elated",       "euphoric",      "triumphant",    "amazed",
    "surprised",    "flirtatious",   "joking/comedic","curious",
    "peaceful",     "serene",        "calm",          "grateful",
    "affectionate", "trust",         "sympathetic",   "anticipation",
    "mysterious",   "sad",           "dejected",      "melancholic",
    "disappointed", "hurt",          "guilty",        "bored",
    "tired",        "rejected",      "nostalgic",     "wistful",
    "apologetic",   "hesitant",      "insecure",      "confused",
    "resigned",     "anxious",       "panicked",      "alarmed",
    "scared",       "proud",         "confident",     "distant",
    "skeptical",    "contemplative", "determined",
};
#define VALID_EMOTIONS_COUNT (sizeof(VALID_EMOTIONS) / sizeof(VALID_EMOTIONS[0]))

static bool is_valid_emotion(const char *emotion) {
    for (size_t i = 0; i < VALID_EMOTIONS_COUNT; i++) {
        if (strcmp(emotion, VALID_EMOTIONS[i]) == 0)
            return true;
    }
    return false;
}

/* Clamp length to a UTF-8 character boundary (don't split multi-byte sequences). */
static size_t clamp_utf8(const char *text, size_t max_len) {
    if (max_len == 0)
        return 0;
    size_t len = strlen(text);
    if (len <= max_len)
        return len;
    size_t pos = max_len;
    while (pos > 0 && ((unsigned char)text[pos] & 0xC0) == 0x80)
        pos--;
    return pos;
}

static hu_error_t svm_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                              hu_tool_result_t *out) {
    (void)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!alloc) {
        *out = hu_tool_result_fail("allocator required", 18);
        return HU_OK;
    }

    hu_agent_t *agent = hu_agent_get_current_for_tools();
    if (!agent) {
        *out = hu_tool_result_fail("agent context not available", 28);
        return HU_OK;
    }

    if (hu_agent_has_pending_voice()) {
        *out = hu_tool_result_fail(
            "voice message already requested this turn", 42);
        return HU_OK;
    }

    const char *transcript = NULL;
    size_t transcript_len = 0;
    const char *emotion = NULL;

    if (args && args->type == HU_JSON_OBJECT) {
        const char *t = hu_json_get_string(args, "transcript");
        if (t && t[0]) {
            transcript_len = clamp_utf8(t, MAX_TRANSCRIPT_LEN);
            transcript = t;
        }

        const char *e = hu_json_get_string(args, "emotion");
        if (e && e[0]) {
            if (!is_valid_emotion(e)) {
                *out = hu_tool_result_fail(
                    "unknown emotion; use: content, calm, excited, sympathetic, "
                    "sad, contemplative, neutral, or other Cartesia emotions", 109);
                return HU_OK;
            }
            emotion = e;
        }
    }

    hu_agent_request_voice_send(emotion, transcript, transcript_len);

    const char *msg = transcript
                          ? "Voice message requested with custom transcript. "
                            "Audio will be synthesized if voice is configured."
                          : "Voice message requested. Response text will be "
                            "synthesized as audio if voice is configured.";
    size_t msg_len = strlen(msg);
    char *resp = (char *)alloc->alloc(alloc->ctx, msg_len + 1);
    if (!resp) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_OK;
    }
    memcpy(resp, msg, msg_len + 1);
    *out = hu_tool_result_ok_owned(resp, msg_len);
    return HU_OK;
}

static const char *svm_name(void *ctx) {
    (void)ctx;
    return TOOL_NAME;
}
static const char *svm_desc(void *ctx) {
    (void)ctx;
    return TOOL_DESC;
}
static const char *svm_params(void *ctx) {
    (void)ctx;
    return TOOL_PARAMS;
}

static const hu_tool_vtable_t svm_vtable = {
    .execute = svm_execute,
    .name = svm_name,
    .description = svm_desc,
    .parameters_json = svm_params,
    .deinit = NULL,
};

hu_error_t hu_send_voice_message_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    (void)alloc;
    out->ctx = NULL;
    out->vtable = &svm_vtable;
    return HU_OK;
}
