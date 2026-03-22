#ifndef HU_AGENT_HULA_COMPILER_H
#define HU_AGENT_HULA_COMPILER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/agent/hula.h"
#include "human/agent/spawn.h"
#include "human/provider.h"
#include "human/tool.h"
#include <stdbool.h>
#include <stddef.h>

struct hu_agent_pool;

/* Pluggable LLM call for HuLa compiler (same shape as hu_provider_vtable_t.chat). */
typedef hu_error_t (*hu_hula_compiler_chat_fn)(void *ctx, hu_allocator_t *alloc,
                                               const hu_chat_request_t *req, const char *model,
                                               size_t model_len, double temperature,
                                               hu_chat_response_t *out);

/* Optional: after a successful run, before exec is torn down (append history, metrics). */
typedef void (*hu_hula_compiler_done_fn)(void *ctx, const hu_hula_program_t *prog,
                                           const hu_hula_exec_t *exec);

/* Build a user message for an LLM to emit a HuLa program JSON for the given goal.
 * Includes tool names and short descriptions from vtable. */
hu_error_t hu_hula_compiler_build_prompt(hu_allocator_t *alloc, const char *goal, size_t goal_len,
                                         hu_tool_t *tools, size_t tools_count, char **out,
                                         size_t *out_len);

/* Extract JSON from model response (fences, brace matching) and parse into out.
 * On failure, out is cleared (caller need not init). */
hu_error_t hu_hula_compiler_parse_response(hu_allocator_t *alloc, const char *response,
                                           size_t response_len, hu_hula_program_t *out);

/* Extract first <hula_program>...</hula_program> block and parse. Returns HU_ERR_NOT_FOUND if none. */
hu_error_t hu_hula_extract_program_from_text(hu_allocator_t *alloc, const char *text, size_t text_len,
                                             hu_hula_program_t *out);

/* Remove <hula_program>...</hula_program> from text; caller frees *out. */
hu_error_t hu_hula_strip_program_tags(hu_allocator_t *alloc, const char *text, size_t text_len,
                                      char **out, size_t *out_len);

/* Build compiler prompt → chat_fn (e.g. provider chat with response_format json_object) → parse →
 * validate → run. When pool and spawn_tpl are non-NULL, binds delegate spawns (spawn_tpl aliases
 * parent; must live through hu_hula_exec_run). trace_persist(NULL dir) when not HU_IS_TEST.
 * *out_ok is true when the program ran and the root node finished HU_HULA_DONE. */
hu_error_t hu_hula_compiler_chat_compile_execute(
    hu_allocator_t *alloc, const char *goal, size_t goal_len, hu_tool_t *tools, size_t tools_count,
    hu_security_policy_t *policy, hu_observer_t *observer, hu_agent_pool_t *pool,
    hu_spawn_config_t *spawn_tpl, hu_hula_compiler_chat_fn chat_fn, void *chat_ctx,
    const char *model_name, size_t model_name_len, double temperature,
    hu_hula_compiler_done_fn done_fn, void *done_ctx, bool *out_ok);

#endif /* HU_AGENT_HULA_COMPILER_H */
