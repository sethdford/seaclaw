#ifndef HU_HOOK_PIPELINE_H
#define HU_HOOK_PIPELINE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/hook.h"

/* ──────────────────────────────────────────────────────────────────────────
 * Hook Pipeline: orchestrates execution of registered hooks.
 *
 * For a given event (pre/post), executes all matching hooks in registration
 * order. First DENY stops the pipeline — remaining hooks are skipped.
 * WARN hooks log but continue. ALLOW hooks are silent successes.
 *
 * The aggregated result contains the final decision and any messages.
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * Execute all hooks matching ctx->event in the registry.
 *
 * - Hooks run in registration order.
 * - First HU_HOOK_DENY stops the pipeline; result->decision = HU_HOOK_DENY.
 * - HU_HOOK_WARN continues but logs the warning.
 * - If a required hook fails to execute, it is treated as HU_HOOK_DENY.
 * - If no hooks match, result->decision = HU_HOOK_ALLOW.
 *
 * Caller must call hu_hook_result_free(alloc, result) when done.
 */
hu_error_t hu_hook_pipeline_execute(const hu_hook_registry_t *registry,
                                    hu_allocator_t *alloc,
                                    const hu_hook_context_t *ctx,
                                    hu_hook_result_t *result);

/**
 * Execute pipeline for pre-tool-execute event. Convenience wrapper that
 * builds the context and calls hu_hook_pipeline_execute.
 */
hu_error_t hu_hook_pipeline_pre_tool(const hu_hook_registry_t *registry,
                                     hu_allocator_t *alloc,
                                     const char *tool_name, size_t tool_name_len,
                                     const char *args_json, size_t args_json_len,
                                     hu_hook_result_t *result);

/**
 * Execute pipeline for post-tool-execute event.
 */
hu_error_t hu_hook_pipeline_post_tool(const hu_hook_registry_t *registry,
                                      hu_allocator_t *alloc,
                                      const char *tool_name, size_t tool_name_len,
                                      const char *args_json, size_t args_json_len,
                                      const char *result_output, size_t result_output_len,
                                      bool result_success,
                                      hu_hook_result_t *result);

#endif /* HU_HOOK_PIPELINE_H */
