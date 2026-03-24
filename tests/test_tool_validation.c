#include "human/tools/validation.h"
#include "test_framework.h"
#include <string.h>

static void test_validator_init(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_SCHEMA);
    HU_ASSERT_EQ((int)v.rule_count, 0);
    HU_ASSERT_EQ((int)v.default_level, (int)HU_VALIDATE_SCHEMA);
}

static void test_validator_add_rule(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_NONE);
    hu_validation_rule_t rule = {0};
    memcpy(rule.tool_name, "shell", 6);
    rule.level = HU_VALIDATE_FULL;
    rule.require_non_empty = true;
    HU_ASSERT_EQ(hu_tool_validator_add_rule(&v, &rule), HU_OK);
    HU_ASSERT_EQ((int)v.rule_count, 1);
}

static void test_validator_schema_pass(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_SCHEMA);

    hu_tool_result_t result = hu_tool_result_ok("hello world", 11);
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "test", 4, &result, &vr), HU_OK);
    HU_ASSERT_TRUE(vr.passed);
    HU_ASSERT_TRUE(vr.schema_ok);
}

static void test_validator_schema_fail_empty(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_SCHEMA);
    hu_validation_rule_t rule = {0};
    memcpy(rule.tool_name, "shell", 6);
    rule.level = HU_VALIDATE_SCHEMA;
    rule.require_non_empty = true;
    hu_tool_validator_add_rule(&v, &rule);

    hu_tool_result_t result = hu_tool_result_ok("", 0);
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "shell", 5, &result, &vr), HU_OK);
    HU_ASSERT_FALSE(vr.passed);
    HU_ASSERT_FALSE(vr.schema_ok);
}

static void test_validator_schema_fail_too_long(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_SCHEMA);
    hu_validation_rule_t rule = {0};
    memcpy(rule.tool_name, "test", 5);
    rule.level = HU_VALIDATE_SCHEMA;
    rule.max_output_len = 5;
    hu_tool_validator_add_rule(&v, &rule);

    hu_tool_result_t result = hu_tool_result_ok("this is too long", 16);
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "test", 4, &result, &vr), HU_OK);
    HU_ASSERT_FALSE(vr.passed);
}

static void test_validator_semantic_pass(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_FULL);
    hu_validation_rule_t rule = {0};
    memcpy(rule.tool_name, "calc", 5);
    rule.level = HU_VALIDATE_FULL;
    memcpy(rule.contains_pattern, "result", 7);
    hu_tool_validator_add_rule(&v, &rule);

    hu_tool_result_t result = hu_tool_result_ok("result: 42", 10);
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "calc", 4, &result, &vr), HU_OK);
    HU_ASSERT_TRUE(vr.passed);
    HU_ASSERT_TRUE(vr.semantic_ok);
}

static void test_validator_semantic_fail(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_FULL);
    hu_validation_rule_t rule = {0};
    memcpy(rule.tool_name, "calc", 5);
    rule.level = HU_VALIDATE_FULL;
    memcpy(rule.contains_pattern, "result", 7);
    hu_tool_validator_add_rule(&v, &rule);

    hu_tool_result_t result = hu_tool_result_ok("no match here", 13);
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "calc", 4, &result, &vr), HU_OK);
    HU_ASSERT_FALSE(vr.passed);
    HU_ASSERT_FALSE(vr.semantic_ok);
}

static void test_validator_json_type_check(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_SCHEMA);
    hu_validation_rule_t rule = {0};
    memcpy(rule.tool_name, "api", 4);
    rule.level = HU_VALIDATE_SCHEMA;
    memcpy(rule.expected_type, "json", 5);
    hu_tool_validator_add_rule(&v, &rule);

    hu_tool_result_t good = hu_tool_result_ok("{\"key\":\"val\"}", 13);
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "api", 3, &good, &vr), HU_OK);
    HU_ASSERT_TRUE(vr.schema_ok);

    hu_tool_result_t bad = hu_tool_result_ok("not json", 8);
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "api", 3, &bad, &vr), HU_OK);
    HU_ASSERT_FALSE(vr.schema_ok);
}

static void test_validator_report(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_SCHEMA);

    hu_tool_result_t result = hu_tool_result_ok("data", 4);
    hu_validation_result_t vr;
    hu_tool_validator_check(&v, "x", 1, &result, &vr);

    char buf[512];
    size_t len = hu_tool_validator_report(&v, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_CONTAINS(buf, "Total checks: 1");
}

static void test_validator_null_args(void) {
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(NULL, NULL, 0, NULL, &vr), HU_ERR_INVALID_ARGUMENT);
}

static void test_validator_none_level(void) {
    hu_tool_validator_t v;
    hu_tool_validator_init(&v, HU_VALIDATE_NONE);
    hu_tool_result_t result = hu_tool_result_ok("", 0);
    hu_validation_result_t vr;
    HU_ASSERT_EQ(hu_tool_validator_check(&v, "x", 1, &result, &vr), HU_OK);
    HU_ASSERT_TRUE(vr.passed);
}

void run_tool_validation_tests(void) {
    HU_TEST_SUITE("Tool Validation");
    HU_RUN_TEST(test_validator_init);
    HU_RUN_TEST(test_validator_add_rule);
    HU_RUN_TEST(test_validator_schema_pass);
    HU_RUN_TEST(test_validator_schema_fail_empty);
    HU_RUN_TEST(test_validator_schema_fail_too_long);
    HU_RUN_TEST(test_validator_semantic_pass);
    HU_RUN_TEST(test_validator_semantic_fail);
    HU_RUN_TEST(test_validator_json_type_check);
    HU_RUN_TEST(test_validator_report);
    HU_RUN_TEST(test_validator_null_args);
    HU_RUN_TEST(test_validator_none_level);
}
