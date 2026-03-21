/* libFuzzer harness for contact phone number normalization. */
#include "human/memory/contact_graph.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 256)
        return 0;
    char input[257];
    for (size_t i = 0; i < size; i++)
        input[i] = (char)data[i];
    input[size] = '\0';

    char out[64];
    (void)hu_contact_normalize_phone(input, out, sizeof(out));

    /* Also test with zero-length buffer */
    char tiny[1];
    (void)hu_contact_normalize_phone(input, tiny, 0);

    /* NULL inputs */
    (void)hu_contact_normalize_phone(NULL, out, sizeof(out));
    (void)hu_contact_normalize_phone(input, NULL, sizeof(out));
    return 0;
}
