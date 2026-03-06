/* libFuzzer harness for sc_sse_parser_feed. Must not crash on any input. */
#include "seaclaw/core/allocator.h"
#include "seaclaw/providers/sse.h"
#include <stddef.h>
#include <stdint.h>

#define SC_FUZZ_SSE_MAX_INPUT 65536

static void sse_count_cb(const char *event_type, size_t event_type_len, const char *data,
                         size_t data_len, void *userdata) {
    (void)event_type;
    (void)event_type_len;
    (void)data;
    (void)data_len;
    (void)userdata;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > SC_FUZZ_SSE_MAX_INPUT)
        return 0;

    sc_allocator_t alloc = sc_system_allocator();
    sc_sse_parser_t p;
    sc_error_t err = sc_sse_parser_init(&p, &alloc);
    if (err != SC_OK)
        return 0;

    err = sc_sse_parser_feed(&p, (const char *)data, size, sse_count_cb, NULL);
    (void)err;

    sc_sse_parser_deinit(&p);
    return 0;
}
