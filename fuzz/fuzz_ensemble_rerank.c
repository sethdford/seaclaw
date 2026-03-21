/* libFuzzer harness for ensemble rerank digit parsing. */
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Simulate the rerank digit-parsing logic from ensemble.c */
    size_t choice = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] >= '1' && data[i] <= '9') {
            choice = (size_t)(data[i] - '0');
            break;
        }
    }
    /* Validate choice is within bounds (simulating 8 providers max) */
    if (choice >= 1 && choice <= 8) {
        (void)choice;
    }
    return 0;
}
