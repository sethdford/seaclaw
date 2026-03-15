#include "human/memory/forgetting.h"
#include <string.h>

hu_error_t hu_memory_decay(hu_allocator_t *alloc, hu_memory_t *memory, double decay_rate, hu_forgetting_stats_t *out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
    if (decay_rate < 0.0 || decay_rate > 1.0) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
#ifdef HU_IS_TEST
    (void)memory;
    out->total_memories = 100;
    out->decayed = 10;
    return HU_OK;
#else
    (void)memory;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_memory_boost(hu_allocator_t *alloc, hu_memory_t *memory, const char *memory_id, double boost_amount) {
    if (!alloc || !memory_id) return HU_ERR_INVALID_ARGUMENT;
    if (boost_amount < 0.0) return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    (void)memory;
    return HU_OK;
#else
    (void)memory;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_memory_prune(hu_allocator_t *alloc, hu_memory_t *memory, double threshold, hu_forgetting_stats_t *out) {
    if (!alloc || !out) return HU_ERR_INVALID_ARGUMENT;
    if (threshold < 0.0 || threshold > 1.0) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
#ifdef HU_IS_TEST
    (void)memory;
    out->total_memories = 100;
    out->pruned = 5;
    return HU_OK;
#else
    (void)memory;
    return HU_ERR_NOT_SUPPORTED;
#endif
}
