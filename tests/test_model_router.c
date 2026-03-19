#include "test_framework.h"
#include "human/agent/model_router.h"
#include <string.h>

static hu_model_router_config_t cfg;

static void default_config_has_all_models(void) {
    cfg = hu_model_router_default_config();
    HU_ASSERT(cfg.reflexive_model != NULL);
    HU_ASSERT(cfg.conversational_model != NULL);
    HU_ASSERT(cfg.analytical_model != NULL);
    HU_ASSERT(cfg.deep_model != NULL);
    HU_ASSERT(cfg.reflexive_model_len > 0);
    HU_ASSERT(cfg.conversational_model_len > 0);
}

static void short_message_routes_reflexive(void) {
    cfg = hu_model_router_default_config();
    hu_model_selection_t sel = hu_model_route(&cfg, "lol", 3, NULL, 0, 14, 0);
    HU_ASSERT(sel.tier == HU_TIER_REFLEXIVE);
    HU_ASSERT(sel.thinking_budget == 0);
}

static void simple_ack_routes_reflexive(void) {
    cfg = hu_model_router_default_config();
    hu_model_selection_t sel = hu_model_route(&cfg, "ok cool", 7, NULL, 0, 10, 0);
    HU_ASSERT(sel.tier == HU_TIER_REFLEXIVE);
}

static void normal_question_routes_higher(void) {
    cfg = hu_model_router_default_config();
    const char *msg = "hey what are you up to today? want to grab lunch or something";
    hu_model_selection_t sel = hu_model_route(&cfg, msg, strlen(msg), NULL, 0, 12, 2);
    HU_ASSERT(sel.tier >= HU_TIER_CONVERSATIONAL);
    HU_ASSERT(sel.thinking_budget > 0);
}

static void emotional_heavy_routes_deep(void) {
    cfg = hu_model_router_default_config();
    const char *msg = "I don't know what to do. Mom passed away last night and I'm terrified of what comes next";
    hu_model_selection_t sel = hu_model_route(&cfg, msg, strlen(msg), "family", 6, 2, 5);
    HU_ASSERT(sel.tier >= HU_TIER_ANALYTICAL);
    HU_ASSERT(sel.thinking_budget >= 4096);
}

static void family_gets_upgraded(void) {
    cfg = hu_model_router_default_config();
    const char *msg = "hey can you help me figure out this insurance thing it's confusing";
    hu_model_selection_t sel_family = hu_model_route(&cfg, msg, strlen(msg), "family", 6, 14, 2);
    hu_model_selection_t sel_regular = hu_model_route(&cfg, msg, strlen(msg), "regular", 7, 14, 2);
    HU_ASSERT(sel_family.tier >= sel_regular.tier);
}

static void advice_question_gets_analytical(void) {
    cfg = hu_model_router_default_config();
    const char *msg = "should i take the job offer or stay where i am? what do you think about the trade-offs";
    hu_model_selection_t sel = hu_model_route(&cfg, msg, strlen(msg), "sister", 6, 20, 3);
    HU_ASSERT(sel.tier >= HU_TIER_ANALYTICAL);
    HU_ASSERT(sel.thinking_budget >= 4096);
}

static void late_night_emotional_upgraded(void) {
    cfg = hu_model_router_default_config();
    const char *msg = "i can't sleep, feeling really stressed about everything going on";
    hu_model_selection_t sel_night = hu_model_route(&cfg, msg, strlen(msg), "friend", 6, 2, 1);
    hu_model_selection_t sel_day = hu_model_route(&cfg, msg, strlen(msg), "friend", 6, 14, 1);
    HU_ASSERT(sel_night.tier >= sel_day.tier);
}

static void null_msg_returns_conversational(void) {
    cfg = hu_model_router_default_config();
    hu_model_selection_t sel = hu_model_route(&cfg, NULL, 0, NULL, 0, 12, 0);
    HU_ASSERT(sel.tier == HU_TIER_CONVERSATIONAL);
    HU_ASSERT(sel.model != NULL);
}

static void null_cfg_returns_default(void) {
    hu_model_selection_t sel = hu_model_route(NULL, "hello", 5, NULL, 0, 12, 0);
    HU_ASSERT(sel.model != NULL);
    HU_ASSERT(sel.model_len > 0);
}

static void frustrated_message_not_reflexive(void) {
    cfg = hu_model_router_default_config();
    const char *msg = "ugh this budget is not working at all, the numbers are broken and i'm so frustrated with it";
    hu_model_selection_t sel = hu_model_route(&cfg, msg, strlen(msg), "family", 6, 10, 3);
    HU_ASSERT(sel.tier > HU_TIER_REFLEXIVE);
    HU_ASSERT(sel.thinking_budget > 0);
}

static void thinking_budget_scales_with_tier(void) {
    cfg = hu_model_router_default_config();
    hu_model_selection_t reflexive = hu_model_route(&cfg, "ok", 2, NULL, 0, 12, 0);
    hu_model_selection_t deep = hu_model_route(
        &cfg, "I don't know what to do, I'm terrified and need help deciding about the divorce",
        80, "family", 6, 2, 5);
    HU_ASSERT(deep.thinking_budget > reflexive.thinking_budget);
}

void run_model_router_tests(void) {
    HU_TEST_SUITE("Model Router");

    HU_RUN_TEST(default_config_has_all_models);
    HU_RUN_TEST(short_message_routes_reflexive);
    HU_RUN_TEST(simple_ack_routes_reflexive);
    HU_RUN_TEST(normal_question_routes_higher);
    HU_RUN_TEST(emotional_heavy_routes_deep);
    HU_RUN_TEST(family_gets_upgraded);
    HU_RUN_TEST(advice_question_gets_analytical);
    HU_RUN_TEST(late_night_emotional_upgraded);
    HU_RUN_TEST(null_msg_returns_conversational);
    HU_RUN_TEST(null_cfg_returns_default);
    HU_RUN_TEST(frustrated_message_not_reflexive);
    HU_RUN_TEST(thinking_budget_scales_with_tier);
}
