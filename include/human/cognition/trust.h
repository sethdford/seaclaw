#ifndef HU_COGNITION_TRUST_CAL_H
#define HU_COGNITION_TRUST_CAL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Trust calibration — TCMM-inspired 5-dimension trust maturity model.
 *
 * Tracks bidirectional trust between the AI and each contact:
 * 1. Competence — does the AI answer accurately?
 * 2. Benevolence — does the AI act in the user's interest?
 * 3. Integrity — is the AI consistent and honest?
 * 4. Predictability — does the AI behave consistently?
 * 5. Transparency — does the AI explain its reasoning?
 *
 * Maps internal confidence to calibrated uncertainty language.
 */

typedef struct hu_tcal_dimensions {
    float competence;       /* 0.0–1.0 */
    float benevolence;      /* 0.0–1.0 */
    float integrity;        /* 0.0–1.0 */
    float predictability;   /* 0.0–1.0 */
    float transparency;     /* 0.0–1.0 */
} hu_tcal_dimensions_t;

typedef enum hu_tcal_level {
    HU_TCAL_UNKNOWN = 0,
    HU_TCAL_CAUTIOUS,
    HU_TCAL_DEVELOPING,
    HU_TCAL_ESTABLISHED,
    HU_TCAL_DEEP,
} hu_tcal_level_t;

typedef struct hu_tcal_state {
    hu_tcal_dimensions_t dimensions;
    hu_tcal_level_t level;
    float composite;            /* weighted average */
    size_t interaction_count;
    bool erosion_detected;      /* true if trust recently declined */
    float erosion_rate;         /* rate of recent decline */
} hu_tcal_state_t;

/* Initialize trust state for a new contact. */
void hu_tcal_init(hu_tcal_state_t *state);

/* Update trust based on an interaction outcome. */
void hu_tcal_update(hu_tcal_state_t *state,
                    float competence_signal,
                    float benevolence_signal,
                    float integrity_signal);

/* Compute trust level from dimensions. */
hu_tcal_level_t hu_tcal_compute_level(const hu_tcal_dimensions_t *dims);

/* Map internal confidence (0.0–1.0) to calibrated language marker.
 * Returns a static string like "I'm fairly confident", "I think", etc. */
const char *hu_tcal_confidence_language(float confidence,
                                        hu_tcal_level_t contact_trust);

/* Build trust context for prompt injection.
 * Includes trust level, interaction history hints, calibration guidance.
 * Caller frees *out. */
hu_error_t hu_tcal_build_context(hu_allocator_t *alloc,
                                 const hu_tcal_state_t *state,
                                 char **out, size_t *out_len);

/* Detect trust erosion: returns true if trust dropped significantly. */
bool hu_tcal_check_erosion(const hu_tcal_state_t *state);

#endif
