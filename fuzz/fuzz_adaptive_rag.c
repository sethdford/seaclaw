/* libFuzzer harness for Adaptive RAG feature extraction and strategy selection. */
#include "human/core/allocator.h"
#include "human/memory/adaptive_rag.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0)
        return 0;
    hu_allocator_t alloc = hu_system_allocator();
    hu_adaptive_rag_t rag;
    hu_adaptive_rag_create(&alloc, NULL, &rag);

    hu_rag_query_features_t features;
    hu_adaptive_rag_extract_features((const char *)data, size, &features);

    hu_adaptive_rag_select(&rag, (const char *)data, size);

    hu_adaptive_rag_record_outcome(&rag, HU_RAG_SEMANTIC, 0.5);

    hu_adaptive_rag_deinit(&rag);
    return 0;
}
