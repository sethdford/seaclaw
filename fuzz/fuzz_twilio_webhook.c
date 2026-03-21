/* libFuzzer harness for Twilio webhook bodies processed by hu_twilio_on_webhook.
 * Under HU_IS_TEST this exercises queue push + truncation bounds; keeps ASan clean. */
#include "human/channels/twilio.h"
#include "human/core/allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FUZZ_MAX_INPUT 65536

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > FUZZ_MAX_INPUT)
        return 0;

    hu_allocator_t alloc = hu_system_allocator();
    hu_channel_t ch = {0};
    if (hu_twilio_create(&alloc, "AC", 2, "tok", 3, "+1", 2, "+2", 2, &ch) != HU_OK)
        return 0;

    char buf[FUZZ_MAX_INPUT];
    memcpy(buf, data, size);
    (void)hu_twilio_on_webhook(ch.ctx, &alloc, buf, size);

    hu_twilio_destroy(&ch);
    return 0;
}
