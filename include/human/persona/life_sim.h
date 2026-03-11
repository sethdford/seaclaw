#ifndef HU_PERSONA_LIFE_SIM_H
#define HU_PERSONA_LIFE_SIM_H

#include "human/core/allocator.h"
#include <stddef.h>
#include <stdint.h>

/* Routine block — time, activity, availability, mood modifier.
 * Defined here; may be moved to persona.h by another agent. */
typedef struct hu_routine_block {
    char *time;           /* "05:30" */
    char *activity;      /* "wake_up", "gym", "work_meetings" */
    char *availability;  /* "brief", "unavailable", "slow", "available" */
    char *mood_modifier; /* "groggy", "energetic_after", "focused" */
} hu_routine_block_t;

typedef struct hu_daily_routine {
    hu_routine_block_t *weekday;
    size_t weekday_count;
    hu_routine_block_t *weekend;
    size_t weekend_count;
    float routine_variance; /* 0.15 = ±15% time jitter */
} hu_daily_routine_t;

typedef struct hu_life_sim_state {
    const char *activity;
    const char *availability; /* "available" | "brief" | "slow" | "unavailable" */
    const char *mood_modifier;
    float availability_factor; /* 0.5=available, 1.0=brief, 2.0=slow, 5.0=unavailable */
} hu_life_sim_state_t;

/* Get current simulated state based on time. Uses routine_variance for ±15% jitter. */
hu_life_sim_state_t hu_life_sim_get_current(const hu_daily_routine_t *routine, int64_t now_ts,
                                            int day_of_week, uint32_t seed);

/* Build context string for prompt. "[LIFE CONTEXT: You just finished dinner...]" */
char *hu_life_sim_build_context(hu_allocator_t *alloc, const hu_life_sim_state_t *state,
                                 size_t *out_len);

#endif /* HU_PERSONA_LIFE_SIM_H */
