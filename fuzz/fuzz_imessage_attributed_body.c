/* Fuzz harness for hu_imessage_extract_attributed_body.
 * Must not crash on any input — tests the NSKeyedArchiver binary parser. */
#include "human/channels/imessage.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 131072)
        return 0;

    char out[8192];
    hu_imessage_extract_attributed_body(data, size, out, sizeof(out));

    /* Also exercise small output buffer edge case */
    char tiny[4];
    hu_imessage_extract_attributed_body(data, size, tiny, sizeof(tiny));

    /* And single-byte output cap */
    char one[1];
    hu_imessage_extract_attributed_body(data, size, one, sizeof(one));

    return 0;
}
