/* Tests for diagnostic slash commands (/hooks, /mcp, /permissions, /instructions) */

#include "human/agent/commands.h"
#include "test_framework.h"
#include <string.h>
#include <stdlib.h>

/* Test 1: Slash command parsing for /hooks */
static void test_parse_slash_hooks(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/hooks", 6);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_EQ(cmd->name_len, 5);
    HU_ASSERT(strstr(cmd->name, "hooks") != NULL);
}

/* Test 2: Slash command parsing for /mcp */
static void test_parse_slash_mcp(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/mcp", 4);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_EQ(cmd->name_len, 3);
    HU_ASSERT(strstr(cmd->name, "mcp") != NULL);
}

/* Test 3: Slash command parsing for /permissions */
static void test_parse_slash_permissions(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/permissions", 12);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_EQ(cmd->name_len, 11);
    HU_ASSERT(strstr(cmd->name, "permissions") != NULL);
}

/* Test 4: Slash command parsing for /instructions */
static void test_parse_slash_instructions(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/instructions", 13);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_EQ(cmd->name_len, 12);
    HU_ASSERT(strstr(cmd->name, "instructions") != NULL);
}

/* Test 5: Slash command parsing with arguments */
static void test_parse_slash_with_args(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/hooks extra args", 17);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT(strstr(cmd->name, "hooks") != NULL);
}

/* Test 6: Slash command parsing respects colons */
static void test_parse_slash_with_colon(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/permissions: arg", 16);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT(strstr(cmd->name, "permissions") != NULL);
}

/* Test 7: Case-insensitive parsing of hooks */
static void test_parse_slash_case_insensitive_hooks(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/HOOKS", 6);
    HU_ASSERT_NOT_NULL(cmd);
    /* Name is stored in buffer which preserves case, but matching is case-insensitive */
    HU_ASSERT_EQ(cmd->name_len, 5);
}

/* Test 8: Case-insensitive parsing of permissions */
static void test_parse_slash_case_insensitive_permissions(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/Permissions", 12);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_EQ(cmd->name_len, 11);
}

/* Test 9: Whitespace handling in slash commands */
static void test_parse_slash_with_whitespace(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/help arg1 arg2", 15);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT(strstr(cmd->name, "help") != NULL);
}

/* Test 10: Empty name after slash returns NULL */
static void test_parse_slash_empty_returns_null(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/", 1);
    HU_ASSERT_NULL(cmd);
}

/* Test 11: Non-slash input returns NULL */
static void test_parse_non_slash_returns_null(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("no slash", 8);
    HU_ASSERT_NULL(cmd);
}

/* Test 12: Slash at end of message still parses */
static void test_parse_slash_no_trailing_newline(void) {
    const hu_slash_cmd_t *cmd = hu_agent_commands_parse("/hooks", 6);
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT(strstr(cmd->name, "hooks") != NULL);
}

void run_diagnostic_commands_tests(void) {
    HU_TEST_SUITE("diagnostic slash commands parsing");
    HU_RUN_TEST(test_parse_slash_hooks);
    HU_RUN_TEST(test_parse_slash_mcp);
    HU_RUN_TEST(test_parse_slash_permissions);
    HU_RUN_TEST(test_parse_slash_instructions);
    HU_RUN_TEST(test_parse_slash_with_args);
    HU_RUN_TEST(test_parse_slash_with_colon);
    HU_RUN_TEST(test_parse_slash_case_insensitive_hooks);
    HU_RUN_TEST(test_parse_slash_case_insensitive_permissions);
    HU_RUN_TEST(test_parse_slash_with_whitespace);
    HU_RUN_TEST(test_parse_slash_empty_returns_null);
    HU_RUN_TEST(test_parse_non_slash_returns_null);
    HU_RUN_TEST(test_parse_slash_no_trailing_newline);
}
