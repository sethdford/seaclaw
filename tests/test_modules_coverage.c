#include "seaclaw/agent/mailbox.h"
#include "seaclaw/channel.h"
#include "seaclaw/channels/dispatch.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include "seaclaw/daemon.h"
#include "seaclaw/mcp.h"
#include "seaclaw/memory.h"
#include "seaclaw/memory/retrieval.h"
#include "seaclaw/security.h"
#include "seaclaw/tool.h"
#include "seaclaw/tools/apply_patch.h"
#include "seaclaw/tools/diff.h"
#include "seaclaw/tools/send_message.h"
#include "seaclaw/tunnel.h"
#include "test_framework.h"
#include <string.h>
#include <time.h>

/* ─── MCP tests ─────────────────────────────────────────────────────────── */

static void test_mcp_server_create_destroy_valid(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_config_t cfg = {.command = "echo", .args = NULL, .args_count = 0};
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, &cfg);
    SC_ASSERT_NOT_NULL(srv);
    sc_mcp_server_destroy(srv);
}

static void test_mcp_server_create_null_config_returns_null(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mcp_server_t *srv = sc_mcp_server_create(&alloc, NULL);
    SC_ASSERT_NULL(srv);
}

static void test_mcp_init_tools_empty_configs_returns_ok(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t *tools = NULL;
    size_t count = 0;
    sc_error_t err = sc_mcp_init_tools(&alloc, NULL, 0, &tools, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NULL(tools);
    SC_ASSERT_EQ(count, 0u);
}

/* ─── Tunnel tests ───────────────────────────────────────────────────────── */

static void test_tunnel_none_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tunnel_t t = sc_none_tunnel_create(&alloc);
    SC_ASSERT_NOT_NULL(t.ctx);
    SC_ASSERT_NOT_NULL(t.vtable);
    if (t.vtable->deinit)
        t.vtable->deinit(t.ctx, &alloc);
}

static void test_tunnel_tailscale_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tunnel_t t = sc_tailscale_tunnel_create(&alloc);
    SC_ASSERT_NOT_NULL(t.ctx);
    SC_ASSERT_STR_EQ(t.vtable->provider_name(t.ctx), "tailscale");
    if (t.vtable->deinit)
        t.vtable->deinit(t.ctx, &alloc);
}

static void test_tunnel_cloudflare_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tunnel_t t = sc_cloudflare_tunnel_create(&alloc, "test-token", 10);
    SC_ASSERT_NOT_NULL(t.ctx);
    SC_ASSERT_STR_EQ(t.vtable->provider_name(t.ctx), "cloudflare");
    if (t.vtable->deinit)
        t.vtable->deinit(t.ctx, &alloc);
}

static void test_tunnel_ngrok_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tunnel_t t = sc_ngrok_tunnel_create(&alloc, "tok", 3, NULL, 0);
    SC_ASSERT_NOT_NULL(t.ctx);
    SC_ASSERT_STR_EQ(t.vtable->provider_name(t.ctx), "ngrok");
    if (t.vtable->deinit)
        t.vtable->deinit(t.ctx, &alloc);
}

static void test_tunnel_custom_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *cmd = "echo https://example.com";
    sc_tunnel_t t = sc_custom_tunnel_create(&alloc, cmd, strlen(cmd));
    SC_ASSERT_NOT_NULL(t.ctx);
    SC_ASSERT_STR_EQ(t.vtable->provider_name(t.ctx), "custom");
    if (t.vtable->deinit)
        t.vtable->deinit(t.ctx, &alloc);
}

static void test_tunnel_factory_none(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tunnel_config_t config = {.provider = SC_TUNNEL_NONE};
    sc_tunnel_t t = sc_tunnel_create(&alloc, &config);
    SC_ASSERT_NOT_NULL(t.ctx);
    SC_ASSERT_STR_EQ(t.vtable->provider_name(t.ctx), "none");
    if (t.vtable->deinit)
        t.vtable->deinit(t.ctx, &alloc);
}

/* ─── Daemon tests ───────────────────────────────────────────────────────── */

static void test_daemon_start_stop_test_mode(void) {
#if SC_IS_TEST
    sc_error_t err = sc_daemon_start();
    SC_ASSERT_EQ(err, SC_OK);
    err = sc_daemon_stop();
    SC_ASSERT_EQ(err, SC_OK);
    bool status = sc_daemon_status();
    SC_ASSERT_FALSE(status);
#else
    (void)0;
#endif
}

static void test_cron_schedule_matches_star(void) {
    struct tm tm = {0};
    tm.tm_min = 30;
    tm.tm_hour = 14;
    tm.tm_mday = 15;
    tm.tm_mon = 5;  /* June = month 6 */
    tm.tm_wday = 3; /* Wednesday */
    bool m = sc_cron_schedule_matches("* * * * *", &tm);
    SC_ASSERT_TRUE(m);
}

static void test_cron_schedule_matches_exact(void) {
    struct tm tm = {0};
    tm.tm_min = 30;
    tm.tm_hour = 14;
    tm.tm_mday = 15;
    tm.tm_mon = 5;
    tm.tm_wday = 3;
    bool m = sc_cron_schedule_matches("30 14 15 6 3", &tm);
    SC_ASSERT_TRUE(m);
}

static void test_cron_schedule_matches_no_match(void) {
    struct tm tm = {0};
    tm.tm_min = 0;
    tm.tm_hour = 12;
    tm.tm_mday = 1;
    tm.tm_mon = 0;
    tm.tm_wday = 1;
    bool m = sc_cron_schedule_matches("59 23 31 12 0", &tm);
    SC_ASSERT_FALSE(m);
}

static void test_cron_schedule_matches_null_returns_false(void) {
    struct tm tm = {0};
    SC_ASSERT_FALSE(sc_cron_schedule_matches(NULL, &tm));
    SC_ASSERT_FALSE(sc_cron_schedule_matches("* * * * *", NULL));
}

/* ─── Tools tests (diff, apply_patch, send_message) ──────────────────────── */

static void test_diff_tool_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_error_t err = sc_diff_tool_create(&alloc, NULL, 0, NULL, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable);
    SC_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "diff");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_diff_tool_create_null_out_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_diff_tool_create(&alloc, NULL, 0, NULL, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_diff_tool_execute_null_args_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_diff_tool_create(&alloc, NULL, 0, NULL, &tool);
    sc_tool_result_t res = {0};
    sc_error_t err = tool.vtable->execute(tool.ctx, &alloc, NULL, &res);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_diff_rejects_path_traversal(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_error_t err = sc_diff_tool_create(&alloc, "/tmp", 4, NULL, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    sc_json_value_t *args = sc_json_object_new(&alloc);
    sc_json_object_set(&alloc, args, "action", sc_json_string_new(&alloc, "diff", 4));
    sc_json_object_set(&alloc, args, "file_a", sc_json_string_new(&alloc, "../etc/passwd", 13));
    sc_json_object_set(&alloc, args, "file_b", sc_json_string_new(&alloc, "b.txt", 5));
    sc_tool_result_t result = {0};
    err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    sc_json_free(&alloc, args);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(!result.success);
    SC_ASSERT_NOT_NULL(result.error_msg);
    SC_ASSERT_TRUE(strstr(result.error_msg, "path not allowed") != NULL);
    sc_tool_result_free(&alloc, &result);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_diff_rejects_absolute_path(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_error_t cr = sc_diff_tool_create(&alloc, "/tmp", 4, NULL, &tool);
    SC_ASSERT_EQ(cr, SC_OK);
    sc_json_value_t *args = sc_json_object_new(&alloc);
    sc_json_object_set(&alloc, args, "action", sc_json_string_new(&alloc, "diff", 4));
    sc_json_object_set(&alloc, args, "file_a", sc_json_string_new(&alloc, "/etc/passwd", 11));
    sc_json_object_set(&alloc, args, "file_b", sc_json_string_new(&alloc, "b.txt", 5));
    sc_tool_result_t result = {0};
    sc_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    sc_json_free(&alloc, args);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(!result.success);
    SC_ASSERT_NOT_NULL(result.error_msg);
    SC_ASSERT_TRUE(strstr(result.error_msg, "path not allowed") != NULL);
    sc_tool_result_free(&alloc, &result);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_apply_patch_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_security_policy_t policy = {0};
    policy.workspace_dir = "/tmp";
    policy.workspace_only = true;
    sc_tool_t tool = {0};
    sc_error_t err = sc_apply_patch_create(&alloc, &policy, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable);
    SC_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "apply_patch");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_apply_patch_create_null_out_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_security_policy_t policy = {0};
    sc_error_t err = sc_apply_patch_create(&alloc, &policy, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_apply_patch_rejects_path_traversal(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_security_policy_t policy = {0};
    policy.workspace_dir = "/tmp";
    policy.workspace_only = true;
    sc_tool_t tool = {0};
    sc_error_t cr = sc_apply_patch_create(&alloc, &policy, &tool);
    SC_ASSERT_EQ(cr, SC_OK);
    sc_json_value_t *args = sc_json_object_new(&alloc);
    sc_json_object_set(&alloc, args, "file", sc_json_string_new(&alloc, "../etc/passwd", 13));
    sc_json_object_set(&alloc, args, "patch",
                       sc_json_string_new(&alloc, "@@ -1 +1 @@\n+line\n", 18));
    sc_tool_result_t result = {0};
    sc_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    sc_json_free(&alloc, args);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(!result.success);
    SC_ASSERT_NOT_NULL(result.error_msg);
    SC_ASSERT_TRUE(strstr(result.error_msg, "path traversal") != NULL ||
                   strstr(result.error_msg, "path not allowed") != NULL);
    sc_tool_result_free(&alloc, &result);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_send_message_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_mailbox_t *mbox = sc_mailbox_create(&alloc, 4);
    SC_ASSERT_NOT_NULL(mbox);
    sc_tool_t tool = {0};
    sc_error_t err = sc_send_message_create(&alloc, mbox, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable);
    SC_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "send_message");
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
    sc_mailbox_destroy(mbox);
}

static void test_send_message_create_null_mailbox(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_error_t err = sc_send_message_create(&alloc, NULL, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

/* ─── Memory retrieval tests ────────────────────────────────────────────── */

static void test_retrieval_keyword_empty_backend(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_none_memory_create(&alloc);
    sc_retrieval_options_t opts = {
        .mode = SC_RETRIEVAL_KEYWORD,
        .limit = 5,
        .min_score = 0.0,
        .use_reranking = false,
        .temporal_decay_factor = 0.0,
    };
    sc_retrieval_result_t res = {0};
    sc_error_t err = sc_keyword_retrieve(&alloc, &mem, "query", 5, &opts, &res);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(res.count, 0u);
    mem.vtable->deinit(mem.ctx);
}

static void test_retrieval_hybrid_empty_backend(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_none_memory_create(&alloc);
    sc_retrieval_options_t opts = {
        .mode = SC_RETRIEVAL_HYBRID,
        .limit = 5,
        .min_score = 0.0,
    };
    sc_retrieval_result_t res = {0};
    sc_error_t err = sc_hybrid_retrieve(&alloc, &mem, NULL, NULL, NULL, "query", 5, &opts, &res);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(res.count, 0u);
    mem.vtable->deinit(mem.ctx);
}

static void test_retrieval_engine_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_none_memory_create(&alloc);
    sc_retrieval_engine_t eng = sc_retrieval_create(&alloc, &mem);
    SC_ASSERT_NOT_NULL(eng.ctx);
    SC_ASSERT_NOT_NULL(eng.vtable);
    eng.vtable->deinit(eng.ctx, &alloc);
    mem.vtable->deinit(mem.ctx);
}

static void test_temporal_decay_score(void) {
    double base = 1.0;
    const char *ts = "2026-01-15T12:00:00Z";
    double d = sc_temporal_decay_score(base, 0.5, ts, strlen(ts));
    SC_ASSERT_TRUE(d >= 0.0);
    SC_ASSERT_TRUE(d <= base);
    double no_decay = sc_temporal_decay_score(base, 0.0, ts, strlen(ts));
    SC_ASSERT_FLOAT_EQ(no_decay, base, 1e-9);
}

#ifdef SC_ENABLE_SQLITE
static void test_retrieval_keyword_with_mock_data(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_sqlite_memory_create(&alloc, ":memory:");
    sc_memory_category_t cat = {.tag = SC_MEMORY_CATEGORY_CORE};
    mem.vtable->store(mem.ctx, "key_a", 5, "alpha beta gamma", 16, &cat, NULL, 0);
    mem.vtable->store(mem.ctx, "key_b", 5, "delta epsilon", 14, &cat, NULL, 0);

    sc_retrieval_options_t opts = {
        .mode = SC_RETRIEVAL_KEYWORD,
        .limit = 5,
        .min_score = 0.0,
    };
    sc_retrieval_result_t res = {0};
    sc_error_t err = sc_keyword_retrieve(&alloc, &mem, "alpha", 5, &opts, &res);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(res.count, 1u);
    SC_ASSERT_STR_EQ(res.entries[0].key, "key_a");
    sc_retrieval_result_free(&alloc, &res);
    mem.vtable->deinit(mem.ctx);
}

static void test_retrieval_hybrid_with_mock_data(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_sqlite_memory_create(&alloc, ":memory:");
    sc_memory_category_t cat = {.tag = SC_MEMORY_CATEGORY_CORE};
    mem.vtable->store(mem.ctx, "doc1", 4, "machine learning basics", 23, &cat, NULL, 0);
    mem.vtable->store(mem.ctx, "doc2", 4, "neural network training", 23, &cat, NULL, 0);

    sc_retrieval_options_t opts = {
        .mode = SC_RETRIEVAL_HYBRID,
        .limit = 5,
        .min_score = 0.0,
    };
    sc_retrieval_result_t res = {0};
    sc_error_t err = sc_hybrid_retrieve(&alloc, &mem, NULL, NULL, NULL, "neural", 6, &opts, &res);
    SC_ASSERT_TRUE(err == SC_OK || err == SC_ERR_NOT_SUPPORTED || err == SC_ERR_INVALID_ARGUMENT);
    sc_retrieval_result_free(&alloc, &res);
    mem.vtable->deinit(mem.ctx);
}
#endif

/* ─── Channel dispatch tests ─────────────────────────────────────────────── */

static void test_dispatch_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_dispatch_create(&alloc, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ch.ctx);
    SC_ASSERT_NOT_NULL(ch.vtable);
    SC_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "dispatch");
    sc_dispatch_destroy(&ch);
    SC_ASSERT_NULL(ch.ctx);
}

static void test_dispatch_create_null_alloc_fails(void) {
    sc_channel_t ch = {0};
    sc_error_t err = sc_dispatch_create(NULL, &ch);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_dispatch_create_null_out_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_dispatch_create(&alloc, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_dispatch_add_channel(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t dispatch_ch = {0};
    sc_dispatch_create(&alloc, &dispatch_ch);

    sc_channel_t sub = {0};
    sub.ctx = (void *)1;
    sub.vtable = NULL;

    sc_error_t err = sc_dispatch_add_channel(&dispatch_ch, &sub);
    SC_ASSERT_EQ(err, SC_OK);

    sc_dispatch_destroy(&dispatch_ch);
}

static void test_dispatch_add_channel_null_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t dispatch_ch = {0};
    sc_dispatch_create(&alloc, &dispatch_ch);
    sc_error_t err = sc_dispatch_add_channel(&dispatch_ch, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
    sc_dispatch_destroy(&dispatch_ch);
}

static void test_dispatch_start_stop_health(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_dispatch_create(&alloc, &ch);
    SC_ASSERT_FALSE(ch.vtable->health_check(ch.ctx));
    sc_error_t err = ch.vtable->start(ch.ctx);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(ch.vtable->health_check(ch.ctx));
    ch.vtable->stop(ch.ctx);
    SC_ASSERT_FALSE(ch.vtable->health_check(ch.ctx));
    sc_dispatch_destroy(&ch);
}

static void test_dispatch_send_test_mode(void) {
#if SC_IS_TEST
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_dispatch_create(&alloc, &ch);
    ch.vtable->start(ch.ctx);
    sc_error_t err = ch.vtable->send(ch.ctx, "target", 6, "hello", 5, NULL, 0);
    SC_ASSERT_EQ(err, SC_OK);
    sc_dispatch_destroy(&ch);
#else
    (void)0;
#endif
}

void run_modules_coverage_tests(void) {
    SC_TEST_SUITE("Modules coverage: MCP");
    SC_RUN_TEST(test_mcp_server_create_destroy_valid);
    SC_RUN_TEST(test_mcp_server_create_null_config_returns_null);
    SC_RUN_TEST(test_mcp_init_tools_empty_configs_returns_ok);

    SC_TEST_SUITE("Modules coverage: Tunnel");
    SC_RUN_TEST(test_tunnel_none_create_destroy);
    SC_RUN_TEST(test_tunnel_tailscale_create_destroy);
    SC_RUN_TEST(test_tunnel_cloudflare_create_destroy);
    SC_RUN_TEST(test_tunnel_ngrok_create_destroy);
    SC_RUN_TEST(test_tunnel_custom_create_destroy);
    SC_RUN_TEST(test_tunnel_factory_none);

    SC_TEST_SUITE("Modules coverage: Daemon");
    SC_RUN_TEST(test_daemon_start_stop_test_mode);
    SC_RUN_TEST(test_cron_schedule_matches_star);
    SC_RUN_TEST(test_cron_schedule_matches_exact);
    SC_RUN_TEST(test_cron_schedule_matches_no_match);
    SC_RUN_TEST(test_cron_schedule_matches_null_returns_false);

    SC_TEST_SUITE("Modules coverage: Tools (diff, apply_patch, send_message)");
    SC_RUN_TEST(test_diff_tool_create);
    SC_RUN_TEST(test_diff_tool_create_null_out_fails);
    SC_RUN_TEST(test_diff_tool_execute_null_args_fails);
    SC_RUN_TEST(test_diff_rejects_path_traversal);
    SC_RUN_TEST(test_diff_rejects_absolute_path);
    SC_RUN_TEST(test_apply_patch_create);
    SC_RUN_TEST(test_apply_patch_create_null_out_fails);
    SC_RUN_TEST(test_apply_patch_rejects_path_traversal);
    SC_RUN_TEST(test_send_message_create);
    SC_RUN_TEST(test_send_message_create_null_mailbox);

    SC_TEST_SUITE("Modules coverage: Memory retrieval");
    SC_RUN_TEST(test_retrieval_keyword_empty_backend);
    SC_RUN_TEST(test_retrieval_hybrid_empty_backend);
    SC_RUN_TEST(test_retrieval_engine_create_destroy);
    SC_RUN_TEST(test_temporal_decay_score);
#ifdef SC_ENABLE_SQLITE
    SC_RUN_TEST(test_retrieval_keyword_with_mock_data);
    SC_RUN_TEST(test_retrieval_hybrid_with_mock_data);
#endif

    SC_TEST_SUITE("Modules coverage: Channel dispatch");
    SC_RUN_TEST(test_dispatch_create_destroy);
    SC_RUN_TEST(test_dispatch_create_null_alloc_fails);
    SC_RUN_TEST(test_dispatch_create_null_out_fails);
    SC_RUN_TEST(test_dispatch_add_channel);
    SC_RUN_TEST(test_dispatch_add_channel_null_fails);
    SC_RUN_TEST(test_dispatch_start_stop_health);
    SC_RUN_TEST(test_dispatch_send_test_mode);
}
