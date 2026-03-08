#ifndef SC_CIRCADIAN_H
#define SC_CIRCADIAN_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stddef.h>
#include <stdint.h>

typedef enum sc_time_phase {
    SC_PHASE_EARLY_MORNING, /* 5:00-8:00 */
    SC_PHASE_MORNING,       /* 8:00-12:00 */
    SC_PHASE_AFTERNOON,     /* 12:00-17:00 */
    SC_PHASE_EVENING,       /* 17:00-21:00 */
    SC_PHASE_NIGHT,         /* 21:00-0:00 */
    SC_PHASE_LATE_NIGHT,    /* 0:00-5:00 */
} sc_time_phase_t;

sc_time_phase_t sc_circadian_phase(uint8_t hour);
sc_error_t sc_circadian_build_prompt(sc_allocator_t *alloc, uint8_t hour,
                                      char **out, size_t *out_len);

#endif
