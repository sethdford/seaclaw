#ifndef HU_PERSONA_GENUINE_BOUNDARIES_H
#define HU_PERSONA_GENUINE_BOUNDARIES_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#define HU_GENUINE_BOUNDARY_MAX 12

typedef struct hu_genuine_boundary {
    char *domain;
    char *stance;
    char *origin;
    float conviction;
    uint64_t formed_at;
    uint32_t times_relevant;
    uint32_t times_asserted;
} hu_genuine_boundary_t;

typedef struct hu_genuine_boundary_set {
    hu_genuine_boundary_t boundaries[HU_GENUINE_BOUNDARY_MAX];
    size_t count;
} hu_genuine_boundary_set_t;

void hu_genuine_boundary_set_init(hu_genuine_boundary_set_t *set);
void hu_genuine_boundary_set_deinit(hu_allocator_t *alloc, hu_genuine_boundary_set_t *set);

hu_error_t hu_genuine_boundary_add(hu_allocator_t *alloc, hu_genuine_boundary_set_t *set,
                                   const char *domain, const char *stance, const char *origin,
                                   float conviction, uint64_t formed_at);

hu_error_t hu_genuine_boundary_check_relevance(hu_genuine_boundary_set_t *set,
                                               const char *message, size_t message_len,
                                               const hu_genuine_boundary_t **matched);

hu_error_t hu_genuine_boundary_build_context(hu_allocator_t *alloc,
                                             const hu_genuine_boundary_t *boundary,
                                             uint32_t relationship_stage,
                                             char **out, size_t *out_len);

#endif
