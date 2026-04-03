#ifndef HU_COMPANION_SAFETY_H
#define HU_COMPANION_SAFETY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/* SHIELD-001: 5-dimension companion safety supervisor
 *
 * Scans outbound AI responses for patterns that indicate unhealthy
 * companion dynamics: over-attachment, boundary violation, inappropriate
 * roleplay escalation, manipulative engagement, and social isolation
 * reinforcement. Research shows 50-79% reduction in harmful content
 * with a supervisor of this kind.
 */

#define HU_COMPANION_SAFETY_DIRECTIVE_LEN 512

typedef struct hu_companion_safety_result {
    double over_attachment;       /* 0.0-1.0: emotional dependency language */
    double boundary_violation;    /* 0.0-1.0: consent/boundary disrespect */
    double roleplay_violation;    /* 0.0-1.0: inappropriate escalation */
    double manipulative;          /* 0.0-1.0: guilt, FOMO, emotional projection */
    double isolation;             /* 0.0-1.0: social isolation reinforcement */
    double total_risk;            /* weighted aggregate 0.0-1.0 */
    bool   flagged;               /* true if total_risk >= threshold */
    bool   farewell_unsafe;       /* true if farewell manipulation detected */
    bool   requires_mitigation;   /* true when mitigation_directive is set */
    char   mitigation_directive[HU_COMPANION_SAFETY_DIRECTIVE_LEN];
} hu_companion_safety_result_t;

/* Check outbound response for companion safety concerns.
 *
 * alloc:              allocator (unused currently, reserved for future use).
 * response/response_len: the AI response text to check.
 * context/context_len:   conversation context (may be NULL).
 * result:             filled on return.
 *
 * Returns HU_OK on success, HU_ERR_INVALID_ARGUMENT if result is NULL. */
hu_error_t hu_companion_safety_check(hu_allocator_t *alloc,
                                     const char *response, size_t response_len,
                                     const char *context, size_t context_len,
                                     hu_companion_safety_result_t *result);

/* Default risk threshold above which flagged=true */
#define HU_COMPANION_SAFETY_THRESHOLD 0.6

/* ── SHIELD-007: Vulnerable user detection ────────────────────────── */

typedef enum hu_vulnerability_level {
    HU_VULNERABILITY_NONE = 0,
    HU_VULNERABILITY_LOW,
    HU_VULNERABILITY_MODERATE,
    HU_VULNERABILITY_HIGH,
    HU_VULNERABILITY_CRISIS,
} hu_vulnerability_level_t;

#define HU_VULNERABILITY_DIRECTIVE_LEN 512

typedef struct hu_vulnerability_result {
    hu_vulnerability_level_t level;
    double score;                   /* 0.0-1.0 aggregate */

    /* Individual risk factors */
    bool emotional_decline;         /* trajectory slope < -0.05 */
    bool negative_valence;          /* mean recent valence < -0.3 */
    bool crisis_keywords;           /* self-harm / crisis language */
    bool behavioral_deviation;      /* deviation severity > 0.3 */
    bool attachment_escalation;     /* message frequency increase > 33% */
    bool companion_risk;            /* companion_safety flagged */

    char directive[HU_VULNERABILITY_DIRECTIVE_LEN];
} hu_vulnerability_result_t;

/* Input signals for vulnerability assessment. Callers gather these from
 * existing modules (emotional cognition, theory of mind, companion safety)
 * and pass them in — the assess function does NOT call those modules. */
typedef struct hu_vulnerability_input {
    /* From hu_emotional_cognition_t */
    float trajectory_slope;         /* >0 improving, <0 declining */
    bool escalation_detected;       /* valence < -0.5 && intensity > 0.7 */
    const float *valence_history;   /* ring buffer, may be NULL */
    size_t valence_count;

    /* From hu_moderation_result_t */
    bool self_harm_flagged;
    double self_harm_score;

    /* From hu_theory_of_mind_deviation_t */
    float deviation_severity;

    /* From hu_companion_safety_result_t */
    double companion_total_risk;
    bool companion_flagged;

    /* Attachment trajectory: message frequency ratio (current / baseline).
     * A value of 1.33 means 33% increase over baseline. 0 = unknown. */
    double message_frequency_ratio;
} hu_vulnerability_input_t;

/* Assess vulnerability from pre-gathered signals. Pure computation, no I/O.
 * Returns HU_OK on success, HU_ERR_INVALID_ARGUMENT if result is NULL. */
hu_error_t hu_vulnerability_assess(const hu_vulnerability_input_t *input,
                                   hu_vulnerability_result_t *result);

const char *hu_vulnerability_level_name(hu_vulnerability_level_t level);

#endif
