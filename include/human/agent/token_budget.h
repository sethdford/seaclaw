#ifndef HU_AGENT_TOKEN_BUDGET_H
#define HU_AGENT_TOKEN_BUDGET_H

#include "human/agent/model_router.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_thinking_tier {
    HU_THINKING_TIER_REFLEXIVE = 0,
    HU_THINKING_TIER_CONVERSATIONAL,
    HU_THINKING_TIER_ANALYTICAL,
    HU_THINKING_TIER_DEEP,
    HU_THINKING_TIER_EXPERT,
    HU_THINKING_TIER_RESEARCH,
    HU_THINKING_TIER_COUNT,
} hu_thinking_tier_t;

typedef struct hu_tier_budget {
    uint32_t max_input_tokens;
    uint32_t max_output_tokens;
    uint32_t thinking_budget;
    double temperature;
} hu_tier_budget_t;

typedef struct hu_token_budget_config {
    bool enabled;
    hu_tier_budget_t tiers[HU_THINKING_TIER_COUNT];
    uint64_t total_budget;
    uint64_t spent;
} hu_token_budget_config_t;

typedef struct hu_tier_classification {
    hu_thinking_tier_t tier;
    double confidence;
    const char *reason;
} hu_tier_classification_t;

void hu_token_budget_init_defaults(hu_token_budget_config_t *config);

hu_thinking_tier_t hu_token_budget_classify(const char *prompt, size_t prompt_len,
                                            size_t history_count, size_t tool_count);

const hu_tier_budget_t *hu_token_budget_get(const hu_token_budget_config_t *config,
                                            hu_thinking_tier_t tier);

bool hu_token_budget_can_spend(const hu_token_budget_config_t *config, uint32_t tokens);

void hu_token_budget_record(hu_token_budget_config_t *config, uint32_t tokens);

const char *hu_thinking_tier_name(hu_thinking_tier_t tier);

#endif /* HU_AGENT_TOKEN_BUDGET_H */
