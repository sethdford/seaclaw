#include "human/agent/agent_git.h"
#include "human/core/allocator.h"
#include "test_framework.h"

static void agent_git_init_null_args(void) {
    HU_ASSERT_EQ(hu_agent_git_init(NULL, "/tmp"), HU_ERR_INVALID_ARGUMENT);
    hu_allocator_t a = hu_system_allocator();
    HU_ASSERT_EQ(hu_agent_git_init(&a, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void agent_git_snapshot_null_args(void) {
    HU_ASSERT_EQ(hu_agent_git_snapshot(NULL, "/tmp", "msg"), HU_ERR_INVALID_ARGUMENT);
    hu_allocator_t a = hu_system_allocator();
    HU_ASSERT_EQ(hu_agent_git_snapshot(&a, NULL, "msg"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_snapshot(&a, "/tmp", NULL), HU_ERR_INVALID_ARGUMENT);
}

static void agent_git_rollback_null_args(void) {
    hu_allocator_t a = hu_system_allocator();
    HU_ASSERT_EQ(hu_agent_git_rollback(NULL, "/tmp", "HEAD"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_rollback(&a, NULL, "HEAD"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_rollback(&a, "/tmp", NULL), HU_ERR_INVALID_ARGUMENT);
}

static void agent_git_diff_null_args(void) {
    hu_allocator_t a = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_agent_git_diff(NULL, "/tmp", "a", "b", &out, &len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_diff(&a, "/tmp", "a", "b", NULL, &len), HU_ERR_INVALID_ARGUMENT);
}

static void agent_git_branch_null_args(void) {
    hu_allocator_t a = hu_system_allocator();
    HU_ASSERT_EQ(hu_agent_git_branch(NULL, "/tmp", "feat"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_branch(&a, NULL, "feat"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_branch(&a, "/tmp", NULL), HU_ERR_INVALID_ARGUMENT);
}

static void agent_git_log_null_args(void) {
    hu_allocator_t a = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_agent_git_log(NULL, "/tmp", 10, &out, &len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_log(&a, NULL, 10, &out, &len), HU_ERR_INVALID_ARGUMENT);
}

static void agent_git_init_test_mode(void) {
    hu_allocator_t a = hu_system_allocator();
    HU_ASSERT_EQ(hu_agent_git_init(&a, "/tmp/test"), HU_OK);
}

static void agent_git_snapshot_test_mode(void) {
    hu_allocator_t a = hu_system_allocator();
    HU_ASSERT_EQ(hu_agent_git_snapshot(&a, "/tmp/test", "snap"), HU_OK);
}

static void agent_git_diff_test_mode(void) {
    hu_allocator_t a = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_agent_git_diff(&a, "/tmp/test", "HEAD~1", "HEAD", &out, &len), HU_OK);
}

static void agent_git_log_test_mode(void) {
    hu_allocator_t a = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_agent_git_log(&a, "/tmp/test", 5, &out, &len), HU_OK);
}

static void agent_git_ref_validation(void) {
    hu_allocator_t a = hu_system_allocator();
    HU_ASSERT_EQ(hu_agent_git_rollback(&a, "/tmp", "HEAD;rm -rf /"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_agent_git_branch(&a, "/tmp", "feat$(evil)"), HU_ERR_INVALID_ARGUMENT);
}

void run_agent_git_tests(void) {
    HU_TEST_SUITE("AgentGit");
    HU_RUN_TEST(agent_git_init_null_args);
    HU_RUN_TEST(agent_git_snapshot_null_args);
    HU_RUN_TEST(agent_git_rollback_null_args);
    HU_RUN_TEST(agent_git_diff_null_args);
    HU_RUN_TEST(agent_git_branch_null_args);
    HU_RUN_TEST(agent_git_log_null_args);
    HU_RUN_TEST(agent_git_init_test_mode);
    HU_RUN_TEST(agent_git_snapshot_test_mode);
    HU_RUN_TEST(agent_git_diff_test_mode);
    HU_RUN_TEST(agent_git_log_test_mode);
    HU_RUN_TEST(agent_git_ref_validation);
}
