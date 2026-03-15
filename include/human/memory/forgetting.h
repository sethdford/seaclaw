#ifndef HU_FORGETTING_H
#define HU_FORGETTING_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stddef.h>
typedef struct hu_forgetting_stats { size_t total_memories; size_t decayed; size_t boosted; size_t pruned; } hu_forgetting_stats_t;
hu_error_t hu_memory_decay(hu_allocator_t *alloc, hu_memory_t *memory, double decay_rate, hu_forgetting_stats_t *out);
hu_error_t hu_memory_boost(hu_allocator_t *alloc, hu_memory_t *memory, const char *memory_id, double boost_amount);
hu_error_t hu_memory_prune(hu_allocator_t *alloc, hu_memory_t *memory, double threshold, hu_forgetting_stats_t *out);
#endif
