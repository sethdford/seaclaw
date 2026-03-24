#ifndef HU_TOOLS_VALIDATION_H
#define HU_TOOLS_VALIDATION_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/tool.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_validation_level {
    HU_VALIDATE_NONE = 0,
    HU_VALIDATE_SCHEMA,
    HU_VALIDATE_SEMANTIC,
    HU_VALIDATE_FULL,
} hu_validation_level_t;

typedef struct hu_validation_rule {
    char tool_name[128];
    hu_validation_level_t level;
    char expected_type[32];
    size_t max_output_len;
    size_t min_output_len;
    bool require_non_empty;
    char contains_pattern[256];
} hu_validation_rule_t;

#define HU_VALIDATION_MAX_RULES 128

typedef struct hu_tool_validator {
    hu_validation_rule_t rules[HU_VALIDATION_MAX_RULES];
    size_t rule_count;
    hu_validation_level_t default_level;
    size_t total_checks;
    size_t schema_pass;
    size_t schema_fail;
    size_t semantic_pass;
    size_t semantic_fail;
} hu_tool_validator_t;

typedef struct hu_validation_result {
    bool passed;
    bool schema_ok;
    bool semantic_ok;
    char reason[256];
} hu_validation_result_t;

void hu_tool_validator_init(hu_tool_validator_t *v, hu_validation_level_t default_level);

hu_error_t hu_tool_validator_add_rule(hu_tool_validator_t *v, const hu_validation_rule_t *rule);

hu_error_t hu_tool_validator_check(hu_tool_validator_t *v, const char *tool_name,
                                   size_t tool_name_len, const hu_tool_result_t *result,
                                   hu_validation_result_t *out);

size_t hu_tool_validator_report(const hu_tool_validator_t *v, char *buf, size_t buf_size);

hu_error_t hu_tool_validate_path(const char *path, const char *workspace_dir,
                                 size_t workspace_dir_len);
hu_error_t hu_tool_validate_url(const char *url);

#endif /* HU_TOOLS_VALIDATION_H */
