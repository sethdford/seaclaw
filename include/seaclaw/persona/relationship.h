#ifndef SC_RELATIONSHIP_H
#define SC_RELATIONSHIP_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stddef.h>
#include <stdint.h>

typedef enum sc_relationship_stage {
    SC_REL_NEW,      /* 0-5 sessions */
    SC_REL_FAMILIAR, /* 5-20 sessions */
    SC_REL_TRUSTED,  /* 20-50 sessions */
    SC_REL_DEEP,     /* 50+ sessions */
} sc_relationship_stage_t;

typedef struct sc_relationship_state {
    sc_relationship_stage_t stage;
    uint32_t session_count;
    uint32_t total_turns;
} sc_relationship_state_t;

void sc_relationship_update(sc_relationship_state_t *state, uint32_t turn_count);
sc_error_t sc_relationship_build_prompt(sc_allocator_t *alloc,
                                         const sc_relationship_state_t *state,
                                         char **out, size_t *out_len);

#endif
