#ifndef HU_COT_AUDIT_H
#define HU_COT_AUDIT_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
typedef enum { HU_COT_SAFE=0, HU_COT_SUSPICIOUS, HU_COT_BLOCKED } hu_cot_verdict_t;
typedef struct hu_cot_audit_result { hu_cot_verdict_t verdict; double confidence; char *reason; size_t reason_len; bool goal_hijack_detected; bool privilege_escalation_detected; bool data_exfiltration_detected; } hu_cot_audit_result_t;
hu_error_t hu_cot_audit(hu_allocator_t *alloc, const char *cot_text, size_t cot_len, hu_cot_audit_result_t *out);
void hu_cot_audit_result_free(hu_allocator_t *alloc, hu_cot_audit_result_t *result);
#endif
