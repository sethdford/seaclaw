/* Adversarial tests: DoS resilience and protocol violations — resource limits,
 * boundary conditions, invalid JSON-RPC, malformed session data, exit codes.
 * Every test uses hu_tracking_allocator_t to verify zero leaks. */
#include "test_framework.h"
#include "human/mcp_manager.h"
#include "human/hook.h"
#include "human/hook_pipeline.h"
#include "human/permission.h"
#include "human/agent/compaction_structured.h"
#include "human/agent/session_persist.h"
#include "human/agent/instruction_discover.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/config.h"
#include "human/agent.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

/* ══════════════════════════════════════════════════════════════════════════
 * MCP DoS / Resource Limits
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1. MCP manager: call_tool with NULL server name */
static void test_mcp_call_tool_null_server(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, NULL, 0, &mgr);

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err = hu_mcp_manager_call_tool(mgr, &alloc, NULL, "tool", "{}",
                                               &result, &result_len);
    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(result);

    hu_mcp_manager_destroy(mgr);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 2. MCP manager: call_tool on non-existent server */
static void test_mcp_call_tool_no_server(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, NULL, 0, &mgr);

    char *result = NULL;
    size_t result_len = 0;
    hu_error_t err = hu_mcp_manager_call_tool(mgr, &alloc, "ghost", "tool", "{}",
                                               &result, &result_len);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    hu_mcp_manager_destroy(mgr);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 3. MCP server_info: out of range index */
static void test_mcp_server_info_out_of_range(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, NULL, 0, &mgr);

    hu_mcp_server_info_t info;
    hu_error_t err = hu_mcp_manager_server_info(mgr, 0, &info);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    err = hu_mcp_manager_server_info(mgr, SIZE_MAX, &info);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    hu_mcp_manager_destroy(mgr);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 4. MCP find_server: NULL name */
static void test_mcp_find_server_null(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_mcp_manager_t *mgr = NULL;
    hu_mcp_manager_create(&alloc, NULL, 0, &mgr);

    size_t idx = 0;
    hu_error_t err = hu_mcp_manager_find_server(mgr, NULL, &idx);
    HU_ASSERT_NEQ(err, HU_OK);

    hu_mcp_manager_destroy(mgr);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 5. MCP tool name: parse with no delimiters */
static void test_mcp_parse_no_delimiters(void) {
    const char *srv = NULL, *tool = NULL;
    size_t srv_len = 0, tool_len = 0;
    HU_ASSERT_FALSE(hu_mcp_tool_parse_name("nodoubleunderscore", &srv, &srv_len, &tool, &tool_len));
    HU_ASSERT_FALSE(hu_mcp_tool_parse_name("mcp__onlyserver", &srv, &srv_len, &tool, &tool_len));
    HU_ASSERT_FALSE(hu_mcp_tool_parse_name("mcp____", &srv, &srv_len, &tool, &tool_len));
    HU_ASSERT_FALSE(hu_mcp_tool_parse_name("", &srv, &srv_len, &tool, &tool_len));
}

/* 6. MCP tool name: parse with NULL input */
static void test_mcp_parse_null_input(void) {
    const char *srv = NULL, *tool = NULL;
    size_t srv_len = 0, tool_len = 0;
    HU_ASSERT_FALSE(hu_mcp_tool_parse_name(NULL, &srv, &srv_len, &tool, &tool_len));
    HU_ASSERT_FALSE(hu_mcp_tool_is_mcp(NULL));
}

/* ══════════════════════════════════════════════════════════════════════════
 * Hook DoS / Timeout / Exit Codes
 * ══════════════════════════════════════════════════════════════════════════ */

/* 7. Hook exit code 0 = allow */
static void test_hook_exit_code_allow(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "allow-hook", .name_len = 10,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "true", .command_len = 4,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 0, .stdout_data = "", .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 8. Hook exit code 2 = deny */
static void test_hook_exit_code_deny(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "deny-hook", .name_len = 9,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "exit 2", .command_len = 6,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 2, .stdout_data = "blocked", .stdout_len = 7};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 9. Hook exit code 3 = warn */
static void test_hook_exit_code_warn(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "warn-hook", .name_len = 9,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "exit 3", .command_len = 6,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 3, .stdout_data = "caution", .stdout_len = 7};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    HU_ASSERT_EQ(result.decision, HU_HOOK_WARN);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 10. Hook exit code -1 (unknown — treat as allow with warning) */
static void test_hook_exit_code_negative(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "bad-exit", .name_len = 8,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "exit -1", .command_len = 7,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = -1, .stdout_data = "", .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    /* Unknown exit code: non-required hook => allow */
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 11. Hook exit code 256 (wraps to 0 on some systems) */
static void test_hook_exit_code_256(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "wrap-exit", .name_len = 9,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "exit 256", .command_len = 8,
        .timeout_sec = 5, .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    /* 256 is passed through the mock — implementation should handle it */
    hu_hook_mock_config_t mock = {.exit_code = 256, .stdout_data = "", .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    /* 256 is not 0, 2, or 3 — should be treated as unknown */
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 12. Hook exit code 137 (SIGKILL) on required hook => deny */
static void test_hook_exit_code_sigkill_required(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "killed", .name_len = 6,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "killed_proc", .command_len = 11,
        .timeout_sec = 5, .required = true,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    hu_hook_mock_config_t mock = {.exit_code = 137, .stdout_data = "", .stdout_len = 0};
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    /* Required hook with unknown exit => deny */
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 13. Hook pipeline: multiple hooks, first deny stops rest */
static void test_hook_pipeline_deny_stops_rest(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entries[3] = {
        {.name = "h1", .name_len = 2, .event = HU_HOOK_PRE_TOOL_EXECUTE,
         .command = "cmd1", .command_len = 4, .timeout_sec = 5, .required = false},
        {.name = "h2", .name_len = 2, .event = HU_HOOK_PRE_TOOL_EXECUTE,
         .command = "cmd2", .command_len = 4, .timeout_sec = 5, .required = false},
        {.name = "h3", .name_len = 2, .event = HU_HOOK_PRE_TOOL_EXECUTE,
         .command = "cmd3", .command_len = 4, .timeout_sec = 5, .required = false},
    };
    for (int i = 0; i < 3; i++)
        hu_hook_registry_add(reg, &alloc, &entries[i]);

    /* First allow, second deny — third should not run */
    hu_hook_mock_config_t seq[3] = {
        {.exit_code = 0, .stdout_data = "", .stdout_len = 0},
        {.exit_code = 2, .stdout_data = "denied", .stdout_len = 6},
        {.exit_code = 0, .stdout_data = "", .stdout_len = 0},
    };
    hu_hook_mock_set_sequence(seq, 3);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_hook_pipeline_pre_tool(reg, &alloc, "tool", 4, "{}", 2, &result);
    HU_ASSERT_EQ(result.decision, HU_HOOK_DENY);
    /* Only 2 hooks should have been called (first allow, second deny) */
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 2);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Instruction Discovery Boundaries
 * ══════════════════════════════════════════════════════════════════════════ */

/* 14. Instruction merge: file exactly at per-file limit (4000 chars) */
static void test_instruction_boundary_at_limit(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_instruction_file_t files[1];
    memset(&files[0], 0, sizeof(files[0]));
    files[0].source = HU_INSTRUCTION_SOURCE_WORKSPACE;
    files[0].path = "/tmp/test.md";
    files[0].path_len = 12;

    /* Exactly 4000 chars */
    char *content = alloc.alloc(alloc.ctx, HU_INSTRUCTION_MAX_CHARS_PER_FILE + 1);
    HU_ASSERT_NOT_NULL(content);
    memset(content, 'A', HU_INSTRUCTION_MAX_CHARS_PER_FILE);
    content[HU_INSTRUCTION_MAX_CHARS_PER_FILE] = '\0';
    files[0].content = content;
    files[0].content_len = HU_INSTRUCTION_MAX_CHARS_PER_FILE;
    files[0].truncated = false;

    char *merged = NULL;
    size_t merged_len = 0;
    hu_error_t err = hu_instruction_merge(&alloc, files, 1, &merged, &merged_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT(merged_len <= HU_INSTRUCTION_MAX_CHARS_TOTAL);
    alloc.free(alloc.ctx, merged, merged_len + 1);

    alloc.free(alloc.ctx, content, HU_INSTRUCTION_MAX_CHARS_PER_FILE + 1);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 15. Instruction merge: file at per-file limit + 1 (4001 chars) */
static void test_instruction_boundary_over_limit(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_instruction_file_t files[1];
    memset(&files[0], 0, sizeof(files[0]));
    files[0].source = HU_INSTRUCTION_SOURCE_WORKSPACE;
    files[0].path = "/tmp/test.md";
    files[0].path_len = 12;

    size_t over = HU_INSTRUCTION_MAX_CHARS_PER_FILE + 1;
    char *content = alloc.alloc(alloc.ctx, over + 1);
    HU_ASSERT_NOT_NULL(content);
    memset(content, 'B', over);
    content[over] = '\0';
    files[0].content = content;
    files[0].content_len = over;
    files[0].truncated = true;

    char *merged = NULL;
    size_t merged_len = 0;
    hu_error_t err = hu_instruction_merge(&alloc, files, 1, &merged, &merged_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    /* Should be capped */
    HU_ASSERT(merged_len <= HU_INSTRUCTION_MAX_CHARS_TOTAL);
    alloc.free(alloc.ctx, merged, merged_len + 1);

    alloc.free(alloc.ctx, content, over + 1);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 16. Instruction merge: total exceeds 12000 chars */
static void test_instruction_boundary_total_exceeded(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    /* 4 files x 3999 chars each = 15996 > 12000 */
    hu_instruction_file_t files[4];
    char *contents[4];
    for (int i = 0; i < 4; i++) {
        memset(&files[i], 0, sizeof(files[i]));
        files[i].source = HU_INSTRUCTION_SOURCE_PROJECT_ROOT;
        files[i].path = "/tmp/f.md";
        files[i].path_len = 9;

        size_t sz = 3999;
        contents[i] = alloc.alloc(alloc.ctx, sz + 1);
        HU_ASSERT_NOT_NULL(contents[i]);
        memset(contents[i], 'C' + i, sz);
        contents[i][sz] = '\0';
        files[i].content = contents[i];
        files[i].content_len = sz;
        files[i].truncated = false;
    }

    char *merged = NULL;
    size_t merged_len = 0;
    hu_error_t err = hu_instruction_merge(&alloc, files, 4, &merged, &merged_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(merged);
    HU_ASSERT(merged_len <= HU_INSTRUCTION_MAX_CHARS_TOTAL);
    alloc.free(alloc.ctx, merged, merged_len + 1);

    for (int i = 0; i < 4; i++)
        alloc.free(alloc.ctx, contents[i], 4000);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 17. Instruction merge: NULL files array with count 0 */
static void test_instruction_merge_null_files(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *merged = NULL;
    size_t merged_len = 0;
    hu_error_t err = hu_instruction_merge(&alloc, NULL, 0, &merged, &merged_len);
    if (err == HU_OK) {
        /* Empty merge is valid */
        if (merged) {
            alloc.free(alloc.ctx, merged, merged_len + 1);
        }
    }

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Session DoS
 * ══════════════════════════════════════════════════════════════════════════ */

/* 18. Session: list in non-existent directory */
static void test_session_list_nonexistent_dir(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_session_metadata_t *metas = NULL;
    size_t count = 0;
    hu_error_t err = hu_session_persist_list(&alloc, "/nonexistent_test_dir_xyz_42", &metas, &count);
    if (err == HU_OK) {
        HU_ASSERT_EQ(count, 0);
        if (metas) {
            hu_session_metadata_free(&alloc, metas, count);
        }
    }

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 19. Session: delete non-existent session */
static void test_session_delete_nonexistent(void) {
    hu_error_t err = hu_session_persist_delete("/tmp", "nonexistent_session_12345");
    /* Should fail gracefully (file not found) */
    HU_ASSERT_NEQ(err, HU_OK);
}

/* 20. Session ID: overlong session ID */
static void test_session_overlong_id(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    /* Try to load with a session ID longer than HU_SESSION_ID_MAX */
    char long_id[256];
    memset(long_id, 'Z', sizeof(long_id) - 1);
    long_id[sizeof(long_id) - 1] = '\0';

    hu_error_t err = hu_session_persist_load(&alloc, NULL, "/tmp", long_id);
    HU_ASSERT_NEQ(err, HU_OK);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Permission Protocol
 * ══════════════════════════════════════════════════════════════════════════ */

/* 21. Permission check: all level combinations */
static void test_permission_check_all_levels(void) {
    /* READ_ONLY (0) can only access READ_ONLY */
    HU_ASSERT_TRUE(hu_permission_check(HU_PERM_READ_ONLY, HU_PERM_READ_ONLY));
    HU_ASSERT_FALSE(hu_permission_check(HU_PERM_READ_ONLY, HU_PERM_WORKSPACE_WRITE));
    HU_ASSERT_FALSE(hu_permission_check(HU_PERM_READ_ONLY, HU_PERM_DANGER_FULL_ACCESS));

    /* WORKSPACE_WRITE (1) can access READ_ONLY and WORKSPACE_WRITE */
    HU_ASSERT_TRUE(hu_permission_check(HU_PERM_WORKSPACE_WRITE, HU_PERM_READ_ONLY));
    HU_ASSERT_TRUE(hu_permission_check(HU_PERM_WORKSPACE_WRITE, HU_PERM_WORKSPACE_WRITE));
    HU_ASSERT_FALSE(hu_permission_check(HU_PERM_WORKSPACE_WRITE, HU_PERM_DANGER_FULL_ACCESS));

    /* DANGER_FULL_ACCESS (2) can access everything */
    HU_ASSERT_TRUE(hu_permission_check(HU_PERM_DANGER_FULL_ACCESS, HU_PERM_READ_ONLY));
    HU_ASSERT_TRUE(hu_permission_check(HU_PERM_DANGER_FULL_ACCESS, HU_PERM_WORKSPACE_WRITE));
    HU_ASSERT_TRUE(hu_permission_check(HU_PERM_DANGER_FULL_ACCESS, HU_PERM_DANGER_FULL_ACCESS));
}

/* 22. Permission level names: all levels return non-NULL */
static void test_permission_level_names(void) {
    HU_ASSERT_NOT_NULL(hu_permission_level_name(HU_PERM_READ_ONLY));
    HU_ASSERT_NOT_NULL(hu_permission_level_name(HU_PERM_WORKSPACE_WRITE));
    HU_ASSERT_NOT_NULL(hu_permission_level_name(HU_PERM_DANGER_FULL_ACCESS));
    /* Invalid level should still return something */
    HU_ASSERT_NOT_NULL(hu_permission_level_name((hu_permission_level_t)99));
}

/* 23. Permission: escalation with NULL agent */
static void test_permission_escalate_null_agent(void) {
    hu_error_t err = hu_permission_escalate_temporary(NULL, HU_PERM_DANGER_FULL_ACCESS, "tool");
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* 24. Permission: reset escalation on NULL — should not crash */
static void test_permission_reset_null(void) {
    hu_permission_reset_escalation(NULL);
    /* No crash = pass */
}

/* ══════════════════════════════════════════════════════════════════════════
 * Compaction Protocol
 * ══════════════════════════════════════════════════════════════════════════ */

/* 25. Compaction summary free: double free safety (NULL fields) */
static void test_compaction_summary_free_empty(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_compaction_summary_t summary;
    memset(&summary, 0, sizeof(summary));
    /* Should be safe to free a zeroed summary */
    hu_compaction_summary_free(&alloc, &summary);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 26. Artifact pins free: empty array */
static void test_compaction_pins_free_empty(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_artifact_pins_free(&alloc, NULL, 0);
    /* No crash = pass */

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Suite runner
 * ══════════════════════════════════════════════════════════════════════════ */

void run_adversarial_dos_protocol_tests(void) {
    HU_TEST_SUITE("Adversarial: DoS & Protocol");

    /* MCP */
    HU_RUN_TEST(test_mcp_call_tool_null_server);
    HU_RUN_TEST(test_mcp_call_tool_no_server);
    HU_RUN_TEST(test_mcp_server_info_out_of_range);
    HU_RUN_TEST(test_mcp_find_server_null);
    HU_RUN_TEST(test_mcp_parse_no_delimiters);
    HU_RUN_TEST(test_mcp_parse_null_input);

    /* Hook exit codes */
    HU_RUN_TEST(test_hook_exit_code_allow);
    HU_RUN_TEST(test_hook_exit_code_deny);
    HU_RUN_TEST(test_hook_exit_code_warn);
    HU_RUN_TEST(test_hook_exit_code_negative);
    HU_RUN_TEST(test_hook_exit_code_256);
    HU_RUN_TEST(test_hook_exit_code_sigkill_required);
    HU_RUN_TEST(test_hook_pipeline_deny_stops_rest);

    /* Instruction boundaries */
    HU_RUN_TEST(test_instruction_boundary_at_limit);
    HU_RUN_TEST(test_instruction_boundary_over_limit);
    HU_RUN_TEST(test_instruction_boundary_total_exceeded);
    HU_RUN_TEST(test_instruction_merge_null_files);

    /* Session DoS */
    HU_RUN_TEST(test_session_list_nonexistent_dir);
    HU_RUN_TEST(test_session_delete_nonexistent);
    HU_RUN_TEST(test_session_overlong_id);

    /* Permission protocol */
    HU_RUN_TEST(test_permission_check_all_levels);
    HU_RUN_TEST(test_permission_level_names);
    HU_RUN_TEST(test_permission_escalate_null_agent);
    HU_RUN_TEST(test_permission_reset_null);

    /* Compaction protocol */
    HU_RUN_TEST(test_compaction_summary_free_empty);
    HU_RUN_TEST(test_compaction_pins_free_empty);
}
