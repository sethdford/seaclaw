#ifndef HU_MCP_AUDIT_H
#define HU_MCP_AUDIT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum hu_mcp_audit_risk {
    HU_MCP_AUDIT_CLEAN,
    HU_MCP_AUDIT_LOW,
    HU_MCP_AUDIT_MEDIUM,
    HU_MCP_AUDIT_HIGH,
    HU_MCP_AUDIT_CRITICAL,
} hu_mcp_audit_risk_t;

typedef struct hu_mcp_audit_finding {
    hu_mcp_audit_risk_t risk;
    const char *pattern;
    const char *description;
} hu_mcp_audit_finding_t;

typedef struct hu_mcp_audit_result {
    hu_mcp_audit_risk_t overall_risk;
    hu_mcp_audit_finding_t *findings;
    size_t finding_count;
    size_t findings_cap;
} hu_mcp_audit_result_t;

hu_error_t hu_mcp_audit_tool_description(hu_allocator_t *alloc, const char *description, size_t desc_len, hu_mcp_audit_result_t *out);
hu_error_t hu_mcp_audit_server(hu_allocator_t *alloc, const char **descriptions, size_t count, hu_mcp_audit_result_t *out);
void hu_mcp_audit_result_free(hu_allocator_t *alloc, hu_mcp_audit_result_t *result);

#endif
