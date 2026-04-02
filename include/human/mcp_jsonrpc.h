#ifndef HU_MCP_JSONRPC_H
#define HU_MCP_JSONRPC_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * JSON-RPC 2.0 message utilities for MCP protocol.
 * Used to build and parse JSON-RPC messages for HTTP and SSE transports.
 */

/**
 * Build a JSON-RPC 2.0 request message.
 *
 * @param alloc       Allocator for the output buffer.
 * @param id          Request ID (unique identifier for request/response matching).
 * @param method      RPC method name (e.g., "tools/list", "tools/call").
 * @param params_json JSON parameters object (e.g., "{\"arg\":\"value"}), or NULL for empty.
 * @param out         Receives pointer to allocated JSON string (caller must free).
 * @param out_len     Receives length of JSON string.
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_jsonrpc_build_request(hu_allocator_t *alloc, uint32_t id, const char *method,
                                        const char *params_json, char **out, size_t *out_len);

/**
 * Parse a JSON-RPC 2.0 response message.
 *
 * @param alloc          Allocator for output strings.
 * @param json           JSON response string from server.
 * @param json_len       Length of JSON string.
 * @param out_id         Receives the request ID from response (for matching).
 * @param out_result     Receives result (or error message if out_is_error is true).
 *                       Caller must free with alloc.
 * @param out_result_len Receives length of result string.
 * @param out_is_error   Set to true if response contains an error.
 * @return HU_OK on success (even if response contains RPC error).
 *         Returns error if JSON is malformed or required fields missing.
 */
hu_error_t hu_mcp_jsonrpc_parse_response(hu_allocator_t *alloc, const char *json, size_t json_len,
                                         uint32_t *out_id, char **out_result,
                                         size_t *out_result_len, bool *out_is_error);

/**
 * Build a JSON-RPC request for "tools/list" method.
 *
 * @param alloc   Allocator for the output buffer.
 * @param id      Request ID.
 * @param out     Receives pointer to allocated JSON string.
 * @param out_len Receives length of JSON string.
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_jsonrpc_build_tools_list(hu_allocator_t *alloc, uint32_t id, char **out,
                                            size_t *out_len);

/**
 * Build a JSON-RPC request for "tools/call" method.
 *
 * @param alloc     Allocator for the output buffer.
 * @param id        Request ID.
 * @param tool_name Name of the tool to call.
 * @param args_json JSON arguments object (e.g., "{\"arg\":\"value"}), or NULL for empty.
 * @param out       Receives pointer to allocated JSON string.
 * @param out_len   Receives length of JSON string.
 * @return HU_OK on success.
 */
hu_error_t hu_mcp_jsonrpc_build_tools_call(hu_allocator_t *alloc, uint32_t id,
                                            const char *tool_name, const char *args_json,
                                            char **out, size_t *out_len);

#endif /* HU_MCP_JSONRPC_H */
