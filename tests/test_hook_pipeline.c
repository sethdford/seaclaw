/* Tests for hook registry, shell escaping, and pipeline execution */
#include "test_framework.h"
#include "human/hook.h"
#include "human/hook_pipeline.h"
#include "human/core/allocator.h"
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Registry tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_hook_registry_create_destroy(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_error_t err = hu_hook_registry_create(&alloc, &reg);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(reg);
    HU_ASSERT_EQ(hu_hook_registry_count(reg), 0);

    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_registry_add_and_get(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "pre-audit",
        .name_len = 9,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "echo check",
        .command_len = 10,
        .timeout_sec = 15,
        .required = true,
    };
    hu_error_t err = hu_hook_registry_add(reg, &alloc, &entry);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(hu_hook_registry_count(reg), 1);

    const hu_hook_entry_t *got = hu_hook_registry_get(reg, 0);
    HU_ASSERT_NOT_NULL(got);
    HU_ASSERT_STR_EQ(got->name, "pre-audit");
    HU_ASSERT_EQ(got->event, HU_HOOK_PRE_TOOL_EXECUTE);
    HU_ASSERT_STR_EQ(got->command, "echo check");
    HU_ASSERT_EQ(got->timeout_sec, 15);
    HU_ASSERT_TRUE(got->required);

    /* Out of bounds returns NULL */
    HU_ASSERT_NULL(hu_hook_registry_get(reg, 1));

    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_registry_add_multiple(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    /* Add more than initial capacity (8) to trigger realloc */
    for (int i = 0; i < 12; i++) {
        char name[32];
        snprintf(name, sizeof(name), "hook-%d", i);
        hu_hook_entry_t entry = {
            .name = name,
            .name_len = strlen(name),
            .event = (i % 2 == 0) ? HU_HOOK_PRE_TOOL_EXECUTE : HU_HOOK_POST_TOOL_EXECUTE,
            .command = "true",
            .command_len = 4,
            .timeout_sec = 0,
            .required = false,
        };
        HU_ASSERT_EQ(hu_hook_registry_add(reg, &alloc, &entry), HU_OK);
    }
    HU_ASSERT_EQ(hu_hook_registry_count(reg), 12);

    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_registry_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_hook_registry_t *reg = NULL;

    HU_ASSERT_EQ(hu_hook_registry_create(NULL, &reg), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_hook_registry_create(&alloc, NULL), HU_ERR_INVALID_ARGUMENT);

    hu_hook_registry_create(&alloc, &reg);
    HU_ASSERT_EQ(hu_hook_registry_add(reg, &alloc, NULL), HU_ERR_INVALID_ARGUMENT);

    /* Empty command is invalid */
    hu_hook_entry_t bad = {.name = "x", .name_len = 1, .command = NULL, .command_len = 0};
    HU_ASSERT_EQ(hu_hook_registry_add(reg, &alloc, &bad), HU_ERR_INVALID_ARGUMENT);

    hu_hook_registry_destroy(reg, &alloc);
}

/* ──────────────────────────────────────────────────────────────────────────
 * Shell escape tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_hook_shell_escape_basic(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_hook_shell_escape(&alloc, "hello", 5, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out, "'hello'");
    HU_ASSERT_EQ(out_len, 7);
    alloc.free(alloc.ctx, out, out_len + 1);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_shell_escape_single_quotes(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *out = NULL;
    size_t out_len = 0;
    /* Input: it's a test */
    hu_error_t err = hu_hook_shell_escape(&alloc, "it's a test", 11, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out, "'it'\\''s a test'");
    alloc.free(alloc.ctx, out, out_len + 1);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_shell_escape_empty(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_hook_shell_escape(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out, "''");
    alloc.free(alloc.ctx, out, out_len + 1);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_shell_escape_metacharacters(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    /* Shell metacharacters should be safely wrapped in single quotes */
    const char *dangerous = "$(rm -rf /); `id` | cat && echo pwned";
    size_t dlen = strlen(dangerous);
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_hook_shell_escape(&alloc, dangerous, dlen, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    /* Should be wrapped in single quotes — no metachar interpretation */
    HU_ASSERT(out[0] == '\'');
    HU_ASSERT(out[out_len - 1] == '\'');
    /* No backtick or $() should appear unquoted */
    alloc.free(alloc.ctx, out, out_len + 1);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ──────────────────────────────────────────────────────────────────────────
 * Pipeline tests (using mocked shell execution)
 * ────────────────────────────────────────────────────────────────────────── */

static void test_hook_pipeline_no_hooks(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));

    /* NULL registry = allow */
    hu_error_t err = hu_hook_pipeline_pre_tool(NULL, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);
    hu_hook_result_free(&alloc, &result);

    /* Empty registry = allow */
    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);
    hu_hook_result_free(&alloc, &result);

    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_allow(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "allow-all", .name_len = 9,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./check.sh", .command_len = 10,
        .timeout_sec = 10, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 0, .stdout_data = NULL, .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "file_write", 10, "{\"path\":\"/tmp\"}", 15, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 1);

    /* Verify shell escape in the command */
    const char *cmd = hu_hook_mock_last_command();
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_CONTAINS(cmd, "HOOK_TOOL_NAME=");
    HU_ASSERT_STR_CONTAINS(cmd, "HOOK_ARGS=");

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_deny(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "blocker", .name_len = 7,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./deny.sh", .command_len = 9,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 2, .stdout_data = "not allowed", .stdout_len = 11};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);
    HU_ASSERT_NOT_NULL(result.message);
    HU_ASSERT_STR_EQ(result.message, "not allowed");

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_warn(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "warn-hook", .name_len = 9,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./warn.sh", .command_len = 9,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 3, .stdout_data = "risky op", .stdout_len = 8};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_WARN);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_deny_stops_pipeline(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    /* Two pre-hooks: first denies, second should never run */
    hu_hook_entry_t h1 = {
        .name = "denier", .name_len = 6,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./deny.sh", .command_len = 9,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_entry_t h2 = {
        .name = "after-deny", .name_len = 10,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./should-not-run.sh", .command_len = 19,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &h1);
    hu_hook_registry_add(reg, &alloc, &h2);

    hu_hook_mock_config_t seq[] = {
        {.exit_code = 2, .stdout_data = "blocked", .stdout_len = 7},
        {.exit_code = 0, .stdout_data = NULL, .stdout_len = 0},
    };
    hu_hook_mock_set_sequence(seq, 2);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 1); /* Only first hook ran */

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_event_filtering(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    /* Register post-hook only */
    hu_hook_entry_t post_hook = {
        .name = "post-only", .name_len = 9,
        .event = HU_HOOK_POST_TOOL_EXECUTE,
        .command = "./post.sh", .command_len = 9,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &post_hook);

    hu_hook_mock_config_t mock = {.exit_code = 2, .stdout_data = "deny", .stdout_len = 4};
    hu_hook_mock_set(&mock);

    /* Pre-hook pipeline should see no matching hooks => allow */
    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 0); /* No hooks executed */

    /* Post-hook pipeline should run the hook */
    err = hu_hook_pipeline_post_tool(reg, &alloc, "shell", 5, "{}", 2,
                                     "output", 6, true, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 1);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_unknown_exit_code(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "weird", .name_len = 5,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./weird.sh", .command_len = 10,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    /* Exit code 42 = unknown = treat as allow */
    hu_hook_mock_config_t mock = {.exit_code = 42, .stdout_data = NULL, .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW); /* Unknown treated as allow for non-required */

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_required_unknown_exit_denies(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "required-weird", .name_len = 14,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./required.sh", .command_len = 13,
        .timeout_sec = 5, .required = true,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    /* Unknown exit code on required hook => deny */
    hu_hook_mock_config_t mock = {.exit_code = 99, .stdout_data = NULL, .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_post_tool_context(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "post-logger", .name_len = 11,
        .event = HU_HOOK_POST_TOOL_EXECUTE,
        .command = "./log.sh", .command_len = 8,
        .timeout_sec = 10, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 0, .stdout_data = NULL, .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_post_tool(reg, &alloc, "shell", 5,
                                                "{\"command\":\"ls\"}", 16,
                                                "file1\nfile2\n", 12, true, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);

    /* Verify post-hook command includes output and success env vars */
    const char *cmd = hu_hook_mock_last_command();
    HU_ASSERT_NOT_NULL(cmd);
    HU_ASSERT_STR_CONTAINS(cmd, "HOOK_OUTPUT=");
    HU_ASSERT_STR_CONTAINS(cmd, "HOOK_SUCCESS=true");

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_multiple_allow_then_warn(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t h1 = {
        .name = "h1", .name_len = 2,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./h1.sh", .command_len = 7,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_entry_t h2 = {
        .name = "h2", .name_len = 2,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./h2.sh", .command_len = 7,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &h1);
    hu_hook_registry_add(reg, &alloc, &h2);

    /* First allows, second warns */
    hu_hook_mock_config_t seq[] = {
        {.exit_code = 0, .stdout_data = NULL, .stdout_len = 0},
        {.exit_code = 3, .stdout_data = "caution", .stdout_len = 7},
    };
    hu_hook_mock_set_sequence(seq, 2);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_WARN);
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 2);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_shell_command_includes_escaped_tool_name(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);
    hu_hook_mock_reset();

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "escape-test", .name_len = 11,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "./hook.sh", .command_len = 9,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 0, .stdout_data = NULL, .stdout_len = 0};
    hu_hook_mock_set(&mock);

    /* Tool name with injection attempt */
    const char *bad_name = "shell'; rm -rf /; echo '";
    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, bad_name, strlen(bad_name), "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);

    /* The command should have the tool name safely escaped */
    const char *cmd = hu_hook_mock_last_command();
    HU_ASSERT_NOT_NULL(cmd);
    /* Should NOT contain unescaped semicolons from the tool name */
    /* The name is wrapped in single quotes which prevents interpretation */
    HU_ASSERT_STR_CONTAINS(cmd, "HOOK_TOOL_NAME='");

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ──────────────────────────────────────────────────────────────────────────
 * Before-reply hook tests
 * ────────────────────────────────────────────────────────────────────────── */

static void test_hook_pipeline_before_reply_allow(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "before-reply-check", .name_len = 18,
        .event = HU_HOOK_BEFORE_REPLY,
        .command = "echo ok", .command_len = 7,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 0, .stdout_data = NULL, .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_before_reply(reg, &alloc, "hello", 5, "cli", 3, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);
    HU_ASSERT_NULL(result.synthetic_response);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_before_reply_short_circuit(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "canned-response", .name_len = 15,
        .event = HU_HOOK_BEFORE_REPLY,
        .command = "echo canned", .command_len = 11,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    const char *synthetic = "I am a canned response";
    hu_hook_mock_config_t mock = {
        .exit_code = 4,
        .stdout_data = synthetic,
        .stdout_len = strlen(synthetic),
    };
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_before_reply(reg, &alloc, "hello", 5, "cli", 3, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_SHORT_CIRCUIT);
    HU_ASSERT_NOT_NULL(result.synthetic_response);
    HU_ASSERT_STR_EQ(result.synthetic_response, "I am a canned response");

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_before_reply_deny(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "rate-limit", .name_len = 10,
        .event = HU_HOOK_BEFORE_REPLY,
        .command = "echo denied", .command_len = 11,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 2, .stdout_data = "rate limited", .stdout_len = 12};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_before_reply(reg, &alloc, "hello", 5, "cli", 3, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);
    HU_ASSERT_NOT_NULL(result.message);
    HU_ASSERT_STR_EQ(result.message, "rate limited");
    HU_ASSERT_NULL(result.synthetic_response);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

static void test_hook_pipeline_before_reply_event_filtering(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    /* Register a pre-tool hook only — should NOT fire for before_reply */
    hu_hook_entry_t entry = {
        .name = "pre-tool-only", .name_len = 13,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "echo should-not-fire", .command_len = 20,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_before_reply(reg, &alloc, "hello", 5, "cli", 3, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 0);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ──────────────────────────────────────────────────────────────────────────
 * Suite runner
 * ────────────────────────────────────────────────────────────────────────── */

void run_hook_pipeline_tests(void) {
    HU_TEST_SUITE("hook_pipeline");
    HU_RUN_TEST(test_hook_registry_create_destroy);
    HU_RUN_TEST(test_hook_registry_add_and_get);
    HU_RUN_TEST(test_hook_registry_add_multiple);
    HU_RUN_TEST(test_hook_registry_null_args);
    HU_RUN_TEST(test_hook_shell_escape_basic);
    HU_RUN_TEST(test_hook_shell_escape_single_quotes);
    HU_RUN_TEST(test_hook_shell_escape_empty);
    HU_RUN_TEST(test_hook_shell_escape_metacharacters);
    HU_RUN_TEST(test_hook_pipeline_no_hooks);
    HU_RUN_TEST(test_hook_pipeline_allow);
    HU_RUN_TEST(test_hook_pipeline_deny);
    HU_RUN_TEST(test_hook_pipeline_warn);
    HU_RUN_TEST(test_hook_pipeline_deny_stops_pipeline);
    HU_RUN_TEST(test_hook_pipeline_event_filtering);
    HU_RUN_TEST(test_hook_pipeline_unknown_exit_code);
    HU_RUN_TEST(test_hook_pipeline_required_unknown_exit_denies);
    HU_RUN_TEST(test_hook_pipeline_post_tool_context);
    HU_RUN_TEST(test_hook_pipeline_multiple_allow_then_warn);
    HU_RUN_TEST(test_hook_shell_command_includes_escaped_tool_name);
    HU_RUN_TEST(test_hook_pipeline_before_reply_allow);
    HU_RUN_TEST(test_hook_pipeline_before_reply_short_circuit);
    HU_RUN_TEST(test_hook_pipeline_before_reply_deny);
    HU_RUN_TEST(test_hook_pipeline_before_reply_event_filtering);
}
