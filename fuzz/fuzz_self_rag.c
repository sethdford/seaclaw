/* libFuzzer harness for Self-RAG retrieval decision and relevance verification. */
#include "human/core/allocator.h"
#include "human/memory/self_rag.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0)
        return 0;
    hu_allocator_t alloc = hu_system_allocator();
    hu_srag_config_t cfg = hu_srag_config_default();

    hu_srag_assessment_t assessment;
    hu_srag_should_retrieve(&alloc, &cfg, (const char *)data, size, NULL, 0, &assessment);

    /* Split input: first half as query, second half as retrieved content */
    if (size >= 4) {
        size_t mid = size / 2;
        double relevance = 0.0;
        bool should_use = false;
        hu_srag_verify_relevance(&alloc, &cfg,
                                 (const char *)data, mid,
                                 (const char *)data + mid, size - mid,
                                 &relevance, &should_use);
    }

    return 0;
}
