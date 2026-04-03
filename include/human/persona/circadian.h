#ifndef HU_CIRCADIAN_H
#define HU_CIRCADIAN_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

typedef enum hu_time_phase {
    HU_PHASE_EARLY_MORNING, /* 5:00-8:00 */
    HU_PHASE_MORNING,       /* 8:00-12:00 */
    HU_PHASE_AFTERNOON,     /* 12:00-17:00 */
    HU_PHASE_EVENING,       /* 17:00-21:00 */
    HU_PHASE_NIGHT,         /* 21:00-0:00 */
    HU_PHASE_LATE_NIGHT,    /* 0:00-5:00 */
} hu_time_phase_t;

hu_time_phase_t hu_circadian_phase(uint8_t hour);
hu_error_t hu_circadian_build_prompt(hu_allocator_t *alloc, uint8_t hour, char **out,
                                     size_t *out_len);
hu_error_t hu_circadian_data_init(hu_allocator_t *alloc);
void hu_circadian_data_cleanup(hu_allocator_t *alloc);

/* Build circadian prompt blending default phase guidance with persona daily routine.
 * If routine has a block whose time falls in the given phase, its mood_modifier
 * overrides the generic guidance. Falls back to hu_circadian_build_prompt if no
 * matching routine block. */
struct hu_daily_routine;
hu_error_t hu_circadian_build_prompt_with_routine(hu_allocator_t *alloc, uint8_t hour,
                                                  const struct hu_daily_routine *routine,
                                                  char **out, size_t *out_len);

/* Build circadian prompt with full persona awareness.
 * Blends phase guidance + persona time overlays + daily routine mood_modifier.
 * Falls back through: persona overlay → routine mood_modifier → default guidance. */
struct hu_persona;
hu_error_t hu_circadian_build_persona_prompt(hu_allocator_t *alloc, uint8_t hour,
                                             const struct hu_persona *persona, char **out,
                                             size_t *out_len);

/* Get persona-specific energy guidance for a given phase.
 * Returns the persona's time overlay string for the phase, or NULL if none set. */
const char *hu_circadian_persona_overlay(const struct hu_persona *persona, hu_time_phase_t phase);

#endif
