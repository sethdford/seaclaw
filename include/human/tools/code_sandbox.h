#ifndef HU_TOOLS_CODE_SANDBOX_H
#define HU_TOOLS_CODE_SANDBOX_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tool.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HU_SANDBOX_PYTHON = 0,
    HU_SANDBOX_JAVASCRIPT,
    HU_SANDBOX_SHELL
} hu_sandbox_language_t;

typedef struct hu_code_sandbox_config {
    hu_sandbox_language_t language;
    int64_t timeout_ms;     /* default 10000 */
    size_t memory_limit_mb; /* default 256 */
    bool allow_network;     /* default false */
} hu_code_sandbox_config_t;

typedef struct hu_code_sandbox_result {
    char stdout_buf[4096];
    size_t stdout_len;
    char stderr_buf[2048];
    size_t stderr_len;
    int exit_code;
    int64_t elapsed_ms;
    bool timed_out;
    bool oom_killed;
} hu_code_sandbox_result_t;

hu_code_sandbox_config_t hu_code_sandbox_config_default(void);
hu_error_t hu_code_sandbox_execute(hu_allocator_t *alloc,
                                   const hu_code_sandbox_config_t *config,
                                   const char *code, size_t code_len,
                                   hu_code_sandbox_result_t *result);
const char *hu_sandbox_language_name(hu_sandbox_language_t lang);

typedef struct hu_code_sandbox_checkpoint {
    char state_id[64];
    hu_sandbox_language_t language;
    int64_t elapsed_ms;
    bool valid;
} hu_code_sandbox_checkpoint_t;

hu_error_t hu_code_sandbox_save_checkpoint(const hu_code_sandbox_result_t *result,
                                            hu_sandbox_language_t language,
                                            hu_code_sandbox_checkpoint_t *ckpt);
hu_error_t hu_code_sandbox_restore_checkpoint(const hu_code_sandbox_checkpoint_t *ckpt,
                                               hu_code_sandbox_config_t *config);

hu_error_t hu_code_sandbox_create(hu_allocator_t *alloc, hu_tool_t *out);

#endif
