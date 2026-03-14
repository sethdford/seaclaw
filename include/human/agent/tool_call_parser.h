#ifndef HU_AGENT_TOOL_CALL_PARSER_H
#define HU_AGENT_TOOL_CALL_PARSER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stddef.h>

#define HU_TEXT_TOOL_CALL_MAX 16

/*
 * Parse text-based tool calls from LLM output.
 * Expects <tool_call>{"name":"...","arguments":{...}}</tool_call> blocks.
 * Returns HU_OK even when no tool calls are found (out_count will be 0).
 * Caller must free each tool call's fields and the array via
 * hu_text_tool_calls_free.
 */
hu_error_t hu_text_tool_calls_parse(hu_allocator_t *alloc, const char *text, size_t text_len,
                                    hu_tool_call_t **out, size_t *out_count);

/* Free array returned by hu_text_tool_calls_parse. */
void hu_text_tool_calls_free(hu_allocator_t *alloc, hu_tool_call_t *calls, size_t count);

/*
 * Extract the non-tool-call portions of the text (everything outside
 * <tool_call>...</tool_call> blocks). Caller owns returned string.
 * Returns HU_OK; *out may be NULL if text is entirely tool calls.
 */
hu_error_t hu_text_tool_calls_strip(hu_allocator_t *alloc, const char *text, size_t text_len,
                                    char **out, size_t *out_len);

#endif /* HU_AGENT_TOOL_CALL_PARSER_H */
