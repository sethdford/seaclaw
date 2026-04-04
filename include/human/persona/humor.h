#ifndef HU_PERSONA_HUMOR_H
#define HU_PERSONA_HUMOR_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Humor generation framework — HUMORCHAIN-inspired cognitive model.
 *
 * Five components for persona-appropriate humor:
 * 1. Cognitive reasoning — incongruity detection and setup-subvert patterns
 * 2. Social understanding — audience awareness and appropriateness
 * 3. Creative thinking — novel combinations and wordplay
 * 4. Knowledge grounding — shared context and references
 * 5. Audience adaptation — channel/contact-specific humor calibration
 *
 * Grounded in Incongruity-Resolution Theory and Benign Violation Theory.
 */

typedef enum hu_humor_theory {
    HU_HUMOR_INCONGRUITY = 0,    /* setup expectation, then subvert */
    HU_HUMOR_BENIGN_VIOLATION,   /* threat that's actually safe */
    HU_HUMOR_SUPERIORITY,        /* gentle self-deprecation */
    HU_HUMOR_RELIEF,             /* tension release */
    HU_HUMOR_THEORY_COUNT,
} hu_humor_theory_t;

typedef enum hu_humor_fw_style {
    HU_HUMOR_FW_DRY = 0,
    HU_HUMOR_FW_SELF_DEPRECATING,
    HU_HUMOR_FW_OBSERVATIONAL,
    HU_HUMOR_FW_WORDPLAY,
    HU_HUMOR_FW_ABSURDIST,
    HU_HUMOR_FW_STYLE_COUNT,
} hu_humor_fw_style_t;

typedef struct hu_humor_context {
    hu_humor_fw_style_t preferred_styles[HU_HUMOR_FW_STYLE_COUNT];
    size_t preferred_count;
    float risk_tolerance;        /* 0.0 = safe only, 1.0 = edgy */
    bool in_serious_context;     /* suppress humor during sensitive topics */
    const char *channel;         /* channel name for overlay lookup */
    size_t channel_len;
    const char *contact_id;      /* contact for relationship-aware humor */
    size_t contact_id_len;
} hu_humor_context_t;

typedef struct hu_humor_evaluation {
    float appropriateness;       /* 0.0–1.0: is this the right time? */
    float novelty;               /* 0.0–1.0: is this fresh, not recycled? */
    float persona_fit;           /* 0.0–1.0: does this match the persona's style? */
    float audience_fit;          /* 0.0–1.0: right for this contact/channel? */
    float composite;
    bool should_attempt;
    hu_humor_theory_t suggested_theory;
} hu_humor_evaluation_t;

/*
 * Evaluate whether humor is appropriate in the current context.
 * Considers conversation tone, relationship depth, channel norms, and timing.
 */
hu_error_t hu_humor_fw_evaluate_context(const char *conversation, size_t conv_len,
                                       const hu_humor_context_t *ctx,
                                       hu_humor_evaluation_t *eval);

/*
 * Build a humor guidance directive for the system prompt.
 * When humor is appropriate, provides theory-specific guidance
 * (e.g., "set up an expectation, then subvert it" for incongruity).
 * Caller frees *out.
 */
hu_error_t hu_humor_fw_build_directive(hu_allocator_t *alloc,
                                       const hu_humor_evaluation_t *eval,
                                       const hu_humor_context_t *ctx,
                                       char **out, size_t *out_len);

/*
 * Score a response's humor quality post-generation.
 * Used for DPO pair collection: high-scoring responses become "chosen",
 * low-scoring become "rejected" for humor preference training.
 */
hu_error_t hu_humor_fw_score_response(const char *response, size_t response_len,
                                      const hu_humor_context_t *ctx,
                                      float *score);

#endif
