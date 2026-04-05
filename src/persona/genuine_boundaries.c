#include "human/persona/genuine_boundaries.h"
#include "human/core/string.h"
#include <ctype.h>
#include <string.h>

#define HU_BOUNDARY_RELEVANCE_THRESHOLD 0.6f

void hu_genuine_boundary_set_init(hu_genuine_boundary_set_t *set) {
    if (!set)
        return;
    memset(set, 0, sizeof(*set));
}

void hu_genuine_boundary_set_deinit(hu_allocator_t *alloc, hu_genuine_boundary_set_t *set) {
    if (!alloc || !set)
        return;
    for (size_t i = 0; i < set->count; i++) {
        hu_str_free(alloc, set->boundaries[i].domain);
        hu_str_free(alloc, set->boundaries[i].stance);
        hu_str_free(alloc, set->boundaries[i].origin);
        set->boundaries[i].domain = NULL;
        set->boundaries[i].stance = NULL;
        set->boundaries[i].origin = NULL;
    }
    set->count = 0;
}

hu_error_t hu_genuine_boundary_add(hu_allocator_t *alloc, hu_genuine_boundary_set_t *set,
                                   const char *domain, const char *stance, const char *origin,
                                   float conviction, uint64_t formed_at) {
    if (!alloc || !set)
        return HU_ERR_INVALID_ARGUMENT;
    if (!domain || !stance)
        return HU_ERR_INVALID_ARGUMENT;
    if (set->count >= HU_GENUINE_BOUNDARY_MAX)
        return HU_ERR_LIMIT_REACHED;

    const char *orig = origin ? origin : "";
    size_t olen = origin ? strlen(origin) : 0;

    char *d = hu_strndup(alloc, domain, strlen(domain));
    char *s = hu_strndup(alloc, stance, strlen(stance));
    char *o = hu_strndup(alloc, orig, olen);
    if (!d || !s || !o) {
        hu_str_free(alloc, d);
        hu_str_free(alloc, s);
        hu_str_free(alloc, o);
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t k = set->count++;
    set->boundaries[k].domain = d;
    set->boundaries[k].stance = s;
    set->boundaries[k].origin = o;
    set->boundaries[k].conviction = conviction;
    set->boundaries[k].formed_at = formed_at;
    set->boundaries[k].times_relevant = 0;
    set->boundaries[k].times_asserted = 0;
    return HU_OK;
}

static bool genuine_domain_in_message(const char *msg, size_t msg_len, const char *domain) {
    if (!msg || !domain)
        return false;
    size_t dlen = strlen(domain);
    if (dlen == 0 || dlen > msg_len)
        return false;
    for (size_t i = 0; i + dlen <= msg_len; i++) {
        size_t j;
        for (j = 0; j < dlen; j++) {
            if (tolower((unsigned char)msg[i + j]) != tolower((unsigned char)domain[j]))
                break;
        }
        if (j == dlen)
            return true;
    }
    return false;
}

hu_error_t hu_genuine_boundary_check_relevance(hu_genuine_boundary_set_t *set,
                                               const char *message, size_t message_len,
                                               const hu_genuine_boundary_t **matched) {
    if (!matched)
        return HU_ERR_INVALID_ARGUMENT;
    *matched = NULL;
    if (!set || !message)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < set->count; i++) {
        hu_genuine_boundary_t *b = &set->boundaries[i];
        if (b->conviction <= HU_BOUNDARY_RELEVANCE_THRESHOLD)
            continue;
        if (!b->domain)
            continue;
        if (!genuine_domain_in_message(message, message_len, b->domain))
            continue;
        b->times_relevant++;
        *matched = b;
        return HU_OK;
    }
    return HU_OK;
}

hu_error_t hu_genuine_boundary_build_context(hu_allocator_t *alloc,
                                             const hu_genuine_boundary_t *boundary,
                                             uint32_t relationship_stage,
                                             char **out, size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    if (!boundary || !boundary->domain || !boundary->stance)
        return HU_OK;

    char *line;
    if (relationship_stage >= 2U && boundary->origin && boundary->origin[0] != '\0') {
        line = hu_sprintf(
            alloc,
            "[GENUINE BOUNDARY: If the conversation touches on %s, you have a real stance: \"%s\". "
            "Say it as a person with values, not as a policy. The reason: %s.]",
            boundary->domain, boundary->stance, boundary->origin);
    } else {
        line = hu_sprintf(
            alloc,
            "[GENUINE BOUNDARY: If the conversation touches on %s, you have a real stance: \"%s\". "
            "Say it as a person with values, not as a policy.]",
            boundary->domain, boundary->stance);
    }

    if (!line)
        return HU_ERR_OUT_OF_MEMORY;

    *out = line;
    *out_len = strlen(line);
    return HU_OK;
}
