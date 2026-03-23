#include "test_framework.h"
#include "human/security/escalate.h"
#include <string.h>

static const char VALID_ESCALATE_MD[] =
    "# ESCALATE.md\n"
    "\n"
    "## Approval Matrix\n"
    "\n"
    "| Action | Level | Timeout | Channel |\n"
    "| --- | --- | --- | --- |\n"
    "| shell_* | approve | 300 | slack |\n"
    "| file_read | auto | 0 | |\n"
    "| file_write | notify | 0 | telegram |\n"
    "| deploy_* | deny | 0 | |\n"
    "| web_search | auto | 0 | |\n";

static void test_escalate_parse_valid(void) {
    hu_escalate_protocol_t protocol;
    hu_error_t err = hu_escalate_parse(VALID_ESCALATE_MD, strlen(VALID_ESCALATE_MD), &protocol);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)protocol.rule_count, 5);

    HU_ASSERT_STR_EQ(protocol.rules[0].action_pattern, "shell_*");
    HU_ASSERT_EQ((int)protocol.rules[0].level, (int)HU_ESCALATE_APPROVE);
    HU_ASSERT_EQ((int)protocol.rules[0].timeout_s, 300);
    HU_ASSERT_STR_EQ(protocol.rules[0].notify_channel, "slack");

    HU_ASSERT_STR_EQ(protocol.rules[1].action_pattern, "file_read");
    HU_ASSERT_EQ((int)protocol.rules[1].level, (int)HU_ESCALATE_AUTO);

    HU_ASSERT_STR_EQ(protocol.rules[2].action_pattern, "file_write");
    HU_ASSERT_EQ((int)protocol.rules[2].level, (int)HU_ESCALATE_NOTIFY);
    HU_ASSERT_STR_EQ(protocol.rules[2].notify_channel, "telegram");

    HU_ASSERT_STR_EQ(protocol.rules[3].action_pattern, "deploy_*");
    HU_ASSERT_EQ((int)protocol.rules[3].level, (int)HU_ESCALATE_DENY);

    HU_ASSERT_STR_EQ(protocol.rules[4].action_pattern, "web_search");
    HU_ASSERT_EQ((int)protocol.rules[4].level, (int)HU_ESCALATE_AUTO);
}

static void test_escalate_parse_empty(void) {
    hu_escalate_protocol_t protocol;
    hu_error_t err = hu_escalate_parse("", 0, &protocol);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)protocol.rule_count, 0);
}

static void test_escalate_parse_null(void) {
    HU_ASSERT_EQ(hu_escalate_parse(NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_escalate_evaluate_auto(void) {
    hu_escalate_protocol_t protocol;
    hu_escalate_parse(VALID_ESCALATE_MD, strlen(VALID_ESCALATE_MD), &protocol);

    hu_escalate_level_t level = hu_escalate_evaluate(&protocol, "file_read", 9);
    HU_ASSERT_EQ((int)level, (int)HU_ESCALATE_AUTO);
}

static void test_escalate_evaluate_approve_glob(void) {
    hu_escalate_protocol_t protocol;
    hu_escalate_parse(VALID_ESCALATE_MD, strlen(VALID_ESCALATE_MD), &protocol);

    hu_escalate_level_t level = hu_escalate_evaluate(&protocol, "shell_exec", 10);
    HU_ASSERT_EQ((int)level, (int)HU_ESCALATE_APPROVE);
}

static void test_escalate_evaluate_deny_glob(void) {
    hu_escalate_protocol_t protocol;
    hu_escalate_parse(VALID_ESCALATE_MD, strlen(VALID_ESCALATE_MD), &protocol);

    hu_escalate_level_t level = hu_escalate_evaluate(&protocol, "deploy_production", 17);
    HU_ASSERT_EQ((int)level, (int)HU_ESCALATE_DENY);
}

static void test_escalate_evaluate_default(void) {
    hu_escalate_protocol_t protocol;
    hu_escalate_parse(VALID_ESCALATE_MD, strlen(VALID_ESCALATE_MD), &protocol);

    /* Unknown action -> default level (APPROVE) */
    hu_escalate_level_t level = hu_escalate_evaluate(&protocol, "unknown_action", 14);
    HU_ASSERT_EQ((int)level, (int)HU_ESCALATE_APPROVE);
}

static void test_escalate_evaluate_null(void) {
    hu_escalate_level_t level = hu_escalate_evaluate(NULL, "test", 4);
    HU_ASSERT_EQ((int)level, (int)HU_ESCALATE_APPROVE);
}

static void test_escalate_level_string(void) {
    HU_ASSERT_STR_EQ(hu_escalate_level_string(HU_ESCALATE_AUTO), "auto");
    HU_ASSERT_STR_EQ(hu_escalate_level_string(HU_ESCALATE_NOTIFY), "notify");
    HU_ASSERT_STR_EQ(hu_escalate_level_string(HU_ESCALATE_APPROVE), "approve");
    HU_ASSERT_STR_EQ(hu_escalate_level_string(HU_ESCALATE_DENY), "deny");
}

static void test_escalate_generate_report(void) {
    hu_escalate_report_t report = {
        .total_decisions = 100,
        .auto_count = 60,
        .notify_count = 15,
        .approve_count = 20,
        .deny_count = 5,
        .approved_count = 18,
        .denied_count = 2,
    };
    char buf[512];
    size_t len = hu_escalate_generate_report(&report, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_CONTAINS(buf, "Total decisions: 100");
    HU_ASSERT_STR_CONTAINS(buf, "Auto:    60");
    HU_ASSERT_STR_CONTAINS(buf, "Deny:    5");
    HU_ASSERT_STR_CONTAINS(buf, "approved: 18");
}

static void test_escalate_generate_report_null(void) {
    char buf[64];
    HU_ASSERT_EQ((int)hu_escalate_generate_report(NULL, buf, sizeof(buf)), 0);
    HU_ASSERT_EQ((int)hu_escalate_generate_report(NULL, NULL, 0), 0);
}

static void test_escalate_log_decision_null(void) {
    HU_ASSERT_EQ(hu_escalate_log_decision(NULL, "test", HU_ESCALATE_AUTO, true),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_escalate_log_decision(NULL, NULL, HU_ESCALATE_AUTO, true),
                 HU_ERR_INVALID_ARGUMENT);
}

void run_escalate_tests(void) {
    HU_TEST_SUITE("ESCALATE Compliance");
    HU_RUN_TEST(test_escalate_parse_valid);
    HU_RUN_TEST(test_escalate_parse_empty);
    HU_RUN_TEST(test_escalate_parse_null);
    HU_RUN_TEST(test_escalate_evaluate_auto);
    HU_RUN_TEST(test_escalate_evaluate_approve_glob);
    HU_RUN_TEST(test_escalate_evaluate_deny_glob);
    HU_RUN_TEST(test_escalate_evaluate_default);
    HU_RUN_TEST(test_escalate_evaluate_null);
    HU_RUN_TEST(test_escalate_level_string);
    HU_RUN_TEST(test_escalate_generate_report);
    HU_RUN_TEST(test_escalate_generate_report_null);
    HU_RUN_TEST(test_escalate_log_decision_null);
}
