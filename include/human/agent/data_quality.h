#ifndef HU_AGENT_DATA_QUALITY_H
#define HU_AGENT_DATA_QUALITY_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Data quality checks before context assembly.
 * Validates context fragments for duplicates, empty fields, encoding issues,
 * and anomalous content before passing to the agent (SOTA assessment §4.3).
 */

typedef enum hu_dq_issue_type {
    HU_DQ_DUPLICATE = 0,
    HU_DQ_EMPTY,
    HU_DQ_TOO_LONG,
    HU_DQ_ENCODING_ERROR,
    HU_DQ_ANOMALOUS,
} hu_dq_issue_type_t;

typedef struct hu_dq_issue {
    hu_dq_issue_type_t type;
    size_t fragment_index;
    char description[128];
} hu_dq_issue_t;

typedef struct hu_dq_fragment {
    const char *content;
    size_t content_len;
    const char *source; /* e.g. "memory", "tool_result", "context" */
    size_t source_len;
} hu_dq_fragment_t;

#define HU_DQ_MAX_ISSUES 64

typedef struct hu_dq_result {
    hu_dq_issue_t issues[HU_DQ_MAX_ISSUES];
    size_t issue_count;
    size_t fragments_checked;
    size_t fragments_passed;
    size_t duplicates_found;
    bool passed; /* true if no blocking issues */
} hu_dq_result_t;

typedef struct hu_dq_config {
    bool enabled;
    bool deduplicate;           /* remove exact duplicates */
    bool check_encoding;        /* detect invalid UTF-8 */
    size_t max_fragment_len;    /* 0 = no limit */
    float similarity_threshold; /* 0.0-1.0 for near-duplicate detection; 0 = exact only */
} hu_dq_config_t;

#define HU_DQ_CONFIG_DEFAULT \
    {.enabled = true,        \
     .deduplicate = true,    \
     .check_encoding = true, \
     .max_fragment_len = 0,  \
     .similarity_threshold = 0.0f}

hu_error_t hu_dq_check(const hu_dq_config_t *config, const hu_dq_fragment_t *fragments,
                       size_t fragment_count, hu_dq_result_t *out);

bool hu_dq_is_valid_utf8(const char *s, size_t len);

size_t hu_dq_report(const hu_dq_result_t *result, char *buf, size_t buf_size);

#endif /* HU_AGENT_DATA_QUALITY_H */
