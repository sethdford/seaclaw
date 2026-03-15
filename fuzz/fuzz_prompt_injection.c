#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Fuzz the system prompt parser to detect injection vectors.
 * Verifies that user input cannot escape the system prompt boundary.
 * Stub — implement when prompt builder has a public API.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 4096) return 0;

    char buf[4097];
    memcpy(buf, data, size);
    buf[size] = '\0';

    /* Test that the prompt builder properly escapes/sanitizes user input.
     * This is a stub — implement when prompt builder has a public API. */
    (void)buf;
    return 0;
}
