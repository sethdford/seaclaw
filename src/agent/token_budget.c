#include "human/agent/token_budget.h"
#include <string.h>

static const hu_tier_budget_t DEFAULT_TIERS[HU_THINKING_TIER_COUNT] = {
    [HU_THINKING_TIER_REFLEXIVE] = {.max_input_tokens = 1024,
                                    .max_output_tokens = 512,
                                    .thinking_budget = 0,
                                    .temperature = 0.3},
    [HU_THINKING_TIER_CONVERSATIONAL] = {.max_input_tokens = 4096,
                                         .max_output_tokens = 2048,
                                         .thinking_budget = 0,
                                         .temperature = 0.7},
    [HU_THINKING_TIER_ANALYTICAL] = {.max_input_tokens = 16384,
                                     .max_output_tokens = 4096,
                                     .thinking_budget = 1024,
                                     .temperature = 0.5},
    [HU_THINKING_TIER_DEEP] = {.max_input_tokens = 65536,
                               .max_output_tokens = 8192,
                               .thinking_budget = 4096,
                               .temperature = 0.4},
    [HU_THINKING_TIER_EXPERT] = {.max_input_tokens = 131072,
                                 .max_output_tokens = 16384,
                                 .thinking_budget = 8192,
                                 .temperature = 0.3},
    [HU_THINKING_TIER_RESEARCH] = {.max_input_tokens = 200000,
                                   .max_output_tokens = 32768,
                                   .thinking_budget = 16384,
                                   .temperature = 0.2},
};

void hu_token_budget_init_defaults(hu_token_budget_config_t *config) {
    if (!config)
        return;
    memset(config, 0, sizeof(*config));
    config->enabled = false;
    memcpy(config->tiers, DEFAULT_TIERS, sizeof(DEFAULT_TIERS));
}

static bool has_keyword(const char *text, size_t len, const char *kw) {
    size_t klen = strlen(kw);
    if (klen > len)
        return false;
    for (size_t i = 0; i <= len - klen; i++) {
        bool match = true;
        for (size_t j = 0; j < klen && match; j++) {
            char c = text[i + j];
            if (c >= 'A' && c <= 'Z')
                c += 32;
            if (c != kw[j])
                match = false;
        }
        if (match)
            return true;
    }
    return false;
}

hu_thinking_tier_t hu_token_budget_classify(const char *prompt, size_t prompt_len,
                                            size_t history_count, size_t tool_count) {
    if (!prompt || prompt_len == 0)
        return HU_THINKING_TIER_REFLEXIVE;

    if (prompt_len < 20 && history_count == 0)
        return HU_THINKING_TIER_REFLEXIVE;

    bool has_research = has_keyword(prompt, prompt_len, "research") ||
                        has_keyword(prompt, prompt_len, "investigate") ||
                        has_keyword(prompt, prompt_len, "comprehensive");
    bool has_analyze = has_keyword(prompt, prompt_len, "analyze") ||
                       has_keyword(prompt, prompt_len, "compare") ||
                       has_keyword(prompt, prompt_len, "evaluate");
    bool has_expert = has_keyword(prompt, prompt_len, "implement") ||
                      has_keyword(prompt, prompt_len, "architect") ||
                      has_keyword(prompt, prompt_len, "optimize");
    bool has_deep = has_keyword(prompt, prompt_len, "explain in detail") ||
                    has_keyword(prompt, prompt_len, "step by step") ||
                    has_keyword(prompt, prompt_len, "thoroughly");

    if (has_research && tool_count > 3)
        return HU_THINKING_TIER_RESEARCH;
    if (has_expert && tool_count > 2)
        return HU_THINKING_TIER_EXPERT;
    if (has_deep || prompt_len > 500)
        return HU_THINKING_TIER_DEEP;
    if (has_analyze || history_count > 10)
        return HU_THINKING_TIER_ANALYTICAL;
    if (prompt_len > 100 || history_count > 3)
        return HU_THINKING_TIER_CONVERSATIONAL;

    return HU_THINKING_TIER_REFLEXIVE;
}

const hu_tier_budget_t *hu_token_budget_get(const hu_token_budget_config_t *config,
                                            hu_thinking_tier_t tier) {
    if (!config || tier >= HU_THINKING_TIER_COUNT)
        return NULL;
    return &config->tiers[tier];
}

bool hu_token_budget_can_spend(const hu_token_budget_config_t *config, uint32_t tokens) {
    if (!config || !config->enabled || config->total_budget == 0)
        return true;
    return config->spent + tokens <= config->total_budget;
}

void hu_token_budget_record(hu_token_budget_config_t *config, uint32_t tokens) {
    if (!config)
        return;
    config->spent += tokens;
}

const char *hu_thinking_tier_name(hu_thinking_tier_t tier) {
    switch (tier) {
    case HU_THINKING_TIER_REFLEXIVE:
        return "reflexive";
    case HU_THINKING_TIER_CONVERSATIONAL:
        return "conversational";
    case HU_THINKING_TIER_ANALYTICAL:
        return "analytical";
    case HU_THINKING_TIER_DEEP:
        return "deep";
    case HU_THINKING_TIER_EXPERT:
        return "expert";
    case HU_THINKING_TIER_RESEARCH:
        return "research";
    default:
        return "unknown";
    }
}
