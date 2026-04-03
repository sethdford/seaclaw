#ifndef HU_SECURITY_SENSITIVITY_H
#define HU_SECURITY_SENSITIVITY_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Content sensitivity classification (inspired by EdgeClaw S1/S2/S3 tiers).
 *
 * S1 (Safe)      — no sensitive content; route to any provider including cloud.
 * S2 (Sensitive)  — contains PII or semi-private data; log warning, prefer local.
 * S3 (Private)    — contains secrets, keys, or highly private data; local-only.
 */

typedef enum hu_sensitivity_level {
    HU_SENSITIVITY_S1 = 1, /* safe — cloud OK */
    HU_SENSITIVITY_S2 = 2, /* sensitive — PII detected, prefer local */
    HU_SENSITIVITY_S3 = 3  /* private — secrets/keys, local-only */
} hu_sensitivity_level_t;

typedef struct hu_sensitivity_result {
    hu_sensitivity_level_t level;
    const char *reason;  /* static string describing why; NULL if S1 */
    float confidence;    /* 0.0-1.0: how confident we are in the classification */
    int signal_count;    /* number of independent signals that triggered this level */
} hu_sensitivity_result_t;

/* Classify a user message for data sensitivity using rule-based detection.
 * Checks keywords, regex-like patterns (SSN, credit card, private keys),
 * and file path patterns. Returns S1 if no sensitive content found. */
hu_sensitivity_result_t hu_sensitivity_classify_message(const char *msg, size_t msg_len);

/* Classify a file path for sensitivity.
 * Paths to SSH keys, .env files, credentials, etc. trigger S3. */
hu_sensitivity_result_t hu_sensitivity_classify_path(const char *path, size_t path_len);

/* Classify a tool name for sensitivity.
 * Tools that access secrets or credentials trigger S3. */
hu_sensitivity_result_t hu_sensitivity_classify_tool(const char *tool_name, size_t tool_len);

/* Merge multiple sensitivity results, taking the highest level. */
hu_sensitivity_result_t hu_sensitivity_merge(const hu_sensitivity_result_t *a,
                                             const hu_sensitivity_result_t *b);

/* Returns true if the level requires local-only processing. */
bool hu_sensitivity_requires_local(hu_sensitivity_level_t level);

const char *hu_sensitivity_level_str(hu_sensitivity_level_t level);

#endif /* HU_SECURITY_SENSITIVITY_H */
