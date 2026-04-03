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
    HU_ASSERT(sel.source == HU_ROUTE_HEURISTIC);
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

/* ── Route cache tests ────────────────────────────────────────────────── */

static void cache_init_all_empty(void) {
    hu_route_cache_t cache;
    hu_route_cache_init(&cache);
    hu_cognitive_tier_t tier;
    HU_ASSERT(!hu_route_cache_get(&cache, "hello", 5, 1000, &tier));
}

static void cache_put_and_get(void) {
    hu_route_cache_t cache;
    hu_route_cache_init(&cache);
    hu_route_cache_put(&cache, "hello world", 11, 1000, HU_TIER_ANALYTICAL);
    hu_cognitive_tier_t tier;
    HU_ASSERT(hu_route_cache_get(&cache, "hello world", 11, 1001, &tier));
    HU_ASSERT(tier == HU_TIER_ANALYTICAL);
}

static void cache_expires_after_ttl(void) {
    hu_route_cache_t cache;
    hu_route_cache_init(&cache);
    hu_route_cache_put(&cache, "test msg", 8, 1000, HU_TIER_DEEP);
    hu_cognitive_tier_t tier;
    HU_ASSERT(hu_route_cache_get(&cache, "test msg", 8, 1299, &tier));
    HU_ASSERT(!hu_route_cache_get(&cache, "test msg", 8, 1301, &tier));
}

static void cache_different_messages_different_slots(void) {
    hu_route_cache_t cache;
    hu_route_cache_init(&cache);
    hu_route_cache_put(&cache, "msg A", 5, 1000, HU_TIER_REFLEXIVE);
    hu_route_cache_put(&cache, "msg B", 5, 1000, HU_TIER_DEEP);
    hu_cognitive_tier_t tier;
    HU_ASSERT(hu_route_cache_get(&cache, "msg B", 5, 1001, &tier));
    HU_ASSERT(tier == HU_TIER_DEEP);
}

static void cache_null_safety(void) {
    hu_route_cache_t cache;
    hu_route_cache_init(&cache);
    hu_cognitive_tier_t tier;
    HU_ASSERT(!hu_route_cache_get(NULL, "x", 1, 0, &tier));
    HU_ASSERT(!hu_route_cache_get(&cache, NULL, 0, 0, &tier));
    hu_route_cache_put(NULL, "x", 1, 0, HU_TIER_DEEP);
    hu_route_cache_put(&cache, NULL, 0, 0, HU_TIER_DEEP);
}

/* ── Judge response parser tests ──────────────────────────────────────── */

static void parse_judge_reflexive(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"REFLEXIVE\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_REFLEXIVE);
}

static void parse_judge_conversational(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"CONVERSATIONAL\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_CONVERSATIONAL);
}

static void parse_judge_analytical(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"ANALYTICAL\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_ANALYTICAL);
}

static void parse_judge_deep(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"DEEP\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_DEEP);
}

static void parse_judge_edgeclaw_simple_maps_reflexive(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"SIMPLE\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_REFLEXIVE);
}

static void parse_judge_edgeclaw_medium_maps_conversational(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"MEDIUM\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_CONVERSATIONAL);
}

static void parse_judge_edgeclaw_complex_maps_analytical(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"COMPLEX\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_ANALYTICAL);
}

static void parse_judge_edgeclaw_reasoning_maps_deep(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"REASONING\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_DEEP);
}

static void parse_judge_with_whitespace(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "  { \"tier\" : \"ANALYTICAL\" }  ";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_ANALYTICAL);
}

static void parse_judge_invalid_returns_false(void) {
    hu_cognitive_tier_t tier;
    HU_ASSERT(!hu_route_parse_judge_response("garbage", 7, &tier));
    HU_ASSERT(!hu_route_parse_judge_response("{\"tier\":\"UNKNOWN\"}", 18, &tier));
    HU_ASSERT(!hu_route_parse_judge_response(NULL, 0, &tier));
    HU_ASSERT(!hu_route_parse_judge_response("{}", 2, &tier));
}

static void parse_judge_case_insensitive(void) {
    hu_cognitive_tier_t tier;
    const char *resp = "{\"tier\":\"deep\"}";
    HU_ASSERT(hu_route_parse_judge_response(resp, strlen(resp), &tier));
    HU_ASSERT(tier == HU_TIER_DEEP);
}

/* ── Prompt hash tests ────────────────────────────────────────────────── */

static void hash_deterministic(void) {
    uint64_t h1 = hu_route_hash_prompt("hello world", 11);
    uint64_t h2 = hu_route_hash_prompt("hello world", 11);
    HU_ASSERT(h1 == h2);
}

static void hash_different_inputs_differ(void) {
    uint64_t h1 = hu_route_hash_prompt("hello", 5);
    uint64_t h2 = hu_route_hash_prompt("world", 5);
    HU_ASSERT(h1 != h2);
}

/* ── Decision log tests ───────────────────────────────────────────────── */

static void log_init_empty(void) {
    hu_route_decision_log_t log;
    hu_route_log_init(&log);
    HU_ASSERT(hu_route_log_count(&log) == 0);
    HU_ASSERT(hu_route_log_get(&log, 0) == NULL);
}

static void log_record_and_get(void) {
    hu_route_decision_log_t log;
    hu_route_log_init(&log);
    hu_model_selection_t sel;
    memset(&sel, 0, sizeof(sel));
    sel.tier = HU_TIER_ANALYTICAL;
    sel.source = HU_ROUTE_JUDGE;
    sel.model = "gemini-3.1-pro-preview";
    sel.model_len = 22;
    hu_route_log_record(&log, &sel, 5, 1000);
    HU_ASSERT(hu_route_log_count(&log) == 1);
    const hu_route_decision_t *d = hu_route_log_get(&log, 0);
    HU_ASSERT(d != NULL);
    HU_ASSERT(d->tier == HU_TIER_ANALYTICAL);
    HU_ASSERT(d->source == HU_ROUTE_JUDGE);
    HU_ASSERT(d->heuristic_score == 5);
    HU_ASSERT(strcmp(d->model, "gemini-3.1-pro-preview") == 0);
}

static void log_wraps_around(void) {
    hu_route_decision_log_t log;
    hu_route_log_init(&log);
    hu_model_selection_t sel;
    memset(&sel, 0, sizeof(sel));
    sel.model = "m";
    sel.model_len = 1;
    for (int i = 0; i < HU_ROUTE_LOG_SIZE + 10; i++) {
        sel.tier = (hu_cognitive_tier_t)(i % 4);
        hu_route_log_record(&log, &sel, i, (int64_t)i);
    }
    HU_ASSERT(hu_route_log_count(&log) == HU_ROUTE_LOG_SIZE);
}

static void log_tier_counts_correct(void) {
    hu_route_decision_log_t log;
    hu_route_log_init(&log);
    hu_model_selection_t sel;
    memset(&sel, 0, sizeof(sel));
    sel.model = "m";
    sel.model_len = 1;

    sel.tier = HU_TIER_REFLEXIVE;
    hu_route_log_record(&log, &sel, 0, 1);
    hu_route_log_record(&log, &sel, 0, 2);
    sel.tier = HU_TIER_DEEP;
    hu_route_log_record(&log, &sel, 0, 3);

    size_t counts[4];
    hu_route_log_tier_counts(&log, counts);
    HU_ASSERT(counts[HU_TIER_REFLEXIVE] == 2);
    HU_ASSERT(counts[HU_TIER_CONVERSATIONAL] == 0);
    HU_ASSERT(counts[HU_TIER_DEEP] == 1);
}

/* ── String conversion tests ──────────────────────────────────────────── */

static void tier_str_all_values(void) {
    HU_ASSERT(strcmp(hu_cognitive_tier_str(HU_TIER_REFLEXIVE), "reflexive") == 0);
    HU_ASSERT(strcmp(hu_cognitive_tier_str(HU_TIER_CONVERSATIONAL), "conversational") == 0);
    HU_ASSERT(strcmp(hu_cognitive_tier_str(HU_TIER_ANALYTICAL), "analytical") == 0);
    HU_ASSERT(strcmp(hu_cognitive_tier_str(HU_TIER_DEEP), "deep") == 0);
}

static void source_str_all_values(void) {
    HU_ASSERT(strcmp(hu_route_source_str(HU_ROUTE_HEURISTIC), "heuristic") == 0);
    HU_ASSERT(strcmp(hu_route_source_str(HU_ROUTE_JUDGE), "judge") == 0);
    HU_ASSERT(strcmp(hu_route_source_str(HU_ROUTE_JUDGE_CACHED), "judge_cached") == 0);
    HU_ASSERT(strcmp(hu_route_source_str(HU_ROUTE_JUDGE_FALLBACK), "judge_fallback") == 0);
}

static void judge_system_prompt_not_null(void) {
    const char *prompt = hu_route_judge_system_prompt();
    HU_ASSERT(prompt != NULL);
    HU_ASSERT(strlen(prompt) > 100);
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

    /* Route cache */
    HU_RUN_TEST(cache_init_all_empty);
    HU_RUN_TEST(cache_put_and_get);
    HU_RUN_TEST(cache_expires_after_ttl);
    HU_RUN_TEST(cache_different_messages_different_slots);
    HU_RUN_TEST(cache_null_safety);

    /* Judge response parser */
    HU_RUN_TEST(parse_judge_reflexive);
    HU_RUN_TEST(parse_judge_conversational);
    HU_RUN_TEST(parse_judge_analytical);
    HU_RUN_TEST(parse_judge_deep);
    HU_RUN_TEST(parse_judge_edgeclaw_simple_maps_reflexive);
    HU_RUN_TEST(parse_judge_edgeclaw_medium_maps_conversational);
    HU_RUN_TEST(parse_judge_edgeclaw_complex_maps_analytical);
    HU_RUN_TEST(parse_judge_edgeclaw_reasoning_maps_deep);
    HU_RUN_TEST(parse_judge_with_whitespace);
    HU_RUN_TEST(parse_judge_invalid_returns_false);
    HU_RUN_TEST(parse_judge_case_insensitive);

    /* Prompt hash */
    HU_RUN_TEST(hash_deterministic);
    HU_RUN_TEST(hash_different_inputs_differ);

    /* Decision log */
    HU_RUN_TEST(log_init_empty);
    HU_RUN_TEST(log_record_and_get);
    HU_RUN_TEST(log_wraps_around);
    HU_RUN_TEST(log_tier_counts_correct);

    /* String conversions */
    HU_RUN_TEST(tier_str_all_values);
    HU_RUN_TEST(source_str_all_values);
    HU_RUN_TEST(judge_system_prompt_not_null);
}
