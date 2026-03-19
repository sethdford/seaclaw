/* libFuzzer harness for PRM step splitting and scoring. Must not crash on any input. */
#include "human/core/allocator.h"
#include "human/agent/process_reward.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0)
        return 0;
    hu_allocator_t alloc = hu_system_allocator();
    hu_prm_config_t cfg = hu_prm_config_default();

    hu_prm_result_t result;
    hu_error_t err = hu_prm_score_chain(&alloc, &cfg, (const char *)data, size, &result);
    if (err == HU_OK)
        hu_prm_result_free(&alloc, &result);

    double score;
    hu_prm_score_step(&alloc, &cfg, (const char *)data, size, NULL, 0, &score);

    return 0;
}
