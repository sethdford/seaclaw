#include "human/persona/humor.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ── Context analysis for humor appropriateness ──────────────────── */

static const char *serious_markers[] = {
    "died", "death", "funeral", "cancer", "suicide",
    "depressed", "heartbroken", "divorce", "fired", "laid off",
    "lost my", "grieving", "struggling", "scared", "terrified",
};
static const size_t serious_count = sizeof(serious_markers) / sizeof(serious_markers[0]);

static const char *light_markers[] = {
    "haha", "lol", "lmao", "funny", "joke", "hilarious",
    "😂", "🤣", "just kidding", "laughing",
};
static const size_t light_count = sizeof(light_markers) / sizeof(light_markers[0]);

static bool ci_contains(const char *text, size_t text_len, const char *pat) {
    size_t plen = strlen(pat);
    if (text_len < plen)
        return false;
    for (size_t i = 0; i + plen <= text_len; i++) {
        bool match = true;
        for (size_t j = 0; j < plen; j++) {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)pat[j])) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

hu_error_t hu_humor_fw_evaluate_context(const char *conversation, size_t conv_len,
                                     const hu_humor_context_t *ctx,
                                     hu_humor_evaluation_t *eval) {
    if (!conversation || !eval)
        return HU_ERR_INVALID_ARGUMENT;
    memset(eval, 0, sizeof(*eval));

    /* Check for serious context markers */
    size_t serious_hits = 0;
    for (size_t i = 0; i < serious_count; i++) {
        if (ci_contains(conversation, conv_len, serious_markers[i]))
            serious_hits++;
    }

    /* Check for light/humor-receptive markers */
    size_t light_hits = 0;
    for (size_t i = 0; i < light_count; i++) {
        if (ci_contains(conversation, conv_len, light_markers[i]))
            light_hits++;
    }

    if (ctx && ctx->in_serious_context)
        serious_hits += 3;

    /* Appropriateness: high when light, low when serious */
    if (serious_hits > 0 && light_hits == 0)
        eval->appropriateness = 0.1f;
    else if (serious_hits > 0)
        eval->appropriateness = 0.3f;
    else if (light_hits > 0)
        eval->appropriateness = 0.9f;
    else
        eval->appropriateness = 0.5f;

    /* Persona fit: check if preferred styles match */
    eval->persona_fit = 0.5f;
    if (ctx && ctx->preferred_count > 0)
        eval->persona_fit = 0.8f;

    /* Audience fit based on channel/contact availability */
    eval->audience_fit = 0.6f;
    if (ctx && ctx->contact_id && ctx->contact_id_len > 0)
        eval->audience_fit = 0.8f;

    eval->novelty = 0.6f;

    eval->composite = eval->appropriateness * 0.4f +
                      eval->persona_fit * 0.2f +
                      eval->audience_fit * 0.2f +
                      eval->novelty * 0.2f;

    eval->should_attempt = (eval->composite >= 0.5f && eval->appropriateness >= 0.3f);

    /* Suggest theory based on context */
    if (light_hits > 0)
        eval->suggested_theory = HU_HUMOR_INCONGRUITY;
    else
        eval->suggested_theory = HU_HUMOR_BENIGN_VIOLATION;

    return HU_OK;
}

/* ── Humor directive for prompt ──────────────────────────────────── */

static const char *theory_guidance[] = {
    [HU_HUMOR_INCONGRUITY] =
        "Use incongruity humor: set up a reasonable expectation, then subvert "
        "it with something unexpected but coherent. The surprise should be "
        "logical in hindsight.",
    [HU_HUMOR_BENIGN_VIOLATION] =
        "Use benign violation humor: reference something that feels slightly "
        "wrong or threatening, but is ultimately safe and playful.",
    [HU_HUMOR_SUPERIORITY] =
        "Use gentle self-deprecating humor: poke fun at yourself or shared "
        "human foibles. Never punch down.",
    [HU_HUMOR_RELIEF] =
        "Use relief humor: acknowledge tension or awkwardness, then release "
        "it with a light observation.",
};

hu_error_t hu_humor_fw_build_directive(hu_allocator_t *alloc,
                                    const hu_humor_evaluation_t *eval,
                                    const hu_humor_context_t *ctx,
                                    char **out, size_t *out_len) {
    if (!alloc || !eval || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    (void)ctx;

    if (!eval->should_attempt) {
        *out = NULL;
        *out_len = 0;
        return HU_OK;
    }

    const char *guidance = theory_guidance[eval->suggested_theory];
    size_t glen = strlen(guidance);
    size_t cap = glen + 64;
    char tmp[512];
    int n = snprintf(tmp, sizeof(tmp), "[HUMOR OPPORTUNITY (%.0f%% appropriate)] %s",
                     (double)(eval->appropriateness * 100.0f), guidance);
    if (n <= 0 || (size_t)n >= sizeof(tmp) || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;

    char *buf = (char *)alloc->alloc(alloc->ctx, (size_t)n + 1);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(buf, tmp, (size_t)n + 1);
    *out = buf;
    *out_len = (size_t)n;
    return HU_OK;
}

/* ── Score a response's humor quality ────────────────────────────── */

hu_error_t hu_humor_fw_score_response(const char *response, size_t response_len,
                                   const hu_humor_context_t *ctx,
                                   float *score) {
    if (!response || !score)
        return HU_ERR_INVALID_ARGUMENT;
    (void)ctx;

    float s = 0.0f;

    /* Positive: contains humor signals */
    if (ci_contains(response, response_len, "haha") ||
        ci_contains(response, response_len, "😂") ||
        ci_contains(response, response_len, "lol"))
        s += 0.3f;

    /* Positive: self-deprecation signals */
    if (ci_contains(response, response_len, "i'm terrible at") ||
        ci_contains(response, response_len, "my bad"))
        s += 0.2f;

    /* Moderate length suggests effort (not just a one-liner) */
    if (response_len > 50 && response_len < 300)
        s += 0.2f;

    /* Negative: too short for humor to land */
    if (response_len < 20)
        s -= 0.1f;

    if (s < 0.0f)
        s = 0.0f;
    if (s > 1.0f)
        s = 1.0f;

    *score = s;
    return HU_OK;
}
