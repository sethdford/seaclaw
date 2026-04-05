#ifndef HU_COGNITION_RUPTURE_REPAIR_H
#define HU_COGNITION_RUPTURE_REPAIR_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HU_RUPTURE_NONE,
    HU_RUPTURE_DETECTED,
    HU_RUPTURE_ACKNOWLEDGED,
    HU_RUPTURE_REPAIRING,
    HU_RUPTURE_REPAIRED,
    HU_RUPTURE_UNRESOLVED
} hu_rupture_stage_t;

typedef struct hu_rupture_state {
    hu_rupture_stage_t stage;
    char *trigger_summary;
    char *last_assistant_msg;
    float severity;
    uint32_t turns_since;
    char *repair_directive;
} hu_rupture_state_t;

void hu_rupture_init(hu_rupture_state_t *state);
void hu_rupture_deinit(hu_allocator_t *alloc, hu_rupture_state_t *state);

typedef struct hu_rupture_signals {
    float tone_delta;
    float length_delta;
    float energy_drop;
    bool explicit_correction;
    bool user_withdrawal;
} hu_rupture_signals_t;

hu_error_t hu_rupture_evaluate(hu_allocator_t *alloc, hu_rupture_state_t *state,
                               const hu_rupture_signals_t *signals,
                               const char *last_assistant_msg, size_t msg_len);

hu_error_t hu_rupture_advance(hu_allocator_t *alloc, hu_rupture_state_t *state,
                              bool user_reengaged);

hu_error_t hu_rupture_build_context(hu_allocator_t *alloc, const hu_rupture_state_t *state,
                                    char **out, size_t *out_len);

const char *hu_rupture_stage_name(hu_rupture_stage_t stage);

#endif
