#ifndef HU_MODEL_ROUTER_H
#define HU_MODEL_ROUTER_H

#include <stddef.h>
#include <stdint.h>

typedef enum hu_cognitive_tier {
    HU_TIER_REFLEXIVE = 0, /* backchannel, ack, simple reply */
    HU_TIER_CONVERSATIONAL, /* standard chat, moderate complexity */
    HU_TIER_ANALYTICAL,     /* questions, emotional depth, advice */
    HU_TIER_DEEP            /* complex reasoning, crisis, life decisions */
} hu_cognitive_tier_t;

typedef struct hu_model_selection {
    const char *model;
    size_t model_len;
    int thinking_budget;    /* 0 = none, >0 = token budget for reasoning */
    double temperature;     /* 0.0 = use default, >0 = override */
    hu_cognitive_tier_t tier;
} hu_model_selection_t;

typedef struct hu_model_router_config {
    const char *reflexive_model;    /* fast: backchannel, ack */
    size_t reflexive_model_len;
    const char *conversational_model; /* standard: normal chat */
    size_t conversational_model_len;
    const char *analytical_model;   /* capable: emotional, questions */
    size_t analytical_model_len;
    const char *deep_model;         /* most capable: complex reasoning */
    size_t deep_model_len;
} hu_model_router_config_t;

/* Analyze message content and context to select the optimal model + thinking budget.
 * relationship: "family", "close_friend", "friend", "regular", etc. NULL = unknown.
 * hour: 0-23, current hour of day. */
hu_model_selection_t hu_model_route(const hu_model_router_config_t *cfg,
                                    const char *msg, size_t msg_len,
                                    const char *relationship, size_t relationship_len,
                                    int hour, size_t history_count);

/* Initialize config with sensible Gemini defaults */
hu_model_router_config_t hu_model_router_default_config(void);

#endif
