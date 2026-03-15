#ifndef HU_MEMORY_ENTROPY_GATE_H
#define HU_MEMORY_ENTROPY_GATE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct hu_entropy_gate_config {
    double threshold;       /* minimum entropy to pass (default 0.3) */
    size_t context_budget; /* max total tokens in context (default 4096) */
    bool adaptive;         /* adjust threshold based on budget (default true) */
} hu_entropy_gate_config_t;

typedef struct hu_memory_chunk {
    const char *text;
    size_t text_len;
    double entropy; /* computed entropy (0.0 = no info, 1.0 = max info) */
    bool passed;   /* true if passes gate */
} hu_memory_chunk_t;

hu_entropy_gate_config_t hu_entropy_gate_config_default(void);

hu_error_t hu_entropy_compute(const char *text, size_t text_len, double *out_entropy);

hu_error_t hu_entropy_gate_filter(const hu_entropy_gate_config_t *config,
                                   hu_memory_chunk_t *chunks, size_t chunk_count,
                                   size_t *out_passed_count);

hu_error_t hu_entropy_coarsen(hu_allocator_t *alloc,
                              const hu_memory_chunk_t *chunks, size_t chunk_count,
                              char *summary, size_t summary_max, size_t *summary_len);

#endif
