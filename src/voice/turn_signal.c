#include "human/voice/turn_signal.h"
#include <string.h>

typedef struct {
    const char *token;
    size_t len;
    hu_turn_signal_t signal;
} token_entry_t;

static const token_entry_t k_tokens[] = {
    {HU_TURN_TOKEN_YIELD, sizeof(HU_TURN_TOKEN_YIELD) - 1, HU_TURN_SIGNAL_YIELD},
    {HU_TURN_TOKEN_HOLD, sizeof(HU_TURN_TOKEN_HOLD) - 1, HU_TURN_SIGNAL_HOLD},
    {HU_TURN_TOKEN_BC, sizeof(HU_TURN_TOKEN_BC) - 1, HU_TURN_SIGNAL_BACKCHANNEL},
    {HU_TURN_TOKEN_CONTINUE, sizeof(HU_TURN_TOKEN_CONTINUE) - 1, HU_TURN_SIGNAL_CONTINUE},
};
#define K_TOKEN_COUNT (sizeof(k_tokens) / sizeof(k_tokens[0]))

static const token_entry_t *match_token_at(const char *text, size_t text_len, size_t pos) {
    if (text[pos] != '<')
        return NULL;
    for (size_t t = 0; t < K_TOKEN_COUNT; t++) {
        if (pos + k_tokens[t].len <= text_len &&
            memcmp(text + pos, k_tokens[t].token, k_tokens[t].len) == 0)
            return &k_tokens[t];
    }
    return NULL;
}

hu_error_t hu_turn_signal_extract(const char *text, size_t text_len, hu_turn_signal_result_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    out->signal = HU_TURN_SIGNAL_NONE;
    out->had_token = false;
    if (!text || text_len == 0)
        return HU_OK;

    for (size_t i = 0; i < text_len; i++) {
        const token_entry_t *m = match_token_at(text, text_len, i);
        if (m) {
            out->signal = m->signal;
            out->had_token = true;
            i += m->len - 1;
        }
    }
    return HU_OK;
}

size_t hu_turn_signal_strip(const char *text, size_t text_len, char *dst, size_t dst_cap) {
    if (!dst || dst_cap == 0)
        return 0;
    if (!text || text_len == 0) {
        dst[0] = '\0';
        return 0;
    }

    size_t out = 0;
    size_t i = 0;
    while (i < text_len && out < dst_cap - 1) {
        const token_entry_t *m = match_token_at(text, text_len, i);
        if (m) {
            i += m->len;
        } else {
            dst[out++] = text[i++];
        }
    }
    dst[out] = '\0';
    return out;
}
