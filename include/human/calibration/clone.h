#ifndef HU_CALIBRATION_CLONE_H
#define HU_CALIBRATION_CLONE_H

#include "human/core/allocator.h"
#include "human/core/error.h"

#include <stddef.h>
#include <stdint.h>

/**
 * Behavioral cloning statistics derived from iMessage chat.db (Apple `message.date`
 * is nanoseconds since 2001-01-01). Used to refine digital-twin parameters.
 *
 * `hu_behavioral_clone_delta(..., since_timestamp, ...)` uses the same Apple
 * nanosecond clock as `message.date`; pass 0 to include all rows (subject to LIMIT).
 */
typedef struct hu_clone_patterns {
    /* Conversation initiation */
    char topic_starters[16][256];
    size_t topic_starter_count;
    double initiation_frequency_per_day;

    /* Conversation endings */
    char sign_offs[8][128];
    size_t sign_off_count;

    /* Response depth curve */
    double response_length_by_depth[10]; /* multiplier at conversation turn 1,2,...10 */

    /* Double-text */
    double double_text_probability;
    int double_text_median_delay_sec;

    /* Read-to-response delay */
    int read_to_response_median_sec;
    int read_to_response_p95_sec;

    /* Style stats (outbound messages) — used for persona behavioral_calibration */
    double avg_message_length; /* 0 = no samples */
    double emoji_frequency;    /* 0.0–1.0, fraction of outbound texts containing emoji */
} hu_clone_patterns_t;

hu_error_t hu_behavioral_clone_extract(hu_allocator_t *alloc, const char *db_path,
                                       const char *contact_filter, hu_clone_patterns_t *out_clone);

hu_error_t hu_behavioral_clone_update_persona(hu_allocator_t *alloc,
                                                const hu_clone_patterns_t *clone_data,
                                                const char *persona_path);

hu_error_t hu_behavioral_clone_delta(hu_allocator_t *alloc, const char *db_path,
                                     int64_t since_timestamp, hu_clone_patterns_t *out_delta);

#endif /* HU_CALIBRATION_CLONE_H */
