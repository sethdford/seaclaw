#ifndef HU_HOOK_H
#define HU_HOOK_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Hook system: pre/post tool execution interception via shell commands.
 *
 * Hooks are shell commands triggered before or after tool execution.
 * Exit codes determine the decision:
 *   0 = Allow (HU_HOOK_ALLOW)
 *   2 = Deny  (HU_HOOK_DENY)
 *   3 = Warn  (HU_HOOK_WARN)
 *   Other = Error (treated as allow with warning)
 *
 * Hooks run in registration order. First deny stops the pipeline.
 * ────────────────────────────────────────────────────────────────────────── */

typedef enum hu_hook_event {
    HU_HOOK_PRE_TOOL_EXECUTE  = 0,
    HU_HOOK_POST_TOOL_EXECUTE = 1,
} hu_hook_event_t;

typedef enum hu_hook_decision {
    HU_HOOK_ALLOW = 0,
    HU_HOOK_DENY  = 2,
    HU_HOOK_WARN  = 3,
} hu_hook_decision_t;

typedef struct hu_hook_result {
    hu_hook_decision_t decision;
    char *message;        /* owned; NULL when no output */
    size_t message_len;
} hu_hook_result_t;

/* Context passed to each hook invocation */
typedef struct hu_hook_context {
    hu_hook_event_t event;
    const char *tool_name;
    size_t tool_name_len;
    const char *args_json;
    size_t args_json_len;
    const char *result_output;   /* post-hook only; NULL for pre-hook */
    size_t result_output_len;
    bool result_success;         /* post-hook only */
} hu_hook_context_t;

/* A single hook entry */
typedef struct hu_hook_entry {
    char *name;            /* owned; human-readable identifier */
    size_t name_len;
    hu_hook_event_t event;
    char *command;         /* owned; shell command template */
    size_t command_len;
    uint32_t timeout_sec;  /* 0 = default (30s) */
    bool required;         /* if true, execution error => deny */
} hu_hook_entry_t;

/* Opaque hook registry */
typedef struct hu_hook_registry hu_hook_registry_t;

/* Create an empty hook registry. */
hu_error_t hu_hook_registry_create(hu_allocator_t *alloc, hu_hook_registry_t **out);

/* Add a hook entry to the registry. The registry copies all data. */
hu_error_t hu_hook_registry_add(hu_hook_registry_t *reg, hu_allocator_t *alloc,
                                const hu_hook_entry_t *entry);

/* Get number of hooks in the registry. */
size_t hu_hook_registry_count(const hu_hook_registry_t *reg);

/* Get a hook entry by index (read-only). Returns NULL if out of bounds. */
const hu_hook_entry_t *hu_hook_registry_get(const hu_hook_registry_t *reg, size_t index);

/* Destroy the registry and all owned entries. */
void hu_hook_registry_destroy(hu_hook_registry_t *reg, hu_allocator_t *alloc);

/* Free a hook result's owned message. */
static inline void hu_hook_result_free(hu_allocator_t *alloc, hu_hook_result_t *r) {
    if (r->message) {
        alloc->free(alloc->ctx, r->message, r->message_len + 1);
        r->message = NULL;
        r->message_len = 0;
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * Shell execution (internal — used by hook_pipeline)
 * ────────────────────────────────────────────────────────────────────────── */

/* Execute a hook's shell command with the given context.
 * Under HU_IS_TEST, this calls the mock executor instead of fork/exec.
 * Returns the process exit code in *exit_code, stdout in *stdout_buf. */
hu_error_t hu_hook_shell_execute(hu_allocator_t *alloc, const hu_hook_entry_t *hook,
                                 const hu_hook_context_t *ctx, int *exit_code,
                                 char **stdout_buf, size_t *stdout_len);

/* Shell-escape a string for safe embedding in shell commands.
 * Returns owned string. Caller must free with alloc->free(ctx, ptr, len+1). */
hu_error_t hu_hook_shell_escape(hu_allocator_t *alloc, const char *input, size_t input_len,
                                char **out, size_t *out_len);

/* ──────────────────────────────────────────────────────────────────────────
 * Test mock support (only available when HU_IS_TEST is defined)
 * ────────────────────────────────────────────────────────────────────────── */

#ifdef HU_IS_TEST

/* Mock configuration: set exit code and stdout for the next N calls */
typedef struct hu_hook_mock_config {
    int exit_code;
    const char *stdout_data;
    size_t stdout_len;
} hu_hook_mock_config_t;

void hu_hook_mock_set(const hu_hook_mock_config_t *config);
void hu_hook_mock_set_sequence(const hu_hook_mock_config_t *configs, size_t count);
void hu_hook_mock_reset(void);
size_t hu_hook_mock_call_count(void);

/* Get the last command that was passed to the mock executor */
const char *hu_hook_mock_last_command(void);

#endif /* HU_IS_TEST */

#endif /* HU_HOOK_H */
