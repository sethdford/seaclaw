/* libFuzzer harness for Memory Tiers core prompt building and auto-tiering. */
#include "human/core/allocator.h"
#include "human/memory/tiers.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2)
        return 0;
    hu_allocator_t alloc = hu_system_allocator();
    hu_tier_manager_t mgr;
    hu_tier_manager_create(&alloc, NULL, &mgr);

    /* Use first byte to select which function to exercise */
    uint8_t selector = data[0];
    const char *payload = (const char *)data + 1;
    size_t payload_len = size - 1;

    if (selector < 64) {
        /* Exercise update_core with fuzzed field names and values */
        size_t split = payload_len / 2;
        if (split > 0)
            hu_tier_manager_update_core(&mgr, payload, split,
                                        payload + split, payload_len - split);
    } else if (selector < 128) {
        /* Exercise build_core_prompt after populating fields with fuzz data */
        size_t chunk = payload_len / 5;
        if (chunk > 0 && chunk < sizeof(mgr.core.user_name) - 1) {
            memcpy(mgr.core.user_name, payload, chunk < 127 ? chunk : 127);
            mgr.core.user_name[chunk < 127 ? chunk : 127] = '\0';
        }
        char out[512];
        size_t out_len = 0;
        hu_tier_manager_build_core_prompt(&mgr, out, sizeof(out), &out_len);
    } else if (selector < 192) {
        /* Exercise auto_tier with fuzzed content */
        hu_memory_tier_t assigned;
        hu_tier_manager_auto_tier(&mgr, "fuzz_key", 8, payload, payload_len, &assigned);
    } else {
        /* Exercise build_core_prompt with very small buffer */
        char tiny[8];
        size_t tiny_len = 0;
        if (payload_len > 0 && payload_len < sizeof(mgr.core.user_name) - 1) {
            memcpy(mgr.core.user_name, payload, payload_len);
            mgr.core.user_name[payload_len] = '\0';
        }
        hu_tier_manager_build_core_prompt(&mgr, tiny, sizeof(tiny), &tiny_len);
    }

    hu_tier_manager_deinit(&mgr);
    return 0;
}
