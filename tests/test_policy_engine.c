/*
 * Tests for src/security/policy_engine.c — advanced policy evaluation.
 */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/security/policy_engine.h"
#include "test_framework.h"
#include <string.h>

static void test_policy_engine_create_null_alloc_returns_null(void) {
    hu_policy_engine_t *e = hu_policy_engine_create(NULL);
    HU_ASSERT_NULL(e);
}

static void test_policy_engine_create_valid_alloc_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_policy_engine_t *e = hu_policy_engine_create(&alloc);
    HU_ASSERT_NOT_NULL(e);
    hu_policy_engine_destroy(e);
}

static void test_policy_engine_destroy_null_handles_gracefully(void) {
    hu_policy_engine_destroy(NULL);
}

static void test_policy_engine_empty_rules_returns_allow(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_policy_engine_t *e = hu_policy_engine_create(&alloc);
    HU_ASSERT_NOT_NULL(e);
    hu_policy_eval_ctx_t ctx = {
        .tool_name = "shell",
        .args_json = "{\"cmd\":\"ls\"}",
        .session_cost_usd = 0.0,
    };
    hu_policy_result_t res = hu_policy_engine_evaluate(e, &ctx);
    HU_ASSERT_EQ(res.action, HU_POLICY_ALLOW);
    HU_ASSERT_NULL(res.rule_name);
    hu_policy_engine_destroy(e);
}

static void test_policy_engine_add_rule_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_policy_engine_t *e = hu_policy_engine_create(&alloc);
    HU_ASSERT_NOT_NULL(e);
    hu_policy_match_t m = {.tool = "shell", .args_contains = NULL, .session_cost_gt = 0, .has_cost_check = false};
    hu_error_t err = hu_policy_engine_add_rule(e, "deny_shell", m, HU_POLICY_DENY, "blocked");
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_policy_engine_rule_count(e), 1u);
    hu_policy_engine_destroy(e);
}

static void test_policy_engine_add_rule_null_name_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_policy_engine_t *e = hu_policy_engine_create(&alloc);
    HU_ASSERT_NOT_NULL(e);
    hu_policy_match_t m = {0};
    hu_error_t err = hu_policy_engine_add_rule(e, NULL, m, HU_POLICY_ALLOW, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_policy_engine_destroy(e);
}

static void test_policy_engine_add_rule_null_engine_fails(void) {
    hu_policy_match_t m = {0};
    hu_error_t err = hu_policy_engine_add_rule(NULL, "rule", m, HU_POLICY_ALLOW, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_policy_engine_evaluate_null_engine_returns_allow(void) {
    hu_policy_eval_ctx_t ctx = {
        .tool_name = "shell",
        .args_json = "{}",
        .session_cost_usd = 0.0,
    };
    hu_policy_result_t res = hu_policy_engine_evaluate(NULL, &ctx);
    HU_ASSERT_EQ(res.action, HU_POLICY_ALLOW);
}

static void test_policy_engine_evaluate_null_ctx_returns_allow(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_policy_engine_t *e = hu_policy_engine_create(&alloc);
    HU_ASSERT_NOT_NULL(e);
    hu_policy_result_t res = hu_policy_engine_evaluate(e, NULL);
    HU_ASSERT_EQ(res.action, HU_POLICY_ALLOW);
    hu_policy_engine_destroy(e);
}

static void test_policy_engine_rule_count_null_returns_zero(void) {
    HU_ASSERT_EQ(hu_policy_engine_rule_count(NULL), 0u);
}

static void test_policy_engine_deny_rule_matches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_policy_engine_t *e = hu_policy_engine_create(&alloc);
    HU_ASSERT_NOT_NULL(e);
    hu_policy_match_t m = {.tool = "shell", .args_contains = NULL, .session_cost_gt = 0, .has_cost_check = false};
    hu_policy_engine_add_rule(e, "deny_shell", m, HU_POLICY_DENY, "blocked");
    hu_policy_eval_ctx_t ctx = {
        .tool_name = "shell",
        .args_json = "{\"cmd\":\"ls\"}",
        .session_cost_usd = 0.0,
    };
    hu_policy_result_t res = hu_policy_engine_evaluate(e, &ctx);
    HU_ASSERT_EQ(res.action, HU_POLICY_DENY);
    HU_ASSERT_STR_EQ(res.rule_name, "deny_shell");
    HU_ASSERT_STR_EQ(res.message, "blocked");
    hu_policy_engine_destroy(e);
}

static void test_policy_engine_require_approval_rule(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_policy_engine_t *e = hu_policy_engine_create(&alloc);
    HU_ASSERT_NOT_NULL(e);
    hu_policy_match_t m = {.tool = "run", .args_contains = "rm", .session_cost_gt = 0, .has_cost_check = false};
    hu_policy_engine_add_rule(e, "approve_rm", m, HU_POLICY_REQUIRE_APPROVAL, "needs approval");
    hu_policy_eval_ctx_t ctx = {
        .tool_name = "run",
        .args_json = "{\"cmd\":\"rm -rf /tmp/x\"}",
        .session_cost_usd = 0.0,
    };
    hu_policy_result_t res = hu_policy_engine_evaluate(e, &ctx);
    HU_ASSERT_EQ(res.action, HU_POLICY_REQUIRE_APPROVAL);
    HU_ASSERT_STR_EQ(res.rule_name, "approve_rm");
    hu_policy_engine_destroy(e);
}

void run_policy_engine_tests(void) {
    HU_TEST_SUITE("policy_engine");
    HU_RUN_TEST(test_policy_engine_create_null_alloc_returns_null);
    HU_RUN_TEST(test_policy_engine_create_valid_alloc_succeeds);
    HU_RUN_TEST(test_policy_engine_destroy_null_handles_gracefully);
    HU_RUN_TEST(test_policy_engine_empty_rules_returns_allow);
    HU_RUN_TEST(test_policy_engine_add_rule_succeeds);
    HU_RUN_TEST(test_policy_engine_add_rule_null_name_fails);
    HU_RUN_TEST(test_policy_engine_add_rule_null_engine_fails);
    HU_RUN_TEST(test_policy_engine_evaluate_null_engine_returns_allow);
    HU_RUN_TEST(test_policy_engine_evaluate_null_ctx_returns_allow);
    HU_RUN_TEST(test_policy_engine_rule_count_null_returns_zero);
    HU_RUN_TEST(test_policy_engine_deny_rule_matches);
    HU_RUN_TEST(test_policy_engine_require_approval_rule);
}
