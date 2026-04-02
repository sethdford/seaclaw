/* Adversarial tests: memory safety — buffer overflows, use-after-free,
 * integer overflow, corrupted input, massive allocations.
 * Every test uses hu_tracking_allocator_t to verify zero leaks. */
#include "test_framework.h"
#include "human/mcp_manager.h"
#include "human/hook.h"
#include "human/hook_pipeline.h"
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

/* ══════════════════════════════════════════════════════════════════════════
 * MCP Memory Safety
 * ══════════════════════════════════════════════════════════════════════════ */

/* 1. 8KB method name should not overflow any fixed-size buffer */
static void test_mcp_huge_method_name(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    /* Build an 8KB tool name */
    size_t name_len = 8192;
    char *huge_name = alloc.alloc(alloc.ctx, name_len + 1);
    HU_ASSERT_NOT_NULL(huge_name);
    memset(huge_name, 'A', name_len);
    huge_name[name_len] = '\0';

    /* mcp_tool_parse_name should handle gracefully */
    bool is_mcp = hu_mcp_tool_is_mcp(huge_name);
    HU_ASSERT_FALSE(is_mcp);

    const char *srv = NULL, *tool = NULL;
    size_t srv_len = 0, tool_len = 0;
    bool parsed = hu_mcp_tool_parse_name(huge_name, &srv, &srv_len, &tool, &tool_len);
    HU_ASSERT_FALSE(parsed);

    /* Build name with huge server component */
    char *built = NULL;
    size_t built_len = 0;
    hu_error_t err = hu_mcp_tool_build_name(&alloc, huge_name, "tool", &built, &built_len);
    /* Should succeed (just a long string) or fail gracefully */
    if (err == HU_OK) {
        HU_ASSERT_NOT_NULL(built);
        HU_ASSERT_GT(built_len, name_len);
        alloc.free(alloc.ctx, built, built_len + 1);
    }

    alloc.free(alloc.ctx, huge_name, name_len + 1);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 2. Null bytes embedded in tool name strings */
static void test_mcp_null_bytes_in_strings(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    /* Tool name with embedded null: "mcp__sv\0r__tool" */
    char name_with_null[] = "mcp__sv\0r__tool";
    /* strlen will see "mcp__sv" — parse should fail for the short form */
    const char *srv = NULL, *tool = NULL;
    size_t srv_len = 0, tool_len = 0;
    bool parsed = hu_mcp_tool_parse_name(name_with_null, &srv, &srv_len, &tool, &tool_len);
    /* The string appears truncated at null byte — likely no second __ delimiter */
    HU_ASSERT_FALSE(parsed);

    /* Build name with null byte in server name */
    char *built = NULL;
    size_t built_len = 0;
    hu_error_t err = hu_mcp_tool_build_name(&alloc, "sv\0r", "tool", &built, &built_len);
    /* Treats as "sv" due to C string semantics */
    if (err == HU_OK) {
        HU_ASSERT_NOT_NULL(built);
        alloc.free(alloc.ctx, built, built_len + 1);
    }

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 3. MCP manager: create with maximum servers then destroy — no leaks */
static void test_mcp_max_servers_no_leak(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_mcp_server_entry_t entries[HU_MCP_MANAGER_MAX_SERVERS];
    char names[HU_MCP_MANAGER_MAX_SERVERS][32];
    for (size_t i = 0; i < HU_MCP_MANAGER_MAX_SERVERS; i++) {
        memset(&entries[i], 0, sizeof(entries[i]));
        snprintf(names[i], sizeof(names[i]), "server-%zu", i);
        entries[i].name = names[i];
        entries[i].command = "echo";
        entries[i].auto_connect = false;
        entries[i].timeout_ms = 1000;
    }

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, HU_MCP_MANAGER_MAX_SERVERS, &mgr);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(mgr);
    HU_ASSERT_EQ(hu_mcp_manager_server_count(mgr), (size_t)HU_MCP_MANAGER_MAX_SERVERS);

    hu_mcp_manager_destroy(mgr);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 4. MCP manager: exceed max servers should fail */
static void test_mcp_exceed_max_servers(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    size_t over = HU_MCP_MANAGER_MAX_SERVERS + 1;
    hu_mcp_server_entry_t *entries = alloc.alloc(alloc.ctx, over * sizeof(hu_mcp_server_entry_t));
    HU_ASSERT_NOT_NULL(entries);
    char names[HU_MCP_MANAGER_MAX_SERVERS + 1][32];
    for (size_t i = 0; i < over; i++) {
        memset(&entries[i], 0, sizeof(entries[i]));
        snprintf(names[i], sizeof(names[i]), "s%zu", i);
        entries[i].name = names[i];
        entries[i].command = "echo";
    }

    hu_mcp_manager_t *mgr = NULL;
    hu_error_t err = hu_mcp_manager_create(&alloc, entries, over, &mgr);
    /* Should reject or clamp */
    if (err != HU_OK) {
        HU_ASSERT_NULL(mgr);
    } else {
        /* Accepted — verify it didn't allocate beyond MAX */
        HU_ASSERT(hu_mcp_manager_server_count(mgr) <= HU_MCP_MANAGER_MAX_SERVERS);
        hu_mcp_manager_destroy(mgr);
    }

    alloc.free(alloc.ctx, entries, over * sizeof(hu_mcp_server_entry_t));
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 5. MCP: 1MB escaped string in tool build name */
static void test_mcp_1mb_escaped_string(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    size_t big = 1024 * 1024;
    char *bigstr = alloc.alloc(alloc.ctx, big + 1);
    HU_ASSERT_NOT_NULL(bigstr);
    memset(bigstr, 'X', big);
    bigstr[big] = '\0';

    char *built = NULL;
    size_t built_len = 0;
    hu_error_t err = hu_mcp_tool_build_name(&alloc, "server", bigstr, &built, &built_len);
    if (err == HU_OK) {
        HU_ASSERT_NOT_NULL(built);
        /* Prefix is "mcp__server__" = 13 chars + 1MB */
        HU_ASSERT_GT(built_len, big);
        alloc.free(alloc.ctx, built, built_len + 1);
    }

    alloc.free(alloc.ctx, bigstr, big + 1);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Hook Memory Safety
 * ══════════════════════════════════════════════════════════════════════════ */

/* 6. Shell escape: input with every byte value 0x01..0xFF */
static void test_hook_shell_escape_all_bytes(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char input[255];
    for (int i = 0; i < 255; i++) {
        input[i] = (char)(i + 1); /* 0x01..0xFF */
    }

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_hook_shell_escape(&alloc, input, 255, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    /* Output should be safely quoted */
    HU_ASSERT(out[0] == '\'');
    HU_ASSERT(out[out_len - 1] == '\'');
    alloc.free(alloc.ctx, out, out_len + 1);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 7. Hook registry: add then destroy many hooks — no leaks */
static void test_hook_registry_mass_add_no_leak(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    for (int i = 0; i < 100; i++) {
        char name[32], cmd[64];
        snprintf(name, sizeof(name), "hook-%d", i);
        snprintf(cmd, sizeof(cmd), "echo test-%d", i);
        hu_hook_entry_t entry = {
            .name = name,
            .name_len = strlen(name),
            .event = (i % 2) ? HU_HOOK_PRE_TOOL_EXECUTE : HU_HOOK_POST_TOOL_EXECUTE,
            .command = cmd,
            .command_len = strlen(cmd),
            .timeout_sec = 10,
            .required = false,
        };
        HU_ASSERT_EQ(hu_hook_registry_add(reg, &alloc, &entry), HU_OK);
    }
    HU_ASSERT_EQ(hu_hook_registry_count(reg), 100);

    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 8. Hook pipeline: mock returns large stdout — check cleanup */
static void test_hook_pipeline_large_output_cleanup(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_hook_registry_t *reg = NULL;
    hu_hook_registry_create(&alloc, &reg);

    hu_hook_entry_t entry = {
        .name = "big-output",
        .name_len = 10,
        .event = HU_HOOK_PRE_TOOL_EXECUTE,
        .command = "echo big",
        .command_len = 8,
        .timeout_sec = 5,
        .required = false,
    };
    hu_hook_registry_add(reg, &alloc, &entry);

    /* Set mock to return a large stdout */
    char big_output[4096];
    memset(big_output, 'Z', sizeof(big_output) - 1);
    big_output[sizeof(big_output) - 1] = '\0';

    hu_hook_mock_config_t mock = {
        .exit_code = 0,
        .stdout_data = big_output,
        .stdout_len = sizeof(big_output) - 1,
    };
    hu_hook_mock_set(&mock);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_hook_pipeline_pre_tool(reg, &alloc, "shell", 5, "{}", 2, &result);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hook_result_free(&alloc, &result);

    hu_hook_mock_reset();
    hu_hook_registry_destroy(reg, &alloc);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 9. Hook shell escape: empty string (zero length) */
static void test_hook_shell_escape_zero_len(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_hook_shell_escape(&alloc, "", 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_EQ(out, "''");
    alloc.free(alloc.ctx, out, out_len + 1);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Session Memory Safety
 * ══════════════════════════════════════════════════════════════════════════ */

/* 10. Session ID generation: buffer exactly HU_SESSION_ID_MAX */
static void test_session_id_no_overflow(void) {
    char buf[HU_SESSION_ID_MAX];
    memset(buf, 'X', sizeof(buf));
    hu_session_generate_id(buf, sizeof(buf));
    /* Must be null-terminated within bounds */
    size_t len = strlen(buf);
    HU_ASSERT_LT(len, (long long)HU_SESSION_ID_MAX);
}

/* 11. Session ID generation: tiny buffer — must not overflow */
static void test_session_id_tiny_buffer(void) {
    char buf[4];
    memset(buf, 'X', sizeof(buf));
    hu_session_generate_id(buf, sizeof(buf));
    /* Should produce truncated but null-terminated output */
    HU_ASSERT_EQ(buf[3], '\0');
}

/* 12. Session save with NULL agent — should fail gracefully */
static void test_session_save_null_agent(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_session_persist_save(&alloc, NULL, "/tmp", NULL);
    HU_ASSERT_NEQ(err, HU_OK);
}

/* 13. Session load: non-existent session */
static void test_session_load_nonexistent(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_error_t err = hu_session_persist_load(&alloc, NULL, "/tmp/nonexistent_hu_test_dir",
                                     "nonexistent_session_999");
    HU_ASSERT_NEQ(err, HU_OK);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Compaction Memory Safety
 * ══════════════════════════════════════════════════════════════════════════ */

/* 14. Compaction: zero messages should not crash */
static void test_compaction_zero_messages(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_compaction_summary_t meta;
    memset(&meta, 0, sizeof(meta));

    hu_error_t err = hu_compact_extract_metadata(&alloc, NULL, 0, 0, &meta);
    /* Should succeed with empty output or return error — no crash */
    if (err == HU_OK) {
        HU_ASSERT_EQ(meta.tool_mentions_count, 0);
        hu_compaction_summary_free(&alloc, &meta);
    }

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 15. Compaction: strip analysis on empty string */
static void test_compaction_strip_analysis_empty(void) {
    char empty[] = "";
    size_t result = hu_compact_strip_analysis(empty, 0);
    HU_ASSERT_EQ(result, 0);
}

/* 16. Compaction: strip analysis with no analysis block */
static void test_compaction_strip_analysis_no_block(void) {
    char content[] = "Hello world, no analysis here.";
    size_t orig_len = strlen(content);
    size_t result = hu_compact_strip_analysis(content, orig_len);
    HU_ASSERT_EQ(result, orig_len);
    HU_ASSERT_STR_EQ(content, "Hello world, no analysis here.");
}

/* 17. Compaction: strip analysis with block */
static void test_compaction_strip_analysis_with_block(void) {
    char content[256];
    snprintf(content, sizeof(content), "before<analysis>secret</analysis>after");
    size_t len = strlen(content);
    size_t result = hu_compact_strip_analysis(content, len);
    /* Should have removed the <analysis>...</analysis> block */
    HU_ASSERT_LT(result, len);
    /* "before" and "after" should both be present */
    HU_ASSERT(strstr(content, "before") != NULL);
    HU_ASSERT(strstr(content, "after") != NULL);
    HU_ASSERT(strstr(content, "secret") == NULL);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Instruction Discovery Memory Safety
 * ══════════════════════════════════════════════════════════════════════════ */

/* 18. Instruction validate_path: null byte injection */
static void test_instruction_path_null_byte(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    char path_with_null[] = "/tmp/test\0../../etc/passwd";
    char *canonical = NULL;
    size_t canonical_len = 0;
    hu_error_t err = hu_instruction_validate_path(&alloc, path_with_null,
                                                   sizeof(path_with_null) - 1,
                                                   &canonical, &canonical_len);
    /* Should detect null byte and reject, or treat as truncated */
    if (err == HU_OK) {
        HU_ASSERT_NOT_NULL(canonical);
        /* If it accepted, canonical should NOT contain "passwd" */
        HU_ASSERT(strstr(canonical, "passwd") == NULL);
        alloc.free(alloc.ctx, canonical, canonical_len + 1);
    }

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* 19. Instruction validate_path: invalid UTF-8 bytes */
static void test_instruction_path_invalid_utf8(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    /* 0xFE and 0xFF are never valid UTF-8 */
    char bad_path[] = "/tmp/\xFE\xFF_test";
    char *canonical = NULL;
    size_t canonical_len = 0;
    hu_error_t err = hu_instruction_validate_path(&alloc, bad_path,
                                                   strlen(bad_path),
                                                   &canonical, &canonical_len);
    /* May fail or succeed — must not crash and must not leak */
    if (err == HU_OK && canonical) {
        alloc.free(alloc.ctx, canonical, canonical_len + 1);
    }

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Suite runner
 * ══════════════════════════════════════════════════════════════════════════ */

void run_adversarial_memory_safety_tests(void) {
    HU_TEST_SUITE("Adversarial: Memory Safety");

    /* MCP */
    HU_RUN_TEST(test_mcp_huge_method_name);
    HU_RUN_TEST(test_mcp_null_bytes_in_strings);
    HU_RUN_TEST(test_mcp_max_servers_no_leak);
    HU_RUN_TEST(test_mcp_exceed_max_servers);
    HU_RUN_TEST(test_mcp_1mb_escaped_string);

    /* Hooks */
    HU_RUN_TEST(test_hook_shell_escape_all_bytes);
    HU_RUN_TEST(test_hook_registry_mass_add_no_leak);
    HU_RUN_TEST(test_hook_pipeline_large_output_cleanup);
    HU_RUN_TEST(test_hook_shell_escape_zero_len);

    /* Session */
    HU_RUN_TEST(test_session_id_no_overflow);
    HU_RUN_TEST(test_session_id_tiny_buffer);
    HU_RUN_TEST(test_session_save_null_agent);
    HU_RUN_TEST(test_session_load_nonexistent);

    /* Compaction */
    HU_RUN_TEST(test_compaction_zero_messages);
    HU_RUN_TEST(test_compaction_strip_analysis_empty);
    HU_RUN_TEST(test_compaction_strip_analysis_no_block);
    HU_RUN_TEST(test_compaction_strip_analysis_with_block);

    /* Instruction Discovery */
    HU_RUN_TEST(test_instruction_path_null_byte);
    HU_RUN_TEST(test_instruction_path_invalid_utf8);
}
