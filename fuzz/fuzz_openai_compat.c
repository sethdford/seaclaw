/* libFuzzer harness for OpenAI-compatible API request parsing.
 * Feeds random bytes as HTTP request bodies to the chat completions handler.
 * Goal: find crashes or leaks in request body parsing.
 * Requires SC_GATEWAY_POSIX (conditionally compiled). */
#include "seaclaw/core/allocator.h"
#include "seaclaw/gateway/openai_compat.h"
#include <stddef.h>
#include <stdint.h>

#define FUZZ_MAX_INPUT 16384

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
#ifdef SC_GATEWAY_POSIX
    if (size > FUZZ_MAX_INPUT)
        return 0;

    sc_allocator_t alloc = sc_system_allocator();
    int status = 0;
    char *body = NULL;
    size_t body_len = 0;
    const char *content_type = NULL;

    sc_openai_compat_handle_chat_completions((const char *)data, size, &alloc, NULL, &status, &body,
                                             &body_len, &content_type);

    if (body)
        alloc.free(alloc.ctx, body, body_len);
#else
    (void)data;
    (void)size;
#endif
    return 0;
}
