#include "test_framework.h"
#include "human/tools/code_sandbox.h"
#include <string.h>

static void sandbox_config_defaults(void) {
    hu_code_sandbox_config_t cfg = hu_code_sandbox_config_default();
    HU_ASSERT_EQ(cfg.timeout_ms, 10000);
    HU_ASSERT_EQ(cfg.memory_limit_mb, 256u);
    HU_ASSERT(!cfg.allow_network);
}

static void sandbox_execute_python(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_code_sandbox_config_t config = hu_code_sandbox_config_default();
    config.language = HU_SANDBOX_PYTHON;
    hu_code_sandbox_result_t result;
    memset(&result, 0, sizeof(result));
    const char *code = "print('hello')";
    HU_ASSERT_EQ(hu_code_sandbox_execute(&alloc, &config, code, strlen(code), &result), HU_OK);
    HU_ASSERT(strstr(result.stdout_buf, "Hello") != NULL);
    HU_ASSERT_EQ(result.exit_code, 0);
}

static void sandbox_execute_javascript(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_code_sandbox_config_t config = hu_code_sandbox_config_default();
    config.language = HU_SANDBOX_JAVASCRIPT;
    hu_code_sandbox_result_t result;
    memset(&result, 0, sizeof(result));
    const char *code = "console.log(42)";
    HU_ASSERT_EQ(hu_code_sandbox_execute(&alloc, &config, code, strlen(code), &result), HU_OK);
    HU_ASSERT(strstr(result.stdout_buf, "42") != NULL);
    HU_ASSERT_EQ(result.exit_code, 0);
}

static void sandbox_execute_shell(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_code_sandbox_config_t config = hu_code_sandbox_config_default();
    config.language = HU_SANDBOX_SHELL;
    hu_code_sandbox_result_t result;
    memset(&result, 0, sizeof(result));
    const char *code = "ls";
    HU_ASSERT_EQ(hu_code_sandbox_execute(&alloc, &config, code, strlen(code), &result), HU_OK);
    HU_ASSERT(result.stdout_len > 0);
    HU_ASSERT_EQ(result.exit_code, 0);
}

static void sandbox_language_name(void) {
    HU_ASSERT_STR_EQ(hu_sandbox_language_name(HU_SANDBOX_PYTHON), "python");
    HU_ASSERT_STR_EQ(hu_sandbox_language_name(HU_SANDBOX_JAVASCRIPT), "javascript");
    HU_ASSERT_STR_EQ(hu_sandbox_language_name(HU_SANDBOX_SHELL), "shell");
}

static void sandbox_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_code_sandbox_config_t config = hu_code_sandbox_config_default();
    hu_code_sandbox_result_t result;
    memset(&result, 0, sizeof(result));
    const char *code = "1+1";
    HU_ASSERT_NEQ(hu_code_sandbox_execute(NULL, &config, code, 3, &result), HU_OK);
    HU_ASSERT_NEQ(hu_code_sandbox_execute(&alloc, NULL, code, 3, &result), HU_OK);
    HU_ASSERT_NEQ(hu_code_sandbox_execute(&alloc, &config, NULL, 0, &result), HU_OK);
    HU_ASSERT_NEQ(hu_code_sandbox_execute(&alloc, &config, code, 3, NULL), HU_OK);
}

void run_code_sandbox_tests(void) {
    HU_TEST_SUITE("Code Sandbox");
    HU_RUN_TEST(sandbox_config_defaults);
    HU_RUN_TEST(sandbox_execute_python);
    HU_RUN_TEST(sandbox_execute_javascript);
    HU_RUN_TEST(sandbox_execute_shell);
    HU_RUN_TEST(sandbox_language_name);
    HU_RUN_TEST(sandbox_null_args_returns_error);
}
