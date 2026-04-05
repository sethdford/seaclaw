#ifndef HU_AGENT_GROWTH_NARRATIVE_H
#define HU_AGENT_GROWTH_NARRATIVE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HU_GROWTH_MAX_OBSERVATIONS 16
#define HU_GROWTH_MAX_MILESTONES 16

typedef struct hu_growth_observation {
    char *contact_id;
    char *observation;
    char *evidence;
    float confidence;
    uint64_t observed_at;
    bool surfaced;
} hu_growth_observation_t;

typedef struct hu_relational_milestone {
    char *contact_id;
    char *description;
    uint64_t timestamp;
    float significance;
} hu_relational_milestone_t;

typedef struct hu_growth_narrative {
    hu_growth_observation_t observations[HU_GROWTH_MAX_OBSERVATIONS];
    size_t observation_count;
    hu_relational_milestone_t milestones[HU_GROWTH_MAX_MILESTONES];
    size_t milestone_count;
    uint32_t conversations_since_last_surface;
    uint32_t surface_cooldown;
} hu_growth_narrative_t;

void hu_growth_narrative_init(hu_growth_narrative_t *gn);
void hu_growth_narrative_deinit(hu_allocator_t *alloc, hu_growth_narrative_t *gn);

hu_error_t hu_growth_narrative_add_observation(hu_allocator_t *alloc, hu_growth_narrative_t *gn,
                                               const char *contact_id, const char *observation,
                                               const char *evidence, float confidence,
                                               uint64_t observed_at);

hu_error_t hu_growth_narrative_add_milestone(hu_allocator_t *alloc, hu_growth_narrative_t *gn,
                                             const char *contact_id, const char *description,
                                             uint64_t timestamp, float significance);

hu_error_t hu_growth_narrative_build_context(hu_allocator_t *alloc, hu_growth_narrative_t *gn,
                                             const char *contact_id, char **out, size_t *out_len);

#endif
