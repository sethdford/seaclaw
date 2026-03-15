#include "human/core/error.h"
#include "human/runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct hu_wasm_runtime {
    uint64_t memory_limit_mb;
} hu_wasm_runtime_t;

static hu_wasm_runtime_t *get_ctx(void *ctx) {
    return (hu_wasm_runtime_t *)ctx;
}

static const char *impl_name(void *ctx) {
    (void)ctx;
    return "wasm";
}

static bool impl_has_shell_access(void *ctx) {
    (void)ctx;
    return false;
}

static bool impl_has_filesystem_access(void *ctx) {
    (void)ctx;
    return false;
}

static const char *impl_storage_path(void *ctx) {
    (void)ctx;
    return ".human/wasm";
}

static bool impl_supports_long_running(void *ctx) {
    (void)ctx;
    return false;
}

static uint64_t impl_memory_budget(void *ctx) {
    hu_wasm_runtime_t *w = get_ctx(ctx);
    if (w->memory_limit_mb > 0)
        return w->memory_limit_mb * 1024 * 1024;
    return 64 * 1024 * 1024; /* default 64 MB */
}

static hu_error_t wasm_wrap_command(void *ctx, const char **argv_in, size_t argc_in,
                                    const char **argv_out, size_t max_out, size_t *argc_out) {
#if !defined(HU_HAS_RUNTIME_EXOTIC)
    (void)ctx;
    (void)argv_in;
    (void)argc_in;
    (void)argv_out;
    (void)max_out;
    (void)argc_out;
    return HU_ERR_NOT_SUPPORTED;
#else
    hu_wasm_runtime_t *w = get_ctx(ctx);
    if (!argv_out || !argc_out || (argc_in > 0 && !argv_in))
        return HU_ERR_INVALID_ARGUMENT;
    /* wasmtime run --dir=. [--max-memory-pages=N] <argv...>; need 4+ slots + argc_in + NULL */
    size_t min_slots = 4 + argc_in + 1; /* wasmtime, run, --dir=., args, NULL */
    if (w->memory_limit_mb > 0)
        min_slots += 1; /* --max-memory-pages=N */
    if (max_out < min_slots)
        return HU_ERR_INVALID_ARGUMENT;

    size_t idx = 0;
    argv_out[idx++] = "wasmtime";
    argv_out[idx++] = "run";
    argv_out[idx++] = "--dir=.";

    if (w->memory_limit_mb > 0) {
        static char pages_arg[32];
        uint64_t pages = w->memory_limit_mb * 16; /* 64KB per page */
        int n = snprintf(pages_arg, sizeof(pages_arg), "--max-memory-pages=%llu",
                        (unsigned long long)pages);
        if (n <= 0 || (size_t)n >= sizeof(pages_arg))
            return HU_ERR_INVALID_ARGUMENT;
        argv_out[idx++] = pages_arg;
    }

    for (size_t i = 0; i < argc_in && idx < max_out - 1; i++)
        argv_out[idx++] = argv_in[i];

    argv_out[idx] = NULL;
    *argc_out = idx;
    return HU_OK;
#endif
}

static const hu_runtime_vtable_t wasm_vtable = {
    .name = impl_name,
    .has_shell_access = impl_has_shell_access,
    .has_filesystem_access = impl_has_filesystem_access,
    .storage_path = impl_storage_path,
    .supports_long_running = impl_supports_long_running,
    .memory_budget = impl_memory_budget,
    .wrap_command = wasm_wrap_command,
};

hu_runtime_t hu_runtime_wasm(uint64_t memory_limit_mb) {
    static hu_wasm_runtime_t s_wasm = {0};
    s_wasm.memory_limit_mb = memory_limit_mb;
    return (hu_runtime_t){
        .ctx = &s_wasm,
        .vtable = &wasm_vtable,
    };
}
