#include "human/persona/creative_voice.h"
#include "human/core/string.h"
#include <string.h>

#define HU_CREATIVE_HIGH_EXPRESSIVENESS 0.7f
#define HU_CREATIVE_LOW_EXPRESSIVENESS  0.3f

void hu_creative_voice_init(hu_creative_voice_t *voice) {
    if (!voice)
        return;
    memset(voice, 0, sizeof(*voice));
    voice->expressiveness = 0.5f;
}

void hu_creative_voice_deinit(hu_allocator_t *alloc, hu_creative_voice_t *voice) {
    if (!alloc || !voice)
        return;
    for (size_t i = 0; i < voice->domain_count; i++) {
        hu_str_free(alloc, voice->metaphor_domains[i]);
        voice->metaphor_domains[i] = NULL;
    }
    voice->domain_count = 0;
    for (size_t i = 0; i < voice->anchor_count; i++) {
        hu_str_free(alloc, voice->worldview_anchors[i]);
        voice->worldview_anchors[i] = NULL;
    }
    voice->anchor_count = 0;
    hu_str_free(alloc, voice->voice_directive);
    voice->voice_directive = NULL;
}

hu_error_t hu_creative_voice_add_domain(hu_allocator_t *alloc, hu_creative_voice_t *voice,
                                        const char *domain, size_t len) {
    if (!alloc || !voice)
        return HU_ERR_INVALID_ARGUMENT;
    if (!domain)
        return HU_ERR_INVALID_ARGUMENT;
    if (voice->domain_count >= HU_CREATIVE_MAX_DOMAINS)
        return HU_ERR_LIMIT_REACHED;

    char *copy = hu_strndup(alloc, domain, len);
    if (!copy)
        return HU_ERR_OUT_OF_MEMORY;

    voice->metaphor_domains[voice->domain_count++] = copy;
    return HU_OK;
}

hu_error_t hu_creative_voice_add_anchor(hu_allocator_t *alloc, hu_creative_voice_t *voice,
                                        const char *anchor, size_t len) {
    if (!alloc || !voice)
        return HU_ERR_INVALID_ARGUMENT;
    if (!anchor)
        return HU_ERR_INVALID_ARGUMENT;
    if (voice->anchor_count >= HU_CREATIVE_MAX_ANCHORS)
        return HU_ERR_LIMIT_REACHED;

    char *copy = hu_strndup(alloc, anchor, len);
    if (!copy)
        return HU_ERR_OUT_OF_MEMORY;

    voice->worldview_anchors[voice->anchor_count++] = copy;
    return HU_OK;
}

static size_t creative_joined_len(char *const *parts, size_t count, size_t sep_len) {
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (parts[i])
            total += strlen(parts[i]);
        if (i + 1 < count)
            total += sep_len;
    }
    return total;
}

static void creative_join_into(char *dest, char *const *parts, size_t count, const char *sep,
                               size_t sep_len) {
    char *p = dest;
    for (size_t i = 0; i < count; i++) {
        if (parts[i]) {
            size_t plen = strlen(parts[i]);
            memcpy(p, parts[i], plen);
            p += plen;
        }
        if (i + 1 < count) {
            memcpy(p, sep, sep_len);
            p += sep_len;
        }
    }
    *p = '\0';
}

hu_error_t hu_creative_voice_build_context(hu_allocator_t *alloc, const hu_creative_voice_t *voice,
                                           char **out, size_t *out_len) {
    if (!alloc || !voice || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    if (voice->domain_count == 0 && voice->anchor_count == 0)
        return HU_OK;

    static const char open[] = "[CREATIVE VOICE: Your natural metaphor palette draws from ";
    static const char open_anchors_only[] = "[CREATIVE VOICE: ";
    static const char after_domains[] = ". ";
    static const char lens_open[] = "Your worldview lens: ";
    static const char after_anchors[] = ". ";
    static const char express_core[] =
        "Express that perspective naturally — don't narrate it.";
    static const char lean[] = " Lean into metaphor when explaining abstractions.";
    static const char plain[] =
        " Keep expression plain and direct — metaphor only when it genuinely clarifies.";
    static const char close[] = "]";

    size_t dom_joined = creative_joined_len(voice->metaphor_domains, voice->domain_count, 2);
    size_t anc_joined = creative_joined_len(voice->worldview_anchors, voice->anchor_count, 2);

    size_t n = 0;
    if (voice->domain_count > 0) {
        n += sizeof(open) - 1U + dom_joined + sizeof(after_domains) - 1U;
    } else {
        n += sizeof(open_anchors_only) - 1U;
    }
    if (voice->anchor_count > 0) {
        n += sizeof(lens_open) - 1U + anc_joined + sizeof(after_anchors) - 1U;
    }
    n += sizeof(express_core) - 1U;
    if (voice->expressiveness > HU_CREATIVE_HIGH_EXPRESSIVENESS)
        n += sizeof(lean) - 1U;
    else if (voice->expressiveness < HU_CREATIVE_LOW_EXPRESSIVENESS)
        n += sizeof(plain) - 1U;
    n += sizeof(close) - 1U;

    char *buf = (char *)alloc->alloc(alloc->ctx, n + 1U);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    char *w = buf;
    if (voice->domain_count > 0) {
        memcpy(w, open, sizeof(open) - 1U);
        w += sizeof(open) - 1U;
        creative_join_into(w, voice->metaphor_domains, voice->domain_count, ", ", 2);
        w += dom_joined;
        memcpy(w, after_domains, sizeof(after_domains) - 1U);
        w += sizeof(after_domains) - 1U;
    } else {
        memcpy(w, open_anchors_only, sizeof(open_anchors_only) - 1U);
        w += sizeof(open_anchors_only) - 1U;
    }

    if (voice->anchor_count > 0) {
        memcpy(w, lens_open, sizeof(lens_open) - 1U);
        w += sizeof(lens_open) - 1U;
        creative_join_into(w, voice->worldview_anchors, voice->anchor_count, "; ", 2);
        w += anc_joined;
        memcpy(w, after_anchors, sizeof(after_anchors) - 1U);
        w += sizeof(after_anchors) - 1U;
    }

    memcpy(w, express_core, sizeof(express_core) - 1U);
    w += sizeof(express_core) - 1U;

    if (voice->expressiveness > HU_CREATIVE_HIGH_EXPRESSIVENESS) {
        memcpy(w, lean, sizeof(lean) - 1U);
        w += sizeof(lean) - 1U;
    } else if (voice->expressiveness < HU_CREATIVE_LOW_EXPRESSIVENESS) {
        memcpy(w, plain, sizeof(plain) - 1U);
        w += sizeof(plain) - 1U;
    }

    memcpy(w, close, sizeof(close) - 1U);
    w += sizeof(close) - 1U;
    *w = '\0';

    *out = buf;
    *out_len = n;
    return HU_OK;
}
