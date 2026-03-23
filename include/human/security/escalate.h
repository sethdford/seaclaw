#ifndef HU_SECURITY_ESCALATE_H
#define HU_SECURITY_ESCALATE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/security/audit.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_escalate_level {
    HU_ESCALATE_AUTO = 0,       /* Execute without approval */
    HU_ESCALATE_NOTIFY,         /* Execute and notify */
    HU_ESCALATE_APPROVE,        /* Require approval before execution */
    HU_ESCALATE_DENY,           /* Never execute */
} hu_escalate_level_t;

const char *hu_escalate_level_string(hu_escalate_level_t level);

typedef struct hu_escalate_rule {
    char action_pattern[128];   /* Glob pattern matching tool/action names */
    hu_escalate_level_t level;
    uint32_t timeout_s;         /* Approval timeout; 0 = no timeout */
    char notify_channel[64];    /* Channel to notify; empty = default */
} hu_escalate_rule_t;

#define HU_ESCALATE_MAX_RULES 64

typedef struct hu_escalate_protocol {
    hu_escalate_rule_t rules[HU_ESCALATE_MAX_RULES];
    size_t rule_count;
    hu_escalate_level_t default_level;
} hu_escalate_protocol_t;

/* Parse ESCALATE.md text into a protocol struct.
 * Format: markdown with a table where columns are: Action | Level | Timeout | Channel */
hu_error_t hu_escalate_parse(const char *text, size_t text_len, hu_escalate_protocol_t *out);

/* Evaluate a tool action against the protocol. Returns the matching escalation level. */
hu_escalate_level_t hu_escalate_evaluate(const hu_escalate_protocol_t *protocol,
                                         const char *action_name, size_t action_name_len);

/* Log an escalation decision to the audit logger. */
hu_error_t hu_escalate_log_decision(hu_audit_logger_t *logger,
                                    const char *action_name,
                                    hu_escalate_level_t level,
                                    bool approved);

typedef struct hu_escalate_report {
    size_t total_decisions;
    size_t auto_count;
    size_t notify_count;
    size_t approve_count;
    size_t deny_count;
    size_t approved_count;
    size_t denied_count;
} hu_escalate_report_t;

/* Generate a compliance report into buf. Returns bytes written. */
size_t hu_escalate_generate_report(const hu_escalate_report_t *report, char *buf, size_t buf_size);

#endif /* HU_SECURITY_ESCALATE_H */
