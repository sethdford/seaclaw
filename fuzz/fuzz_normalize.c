/* libFuzzer harness for hu_normalize_confusables. Must not crash on any input. */
#include "human/security/normalize.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 65536)
        return 0;

    char out[4096];
    size_t out_len = 0;
    hu_error_t err = hu_normalize_confusables((const char *)data, size, out, sizeof(out), &out_len);
    (void)err;
    (void)out_len;

    /* Truncation path: same logic as moderation fixed buffer */
    char small[64];
    size_t small_len = 0;
    (void)hu_normalize_confusables((const char *)data, size, small, sizeof(small), &small_len);

    return 0;
}
