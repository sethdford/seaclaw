/* libFuzzer harness for visual grounding JSON response parsing. */
#include "human/core/allocator.h"
#include "human/core/json.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *json = NULL;
    hu_error_t err = hu_json_parse(&alloc, (const char *)data, size, &json);
    if (err == HU_OK && json) {
        (void)hu_json_get_number(json, "x", -1.0);
        (void)hu_json_get_number(json, "y", -1.0);
        (void)hu_json_get_number(json, "confidence", 0.0);
        const char *sel = hu_json_get_string(json, "selector");
        (void)sel;
        hu_json_free(&alloc, json);
    }
    return 0;
}
