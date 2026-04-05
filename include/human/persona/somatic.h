#ifndef HU_PERSONA_SOMATIC_H
#define HU_PERSONA_SOMATIC_H

#include "human/context/authentic.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdint.h>

typedef struct hu_somatic_state {
    float energy;
    float social_battery;
    float focus;
    float arousal;
    hu_physical_state_t physical;
    uint64_t last_interaction_ts;
    uint64_t last_recharge_ts;
    float conversation_load_accumulated;
} hu_somatic_state_t;

void hu_somatic_init(hu_somatic_state_t *state);

void hu_somatic_update(hu_somatic_state_t *state, uint64_t now_ts, float emotional_intensity,
                       uint32_t topic_switches, hu_physical_state_t scheduled_physical);

hu_error_t hu_somatic_build_context(hu_allocator_t *alloc, const hu_somatic_state_t *state, char **out,
                                    size_t *out_len);

const char *hu_somatic_energy_label(float energy);
const char *hu_somatic_battery_label(float battery);

#endif
