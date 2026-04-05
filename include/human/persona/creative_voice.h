#ifndef HU_PERSONA_CREATIVE_VOICE_H
#define HU_PERSONA_CREATIVE_VOICE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

#define HU_CREATIVE_MAX_DOMAINS 6
#define HU_CREATIVE_MAX_ANCHORS 8

typedef struct hu_creative_voice {
    char *metaphor_domains[HU_CREATIVE_MAX_DOMAINS];
    size_t domain_count;
    char *worldview_anchors[HU_CREATIVE_MAX_ANCHORS];
    size_t anchor_count;
    float expressiveness;
    char *voice_directive;
} hu_creative_voice_t;

void hu_creative_voice_init(hu_creative_voice_t *voice);
void hu_creative_voice_deinit(hu_allocator_t *alloc, hu_creative_voice_t *voice);

hu_error_t hu_creative_voice_add_domain(hu_allocator_t *alloc, hu_creative_voice_t *voice,
                                        const char *domain, size_t len);

hu_error_t hu_creative_voice_add_anchor(hu_allocator_t *alloc, hu_creative_voice_t *voice,
                                        const char *anchor, size_t len);

hu_error_t hu_creative_voice_build_context(hu_allocator_t *alloc, const hu_creative_voice_t *voice,
                                           char **out, size_t *out_len);

#endif
