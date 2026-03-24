#ifndef HU_SECURITY_ARG_INSPECTOR_H
#define HU_SECURITY_ARG_INSPECTOR_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/security.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * AEGIS-inspired deep argument inspection for tool calls.
 *
 * Three-stage pipeline before tool execution:
 *   1. Deep string extraction — recursively pulls strings from JSON args
 *   2. Content-first risk scanning — scans extracted strings for dangerous patterns
 *   3. Composable policy validation — applies rules against extracted content
 *
 * Reference: arXiv:2603.12621 (AEGIS)
 */

typedef enum hu_arg_risk_flag {
    HU_ARG_RISK_NONE           = 0,
    HU_ARG_RISK_SHELL_INJECT   = 1 << 0,
    HU_ARG_RISK_PATH_TRAVERSAL = 1 << 1,
    HU_ARG_RISK_URL_SUSPECT    = 1 << 2,
    HU_ARG_RISK_SECRET_LEAK    = 1 << 3,
    HU_ARG_RISK_PROMPT_INJECT  = 1 << 4,
    HU_ARG_RISK_EXFILTRATION   = 1 << 5,
} hu_arg_risk_flag_t;

typedef struct hu_arg_inspection {
    uint32_t risk_flags;
    hu_command_risk_level_t overall_risk;
    char findings[512];
    size_t findings_len;
    size_t strings_extracted;
} hu_arg_inspection_t;

/**
 * Deep-inspect tool arguments for security risks.
 * Extracts all string values from args_json, scans each for dangerous patterns.
 * Operates in O(n) over the JSON with bounded stack depth.
 */
hu_error_t hu_arg_inspect(const char *tool_name, const char *args_json, size_t args_json_len,
                          hu_arg_inspection_t *out);

/** Check if inspection result should block execution given a policy. */
bool hu_arg_inspection_should_block(const hu_arg_inspection_t *insp,
                                    const hu_security_policy_t *policy);

/** Check if inspection result should require approval given a policy. */
bool hu_arg_inspection_needs_approval(const hu_arg_inspection_t *insp,
                                      const hu_security_policy_t *policy);

#endif /* HU_SECURITY_ARG_INSPECTOR_H */
