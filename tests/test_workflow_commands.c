#include "test_framework.h"
#include "human/agent/commands.h"
#include "human/core/allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Allocator wrapper functions */
static void *test_alloc_wrapper(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}

static void test_free_wrapper(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}

/* Test allocator */
__attribute__((unused))
static hu_allocator_t test_alloc = {
    .ctx = NULL,
    .alloc = test_alloc_wrapper,
    .free = test_free_wrapper,
    .realloc = NULL
};

/* Helper to parse a slash command */
static const hu_slash_cmd_t *parse_cmd(const char *msg) {
    return hu_agent_commands_parse(msg, strlen(msg));
}

static void test_workflow_start_parse(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/workflow start Deploy to staging");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "start");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "Deploy");
}

static void test_workflow_status_parse(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/workflow status wf-123");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "status");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "wf-123");
}

static void test_workflow_resume_parse(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/workflow resume wf-456");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "resume");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "wf-456");
}

static void test_workflow_list_parse(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/workflow list");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "list");
}

static void test_workflow_approve_parse(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/workflow approve gate-123 Looks good");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "approve");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "gate-123");
}

static void test_workflow_reject_parse(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/workflow reject gate-789 Needs more testing");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "reject");
    HU_ASSERT_STR_CONTAINS(cmd->arg, "gate-789");
}

static void test_non_workflow_command(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/help");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "help");
    HU_ASSERT_EQ(cmd->arg_len, 0);
}

static void test_not_slash_command(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("just a message");
    HU_ASSERT_NULL(cmd);
}

static void test_workflow_with_quotes(void) {
    /* Multi-word descriptions */
    const hu_slash_cmd_t *cmd =
        parse_cmd("/workflow start \"Deploy to production with blue-green strategy\"");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
}

static void test_workflow_empty_arg(void) {
    const hu_slash_cmd_t *cmd = parse_cmd("/workflow");
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_EQ(cmd->name, "workflow");
    HU_ASSERT_EQ(cmd->arg_len, 0);
}

void run_workflow_commands_tests(void) {
    HU_TEST_SUITE("workflow_commands");

    HU_RUN_TEST(test_workflow_start_parse);
    HU_RUN_TEST(test_workflow_status_parse);
    HU_RUN_TEST(test_workflow_resume_parse);
    HU_RUN_TEST(test_workflow_list_parse);
    HU_RUN_TEST(test_workflow_approve_parse);
    HU_RUN_TEST(test_workflow_reject_parse);
    HU_RUN_TEST(test_non_workflow_command);
    HU_RUN_TEST(test_not_slash_command);
    HU_RUN_TEST(test_workflow_with_quotes);
    HU_RUN_TEST(test_workflow_empty_arg);
}
