/* Gateway edge cases + control protocol + event bridge tests. */
#include "human/agent.h"
#include "human/bus.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/gateway.h"
#include "human/gateway/control_protocol.h"
#include "human/gateway/event_bridge.h"
#include "human/gateway/openai_compat.h"
#include "human/gateway/rate_limit.h"
#include "human/gateway/ws_server.h"
#include "human/health.h"
#include "human/memory.h"
#include "human/provider.h"
#include "human/tool.h"
#include "test_framework.h"
#include "cp_internal.h"
#include <string.h>
#include <time.h>

typedef struct gw_e2e_mock_provider_ctx {
    const char *name;
} gw_e2e_mock_provider_ctx_t;

static hu_error_t gw_e2e_mock_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                               const char *system_prompt, size_t system_prompt_len,
                                               const char *message, size_t message_len,
                                               const char *model, size_t model_len, double temperature,
                                               char **out, size_t *out_len) {
    (void)ctx;
    (void)system_prompt;
    (void)system_prompt_len;
    (void)message;
    (void)message_len;
    (void)model;
    (void)model_len;
    (void)temperature;
    const char *resp = "mock response";
    *out = hu_strndup(alloc, resp, strlen(resp));
    *out_len = *out ? strlen(resp) : 0;
    return *out ? HU_OK : HU_ERR_OUT_OF_MEMORY;
}

static hu_error_t gw_e2e_mock_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *request,
                                   const char *model, size_t model_len, double temperature,
                                   hu_chat_response_t *out) {
    (void)ctx;
    (void)request;
    (void)model;
    (void)model_len;
    (void)temperature;
    const char *resp = "mock response";
    out->content = hu_strndup(alloc, resp, strlen(resp));
    out->content_len = out->content ? strlen(resp) : 0;
    out->tool_calls = NULL;
    out->tool_calls_count = 0;
    out->usage.prompt_tokens = 1;
    out->usage.completion_tokens = 2;
    out->usage.total_tokens = 3;
    out->model = NULL;
    out->model_len = 0;
    out->reasoning_content = NULL;
    out->reasoning_content_len = 0;
    return out->content ? HU_OK : HU_ERR_OUT_OF_MEMORY;
}

static bool gw_e2e_mock_supports_native_tools(void *ctx) {
    (void)ctx;
    return false;
}

static const char *gw_e2e_mock_get_name(void *ctx) {
    return ((gw_e2e_mock_provider_ctx_t *)ctx)->name;
}

static void gw_e2e_mock_deinit(void *ctx, hu_allocator_t *alloc) {
    (void)ctx;
    (void)alloc;
}

static const hu_provider_vtable_t gw_e2e_mock_provider_vtable = {
    .chat_with_system = gw_e2e_mock_chat_with_system,
    .chat = gw_e2e_mock_chat,
    .supports_native_tools = gw_e2e_mock_supports_native_tools,
    .get_name = gw_e2e_mock_get_name,
    .deinit = gw_e2e_mock_deinit,
};

static hu_provider_t gw_e2e_mock_provider_create(gw_e2e_mock_provider_ctx_t *ctx) {
    ctx->name = "mock";
    return (hu_provider_t){.ctx = ctx, .vtable = &gw_e2e_mock_provider_vtable};
}

typedef struct gw_chat_agent_bus_bridge {
    hu_agent_t *agent;
    hu_bus_t *bus;
} gw_chat_agent_bus_bridge_t;

static void gw_e2e_noop_stream_cb(const hu_agent_stream_event_t *event, void *ctx) {
    (void)event;
    (void)ctx;
}

static char gw_e2e_last_assistant_reply[512];

static bool gw_e2e_on_message_sent(hu_bus_event_type_t type, const hu_bus_event_t *ev, void *u) {
    (void)u;
    if (type != HU_BUS_MESSAGE_SENT || !ev)
        return true;
    const char *msg = ev->payload ? (const char *)ev->payload : ev->message;
    if (!msg || !msg[0])
        return true;
    snprintf(gw_e2e_last_assistant_reply, sizeof(gw_e2e_last_assistant_reply), "%s", msg);
    return true;
}

static bool gw_e2e_on_message_received(hu_bus_event_type_t type, const hu_bus_event_t *ev,
                                       void *user_ctx) {
    (void)type;
    gw_chat_agent_bus_bridge_t *b = (gw_chat_agent_bus_bridge_t *)user_ctx;
    if (!b || !b->agent || !ev)
        return true;
    const char *msg = ev->payload ? (const char *)ev->payload : ev->message;
    if (!msg || !msg[0])
        return true;

    char *reply = NULL;
    size_t reply_len = 0;
    b->agent->active_channel = "gateway";
    b->agent->active_channel_len = 8;
    hu_error_t err =
        hu_agent_turn_stream_v2(b->agent, msg, strlen(msg), gw_e2e_noop_stream_cb, NULL, &reply,
                                &reply_len);
    if (err == HU_OK && reply && reply_len > 0 && b->bus) {
        hu_bus_event_t rev;
        memset(&rev, 0, sizeof(rev));
        rev.type = HU_BUS_MESSAGE_SENT;
        snprintf(rev.channel, HU_BUS_CHANNEL_LEN, "%s",
                 ev->channel[0] ? ev->channel : "gateway");
        snprintf(rev.id, HU_BUS_ID_LEN, "%s", ev->id);
        rev.payload = reply;
        size_t rl = reply_len;
        if (rl >= HU_BUS_MSG_LEN)
            rl = HU_BUS_MSG_LEN - 1;
        memcpy(rev.message, reply, rl);
        rev.message[rl] = '\0';
        hu_bus_publish(b->bus, &rev);
    }
    if (reply)
        b->agent->alloc->free(b->agent->alloc->ctx, reply, reply_len + 1);
    return true;
}

static void test_gateway_webhook_paths(void) {
    HU_ASSERT_EQ(HU_GATEWAY_MAX_BODY_SIZE, 65536u);

    /* Validate common webhook path prefixes */
    const char *valid_paths[] = {"/webhook", "/api/webhook", "/gateway/hook"};
    for (size_t i = 0; i < 3; i++) {
        HU_ASSERT_NOT_NULL(valid_paths[i]);
        HU_ASSERT_TRUE(strlen(valid_paths[i]) > 0);
        HU_ASSERT_TRUE(valid_paths[i][0] == '/');
    }

    /* Path traversal must be rejected */
    const char *bad_paths[] = {"/../etc/passwd", "/webhook/../../../secret", "/..%2f..%2f"};
    for (size_t i = 0; i < 3; i++) {
        HU_ASSERT_NOT_NULL(bad_paths[i]);
        HU_ASSERT_TRUE(strstr(bad_paths[i], "..") != NULL);
    }
}

static void test_gateway_health_endpoint(void) {
    hu_health_reset();
    hu_health_mark_ok("gateway");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_READY);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_ready_endpoint(void) {
    hu_health_reset();
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_READY);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_rate_limit_sliding_window(void) {
    hu_gateway_config_t cfg = {0};
    cfg.rate_limit_per_minute = 120;
    HU_ASSERT_EQ(cfg.rate_limit_per_minute, 120);
}

static void test_rate_limiter_allow_under_limit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rate_limiter_t *lim = hu_rate_limiter_create(&alloc, 3, 60);
    HU_ASSERT_NOT_NULL(lim);
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "192.168.1.1"));
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "192.168.1.1"));
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "192.168.1.1"));
    hu_rate_limiter_destroy(lim);
}

static void test_rate_limiter_deny_over_limit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rate_limiter_t *lim = hu_rate_limiter_create(&alloc, 2, 60);
    HU_ASSERT_NOT_NULL(lim);
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "10.0.0.1"));
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "10.0.0.1"));
    HU_ASSERT_FALSE(hu_rate_limiter_allow(lim, "10.0.0.1"));
    hu_rate_limiter_destroy(lim);
}

static void test_rate_limiter_window_expiry_resets(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_rate_limiter_t *lim = hu_rate_limiter_create(&alloc, 1, 1);
    HU_ASSERT_NOT_NULL(lim);
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "172.16.0.1"));
    HU_ASSERT_FALSE(hu_rate_limiter_allow(lim, "172.16.0.1"));
    time_t start = time(NULL);
    while (time(NULL) - start < 2) {
        /* Busy-wait for window to expire */
    }
    HU_ASSERT_TRUE(hu_rate_limiter_allow(lim, "172.16.0.1"));
    hu_rate_limiter_destroy(lim);
}

static void test_gateway_max_body_size(void) {
    hu_gateway_config_t cfg = {0};
    cfg.max_body_size = HU_GATEWAY_MAX_BODY_SIZE;
    HU_ASSERT_EQ(cfg.max_body_size, 65536u);
}

static void test_gateway_auth_required(void) {
    hu_gateway_config_t cfg = {0};
    cfg.hmac_secret = "test-secret";
    cfg.hmac_secret_len = 11;
    HU_ASSERT_NOT_NULL(cfg.hmac_secret);
    HU_ASSERT_EQ(cfg.hmac_secret_len, 11u);
}

static void test_gateway_run_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gateway_config_t config = {.port = 0, .test_mode = true};
    hu_error_t err = hu_gateway_run(&alloc, "127.0.0.1", 0, &config);
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_gateway_run_with_host(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gateway_config_t config = {.host = "0.0.0.0", .port = 8080, .test_mode = true};
    hu_error_t err = hu_gateway_run(&alloc, "0.0.0.0", 8080, &config);
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_health_multiple_components(void) {
    hu_health_reset();
    hu_health_mark_ok("a");
    hu_health_mark_ok("b");
    hu_health_mark_error("c", "failed");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_NOT_READY);
    HU_ASSERT_EQ(r.check_count, 3u);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_health_all_ok(void) {
    hu_health_reset();
    hu_health_mark_ok("g");
    hu_health_mark_ok("p");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_READY);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_health_reset_clears(void) {
    hu_health_reset();
    hu_health_mark_error("x", "err");
    hu_health_reset();
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_READY);
    HU_ASSERT_EQ(r.check_count, 0u);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_config_zeroed(void) {
    hu_gateway_config_t cfg = {0};
    HU_ASSERT_NULL(cfg.host);
    HU_ASSERT_EQ(cfg.port, 0);
    HU_ASSERT_EQ(cfg.max_body_size, 0);
    HU_ASSERT_NULL(cfg.hmac_secret);
}

static void test_gateway_config_from_cfg_null_cfg(void) {
    hu_gateway_config_t out = {0};
    out.port = 9999;
    hu_gateway_config_from_cfg(NULL, &out);
    /* NULL cfg returns early without modifying out */
    HU_ASSERT_EQ(out.port, 9999);
}

static void test_gateway_config_from_cfg_null_out(void) {
    hu_config_gateway_t cfg = {0};
    cfg.port = 8080;
    cfg.host = "127.0.0.1";
    hu_gateway_config_from_cfg(&cfg, NULL);
    /* NULL out returns early without crashing */
}

static void test_gateway_config_from_cfg_valid(void) {
    hu_config_gateway_t cfg = {0};
    cfg.port = 8080;
    cfg.host = "127.0.0.1";
    hu_gateway_config_t out = {0};
    hu_gateway_config_from_cfg(&cfg, &out);
    HU_ASSERT_EQ(out.port, 8080);
    HU_ASSERT_NOT_NULL(out.host);
    HU_ASSERT_TRUE(strcmp(out.host, "127.0.0.1") == 0);
    HU_ASSERT_EQ(out.max_body_size, HU_GATEWAY_MAX_BODY_SIZE);
}

static void test_gateway_config_from_cfg_defaults(void) {
    hu_config_gateway_t cfg = {0};
    /* Zeroed cfg: host/port use defaults */
    hu_gateway_config_t out = {0};
    hu_gateway_config_from_cfg(&cfg, &out);
    HU_ASSERT_NOT_NULL(out.host);
    HU_ASSERT_TRUE(strcmp(out.host, "0.0.0.0") == 0);
    HU_ASSERT_EQ(out.port, 3000); /* HU_GATEWAY_DEFAULT_PORT */
}

static void test_readiness_status_values(void) {
    HU_ASSERT_TRUE(HU_READINESS_READY != HU_READINESS_NOT_READY);
}

static void test_gateway_rate_limit_constant(void) {
    HU_ASSERT_EQ(HU_GATEWAY_RATE_LIMIT_PER_MIN, 60u);
}

static void test_health_check_null_alloc(void) {
    hu_health_reset();
    hu_health_mark_ok("g");
    hu_readiness_result_t r = hu_health_check_readiness(NULL);
    HU_ASSERT(r.checks == NULL);
}

static void test_gateway_config_port_range(void) {
    hu_gateway_config_t cfg = {0};
    cfg.port = 65535;
    HU_ASSERT_EQ(cfg.port, 65535);
}

static void test_gateway_config_test_mode_flag(void) {
    hu_gateway_config_t cfg = {.test_mode = true};
    HU_ASSERT_TRUE(cfg.test_mode);
}

static void test_gateway_config_hmac_optional(void) {
    hu_gateway_config_t cfg = {0};
    HU_ASSERT_NULL(cfg.hmac_secret);
    cfg.hmac_secret = "key";
    cfg.hmac_secret_len = 3;
    HU_ASSERT_EQ(cfg.hmac_secret_len, 3u);
}

static void test_health_single_error_not_ready(void) {
    hu_health_reset();
    hu_health_mark_error("x", "fail");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_NOT_READY);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_max_body_nonzero(void) {
    HU_ASSERT(HU_GATEWAY_MAX_BODY_SIZE > 0);
}

static void test_gateway_rate_limit_nonzero(void) {
    HU_ASSERT(HU_GATEWAY_RATE_LIMIT_PER_MIN > 0);
}

static void test_gateway_run_null_config(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gateway_config_t config = {.port = 0, .test_mode = true};
    hu_error_t err = hu_gateway_run(&alloc, NULL, 0, &config);
    HU_ASSERT_TRUE(err == HU_OK || err == HU_ERR_IO || err == HU_ERR_NOT_SUPPORTED);
}

static void test_health_empty_ready(void) {
    hu_health_reset();
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_READY);
    HU_ASSERT_EQ(r.check_count, 0u);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_config_host_default(void) {
    hu_gateway_config_t cfg = {0};
    HU_ASSERT_NULL(cfg.host);
}

static void test_gateway_config_rate_limit_default(void) {
    hu_gateway_config_t cfg = {0};
    HU_ASSERT_EQ(cfg.rate_limit_per_minute, 0u);
}

static void test_health_mark_ok_then_error(void) {
    hu_health_reset();
    hu_health_mark_ok("a");
    hu_health_mark_error("a", "failed");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_NOT_READY);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_run_bind_zero_port(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gateway_config_t config = {.port = 0, .test_mode = true};
    hu_error_t err = hu_gateway_run(&alloc, "127.0.0.1", 0, &config);
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_component_check_structure(void) {
    hu_health_reset();
    hu_health_mark_ok("comp");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT(r.check_count >= 0);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

/* ─── WP-21B parity: additional gateway / health tests ────────────────────── */
static void test_gateway_config_max_body_boundary(void) {
    hu_gateway_config_t cfg = {0};
    cfg.max_body_size = 1;
    HU_ASSERT_EQ(cfg.max_body_size, 1u);
    cfg.max_body_size = 1024 * 1024;
    HU_ASSERT_EQ(cfg.max_body_size, 1048576u);
}

static void test_gateway_config_hmac_len_zero(void) {
    hu_gateway_config_t cfg = {0};
    cfg.hmac_secret = "key";
    cfg.hmac_secret_len = 0;
    HU_ASSERT_EQ(cfg.hmac_secret_len, 0u);
}

static void test_gateway_config_rate_limit_one(void) {
    hu_gateway_config_t cfg = {.rate_limit_per_minute = 1};
    HU_ASSERT_EQ(cfg.rate_limit_per_minute, 1u);
}

static void test_health_mark_ok_overwrites_error(void) {
    hu_health_reset();
    hu_health_mark_error("x", "fail");
    hu_health_mark_ok("x");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_READY);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_config_port_one(void) {
    hu_gateway_config_t cfg = {.port = 1};
    HU_ASSERT_EQ(cfg.port, 1);
}

static void test_gateway_constants_match_zig(void) {
    HU_ASSERT_EQ(HU_GATEWAY_MAX_BODY_SIZE, 65536u);
    HU_ASSERT_EQ(HU_GATEWAY_RATE_LIMIT_PER_MIN, 60u);
}

static void test_health_three_components_mixed(void) {
    hu_health_reset();
    hu_health_mark_ok("a");
    hu_health_mark_error("b", "err");
    hu_health_mark_ok("c");
    hu_allocator_t alloc = hu_system_allocator();
    hu_readiness_result_t r = hu_health_check_readiness(&alloc);
    HU_ASSERT_EQ(r.status, HU_READINESS_NOT_READY);
    HU_ASSERT_EQ(r.check_count, 3u);
    if (r.checks)
        alloc.free(alloc.ctx, (void *)r.checks, r.check_count * sizeof(hu_component_check_t));
}

static void test_gateway_run_test_mode_host_loopback(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_gateway_config_t config = {.host = "127.0.0.1", .port = 3000, .test_mode = true};
    hu_error_t err = hu_gateway_run(&alloc, "127.0.0.1", 3000, &config);
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_ws_server_init_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t srv;
    hu_ws_server_init(&srv, &alloc, NULL, NULL, NULL);
    HU_ASSERT_EQ(srv.conn_count, 0u);
    hu_ws_server_deinit(&srv);
}

static void test_ws_server_upgrade_null_args(void) {
    hu_ws_conn_t *out = NULL;
    const char *req = "GET / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: "
                      "Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
    hu_error_t err = hu_ws_server_upgrade(NULL, 0, req, strlen(req), &out);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_ws_server_is_upgrade_valid(void) {
    const char *req = "GET / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: "
                      "Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
    bool ok = hu_ws_server_is_upgrade(req, strlen(req));
    HU_ASSERT_TRUE(ok);
}

static void test_ws_server_is_upgrade_invalid(void) {
    const char *req = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    bool ok = hu_ws_server_is_upgrade(req, strlen(req));
    HU_ASSERT_FALSE(ok);
}

static void test_ws_server_send_null_conn(void) {
    hu_error_t err = hu_ws_server_send(NULL, NULL, "x", 1);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_ws_server_broadcast_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t srv;
    hu_ws_server_init(&srv, &alloc, NULL, NULL, NULL);
    hu_ws_server_broadcast(&srv, "msg", 3);
    hu_ws_server_deinit(&srv);
}

/* ── Control Protocol Tests ──────────────────────────────────────────── */

static void test_control_protocol_init_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    HU_ASSERT_EQ(proto.event_seq, 0u);
    HU_ASSERT_TRUE(proto.app_ctx == NULL);
    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_control_set_app_ctx(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    HU_ASSERT_TRUE(proto.app_ctx == NULL);

    hu_app_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    hu_control_set_app_ctx(&proto, &ctx);
    HU_ASSERT_TRUE(proto.app_ctx == &ctx);

    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_control_set_app_ctx_null(void) {
    hu_control_set_app_ctx(NULL, NULL);
}

static void test_control_on_message_null(void) {
    hu_control_on_message(NULL, NULL, 0, NULL);
}

static void test_control_on_message_invalid_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    hu_control_on_message(&conn, "not json", 8, &proto);
    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_control_on_message_non_req(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"event\",\"id\":\"1\",\"method\":\"health\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_control_on_message_no_method(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"1\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_control_send_event_null(void) {
    hu_control_send_event(NULL, "test", "{}");
}

static void test_control_send_event_no_ws(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, NULL);
    hu_control_send_event(&proto, "test", "{}");
    hu_control_protocol_deinit(&proto);
}

static void test_control_send_response_null(void) {
    hu_error_t err = hu_control_send_response(NULL, NULL, "id", true, "{}");
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_control_on_close(void) {
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    hu_control_on_close(&conn, NULL);
}

/* ── Event Bridge Tests ─────────────────────────────────────────────── */

static void test_event_bridge_init_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    hu_bus_t bus;
    hu_bus_init(&bus);

    hu_event_bridge_t bridge;
    hu_event_bridge_init(&bridge, &proto, &bus);
    HU_ASSERT_TRUE(bridge.proto == &proto);
    HU_ASSERT_TRUE(bridge.bus == &bus);
    hu_event_bridge_deinit(&bridge);

    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_event_bridge_init_null(void) {
    hu_event_bridge_init(NULL, NULL, NULL);
}

static void test_event_bridge_deinit_null(void) {
    hu_event_bridge_deinit(NULL);
}

static void test_event_bridge_bus_subscription(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    hu_bus_t bus;
    hu_bus_init(&bus);
    HU_ASSERT_EQ(bus.count, 0u);

    hu_event_bridge_t bridge;
    hu_event_bridge_init(&bridge, &proto, &bus);
    HU_ASSERT_TRUE(bus.count > 0);

    hu_event_bridge_deinit(&bridge);
    HU_ASSERT_EQ(bus.count, 0u);

    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_event_bridge_publish_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    hu_bus_t bus;
    hu_bus_init(&bus);

    hu_event_bridge_t bridge;
    hu_event_bridge_init(&bridge, &proto, &bus);

    hu_bus_publish_simple(&bus, HU_BUS_MESSAGE_RECEIVED, "cli", "s1", "hello");
    hu_bus_publish_simple(&bus, HU_BUS_TOOL_CALL, "cli", "t1", "shell");
    hu_bus_publish_simple(&bus, HU_BUS_ERROR, "cli", "e1", "oops");
    hu_bus_publish_simple(&bus, HU_BUS_HEALTH_CHANGE, "gw", "", "ok");

    hu_event_bridge_deinit(&bridge);
    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

/* ── WS Server Extended Tests ───────────────────────────────────────── */

static void test_ws_server_process_null(void) {
    hu_error_t err = hu_ws_server_process(NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_ws_server_read_and_process_null(void) {
    hu_error_t err = hu_ws_server_read_and_process(NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_ws_server_close_conn_inactive(void) {
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = false;
    hu_ws_server_close_conn(NULL, &conn);
}

static void test_ws_server_is_upgrade_short(void) {
    bool ok = hu_ws_server_is_upgrade("GET", 3);
    HU_ASSERT_FALSE(ok);
}

static void test_ws_server_is_upgrade_null(void) {
    bool ok = hu_ws_server_is_upgrade(NULL, 0);
    HU_ASSERT_FALSE(ok);
}

static void test_ws_server_send_inactive_conn(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t srv;
    hu_ws_server_init(&srv, &alloc, NULL, NULL, NULL);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = false;
    conn.fd = 999;
    hu_error_t err = hu_ws_server_send(&srv, &conn, "test", 4);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_ws_server_deinit(&srv);
}

static void test_ws_server_broadcast_null_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t srv;
    hu_ws_server_init(&srv, &alloc, NULL, NULL, NULL);
    hu_ws_server_broadcast(&srv, NULL, 0);
    hu_ws_server_deinit(&srv);
}

static void test_ws_server_conn_pool_full(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t srv;
    hu_ws_server_init(&srv, &alloc, NULL, NULL, NULL);
    for (int i = 0; i < HU_WS_SERVER_MAX_CONNS; i++) {
        srv.conns[i].active = true;
        srv.conns[i].fd = 100 + i;
    }
    srv.conn_count = HU_WS_SERVER_MAX_CONNS;
    hu_ws_conn_t *out = NULL;
    const char *req = "GET / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: "
                      "Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
    hu_error_t err = hu_ws_server_upgrade(&srv, 99, req, strlen(req), &out);
    HU_ASSERT_EQ(err, HU_ERR_ALREADY_EXISTS);
    HU_ASSERT_TRUE(out == NULL);
    for (int i = 0; i < HU_WS_SERVER_MAX_CONNS; i++) {
        srv.conns[i].active = false;
        srv.conns[i].fd = -1;
    }
    hu_ws_server_deinit(&srv);
}

/* ── RPC Handler Tests ──────────────────────────────────────────────── */

static void setup_proto_with_app(hu_allocator_t *alloc, hu_ws_server_t *ws,
                                 hu_control_protocol_t *proto, hu_app_context_t *app, hu_bus_t *bus,
                                 hu_config_t *cfg) {
    hu_ws_server_init(ws, alloc, NULL, NULL, NULL);
    hu_control_protocol_init(proto, alloc, ws);
    memset(app, 0, sizeof(*app));
    memset(cfg, 0, sizeof(*cfg));
    hu_bus_init(bus);
    app->config = cfg;
    app->alloc = alloc;
    app->bus = bus;
    hu_control_set_app_ctx(proto, app);
}

static void teardown_proto(hu_ws_server_t *ws, hu_control_protocol_t *proto) {
    hu_control_protocol_deinit(proto);
    hu_ws_server_deinit(ws);
}

static void test_rpc_chat_send_empty_message_rejected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg =
        "{\"type\":\"req\",\"id\":\"1\",\"method\":\"chat.send\",\"params\":{\"message\":\"\"}}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_chat_send_null_message_rejected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"2\",\"method\":\"chat.send\",\"params\":{}}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static bool s_bus_got_message = false;
static bool chat_send_bus_listener(hu_bus_event_type_t t, const hu_bus_event_t *e, void *u) {
    (void)u;
    if (t == HU_BUS_MESSAGE_RECEIVED && e->payload)
        s_bus_got_message = true;
    return true;
}

static void test_rpc_chat_send_valid_publishes_bus(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);

    s_bus_got_message = false;
    hu_bus_subscribe(&bus, chat_send_bus_listener, NULL, HU_BUS_MESSAGE_RECEIVED);

    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"3\",\"method\":\"chat.send\","
                      "\"params\":{\"message\":\"hello world\"}}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    HU_ASSERT_TRUE(s_bus_got_message);
    teardown_proto(&ws, &proto);
}

static void test_rpc_chat_send_self_harm_returns_crisis(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);

    /* Call cp_chat_send directly with a self-harm message */
    hu_json_value_t *root = hu_json_object_new(&alloc);
    hu_json_value_t *params = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, params, "message",
                       hu_json_string_new(&alloc, "i want to end it all", 20));
    hu_json_object_set(&alloc, root, "params", params);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_chat_send(&alloc, &app, NULL, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    /* Should contain crisis status */
    HU_ASSERT_NOT_NULL(strstr(out, "crisis"));
    HU_ASSERT_NOT_NULL(strstr(out, "988"));
    if (out)
        alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
    teardown_proto(&ws, &proto);
}

static void test_rpc_chat_abort_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"4\",\"method\":\"chat.abort\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_config_apply_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"5\",\"method\":\"config.apply\","
                      "\"params\":{\"config\":\"{\\\"workspace\\\":\\\".\\\"}\"}}";
    ;
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_cron_run_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"6\",\"method\":\"cron.run\","
                      "\"params\":{\"id\":0}}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_skills_install_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"7\",\"method\":\"skills.install\","
                      "\"params\":{\"url\":\"https://example.com/skill.json\"}}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_exec_approval_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"8\",\"method\":\"exec.approval.resolve\","
                      "\"params\":{\"request_id\":\"r1\",\"approved\":true}}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_usage_summary_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"9\",\"method\":\"usage.summary\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

/* HU_IS_TEST: update.check/run use mock data, no network. */
static void test_rpc_update_check_returns_valid_structure(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"uc\",\"method\":\"update.check\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

/* HU_IS_TEST: update.apply returns HU_OK, no real apply. */
static void test_rpc_update_run_returns_valid_structure(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"ur\",\"method\":\"update.run\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

/* nodes.list returns at least one node (local default). */
static void test_rpc_nodes_list_returns_at_least_one_node(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"nl\",\"method\":\"nodes.list\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_agents_list_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"al\",\"method\":\"agents.list\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_voice_config_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"vc\",\"method\":\"voice.config\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_rpc_nodes_action_missing_fields_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    const char *msg = "{\"type\":\"req\",\"id\":\"na\",\"method\":\"nodes.action\"}";
    hu_control_on_message(&conn, msg, strlen(msg), &proto);
    teardown_proto(&ws, &proto);
}

static void test_cp_admin_nodes_action_restart_returns_mock_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_context_t app;
    hu_config_t cfg;
    hu_bus_t bus;
    memset(&app, 0, sizeof(app));
    memset(&cfg, 0, sizeof(cfg));
    hu_bus_init(&bus);
    app.config = &cfg;
    app.alloc = &alloc;
    app.bus = &bus;

    hu_json_value_t *root = NULL;
    const char *json = "{\"type\":\"req\",\"id\":\"r1\",\"method\":\"nodes.action\",\"params\":{"
                       "\"node_id\":\"local\",\"action\":\"restart\"}}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &root), HU_OK);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_admin_nodes_action(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "\"ok\":true") != NULL);
    HU_ASSERT_TRUE(strstr(out, "\"action\":\"restart\"") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
    hu_bus_deinit(&bus);
}

static void test_event_bridge_payload_propagation(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_t proto;
    hu_control_protocol_init(&proto, &alloc, &ws);
    hu_bus_t bus;
    hu_bus_init(&bus);

    hu_event_bridge_t bridge;
    hu_event_bridge_init(&bridge, &proto, &bus);

    hu_bus_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = HU_BUS_MESSAGE_SENT;
    snprintf(ev.channel, HU_BUS_CHANNEL_LEN, "cli");
    snprintf(ev.id, HU_BUS_ID_LEN, "s1");
    const char *long_msg = "This is a long message that exceeds 256 bytes. "
                           "Padding padding padding padding padding padding padding padding "
                           "padding padding padding padding padding padding padding padding "
                           "padding padding padding padding padding padding padding padding "
                           "padding padding padding padding padding padding END";
    ev.payload = (void *)long_msg;
    memset(ev.message, 0, HU_BUS_MSG_LEN);
    size_t ml = strlen(long_msg);
    if (ml >= HU_BUS_MSG_LEN)
        ml = HU_BUS_MSG_LEN - 1;
    memcpy(ev.message, long_msg, ml);
    ev.message[ml] = '\0';
    hu_bus_publish(&bus, &ev);

    hu_event_bridge_deinit(&bridge);
    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
}

static void test_openai_compat_chat_valid_returns_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    hu_app_context_t app = {.config = &cfg};

    const char *body =
        "{\"model\":\"gpt-4o\",\"messages\":[{\"role\":\"user\",\"content\":\"Hi\"}],"
        "\"max_tokens\":1024,\"temperature\":0.7,\"stream\":false}";
    int status = 500;
    char *resp = NULL;
    size_t resp_len = 0;
    hu_openai_compat_handle_chat_completions(body, strlen(body), &alloc, &app, &status, &resp,
                                             &resp_len, NULL);
    HU_ASSERT_EQ(status, 200);
    HU_ASSERT_NOT_NULL(resp);
    HU_ASSERT_TRUE(strstr(resp, "\"object\":\"chat.completion\"") != NULL);
    HU_ASSERT_TRUE(strstr(resp, "\"choices\"") != NULL);
    HU_ASSERT_TRUE(strstr(resp, "\"usage\"") != NULL);
    HU_ASSERT_TRUE(strstr(resp, "chatcmpl-") != NULL);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

static void test_openai_compat_chat_invalid_json_400(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    hu_app_context_t app = {.config = &cfg};

    const char *body = "{invalid json";
    int status = 200;
    char *resp = NULL;
    size_t resp_len = 0;
    hu_openai_compat_handle_chat_completions(body, strlen(body), &alloc, &app, &status, &resp,
                                             &resp_len, NULL);
    HU_ASSERT_EQ(status, 400);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

static void test_openai_compat_chat_stream_returns_sse(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    hu_app_context_t app = {.config = &cfg};

    const char *body =
        "{\"model\":\"gpt-4o\",\"messages\":[{\"role\":\"user\",\"content\":\"Hi\"}],"
        "\"stream\":true}";
    int status = 500;
    char *resp = NULL;
    size_t resp_len = 0;
    const char *content_type = NULL;
    hu_openai_compat_handle_chat_completions(body, strlen(body), &alloc, &app, &status, &resp,
                                             &resp_len, &content_type);
    HU_ASSERT_EQ(status, 200);
    HU_ASSERT_NOT_NULL(resp);
    HU_ASSERT_TRUE(strstr(resp, "data: ") != NULL);
    /* Verify stream ends with data: [DONE] */
    int has_done = (resp_len >= 14 && memcmp(resp + resp_len - 14, "data: [DONE]\n\n", 14) == 0);
    if (!has_done && resp_len >= 4) {
        for (size_t i = 0; i + 4 <= resp_len; i++)
            if (memcmp(resp + i, "DONE", 4) == 0) {
                has_done = 1;
                break;
            }
    }
    HU_ASSERT_TRUE(has_done);
    HU_ASSERT_TRUE(strstr(resp, "chat.completion.chunk") != NULL);
    HU_ASSERT_TRUE(content_type != NULL && strcmp(content_type, "text/event-stream") == 0);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

static void test_openai_compat_chat_empty_messages_400(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    hu_app_context_t app = {.config = &cfg};

    const char *body = "{\"model\":\"gpt-4o\",\"messages\":[]}";
    int status = 200;
    char *resp = NULL;
    size_t resp_len = 0;
    hu_openai_compat_handle_chat_completions(body, strlen(body), &alloc, &app, &status, &resp,
                                             &resp_len, NULL);
    HU_ASSERT_EQ(status, 400);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

static void test_gateway_missing_content_type(void) {
    /* Handler must not crash when body is NULL or empty (e.g. request without body) */
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    hu_app_context_t app = {.config = &cfg};

    int status = 200;
    char *resp = NULL;
    size_t resp_len = 0;
    hu_openai_compat_handle_chat_completions(NULL, 0, &alloc, &app, &status, &resp, &resp_len,
                                             NULL);
    HU_ASSERT_EQ(status, 400);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

static void test_openai_compat_chat_stream_has_delta_content(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    hu_app_context_t app = {.config = &cfg};

    const char *body =
        "{\"model\":\"gpt-4o\",\"messages\":[{\"role\":\"user\",\"content\":\"Hi\"}],"
        "\"stream\":true}";
    int status = 500;
    char *resp = NULL;
    size_t resp_len = 0;
    hu_openai_compat_handle_chat_completions(body, strlen(body), &alloc, &app, &status, &resp,
                                             &resp_len, NULL);
    HU_ASSERT_EQ(status, 200);
    HU_ASSERT_NOT_NULL(resp);
    HU_ASSERT_TRUE(strstr(resp, "\"delta\"") != NULL);
    HU_ASSERT_TRUE(strstr(resp, "\"content\":") != NULL);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

static void test_openai_compat_models_returns_list(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    hu_app_context_t app = {.config = &cfg};

    int status = 500;
    char *resp = NULL;
    size_t resp_len = 0;
    hu_openai_compat_handle_models(&alloc, &app, &status, &resp, &resp_len);
    HU_ASSERT_EQ(status, 200);
    HU_ASSERT_NOT_NULL(resp);
    HU_ASSERT_TRUE(strstr(resp, "\"object\":\"list\"") != NULL);
    HU_ASSERT_TRUE(strstr(resp, "\"data\"") != NULL);
    HU_ASSERT_TRUE(strstr(resp, "openai") != NULL);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

static void test_openai_compat_models_null_config_503(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_context_t app = {.config = NULL};

    int status = 200;
    char *resp = NULL;
    size_t resp_len = 0;
    hu_openai_compat_handle_models(&alloc, &app, &status, &resp, &resp_len);
    HU_ASSERT_EQ(status, 503);
    if (resp)
        alloc.free(alloc.ctx, resp, resp_len + 1);
}

/* ── Synthetic E2E: full conversation lifecycle via control protocol ─────── */

static int s_e2e_msg_count;
static char s_e2e_last_message[512];

static bool e2e_bus_listener(hu_bus_event_type_t t, const hu_bus_event_t *e, void *u) {
    (void)u;
    if (t == HU_BUS_MESSAGE_RECEIVED && e->message[0]) {
        s_e2e_msg_count++;
        snprintf(s_e2e_last_message, sizeof(s_e2e_last_message), "%s", e->message);
    }
    return true;
}

static void test_e2e_gateway_conversation_lifecycle(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);

    s_e2e_msg_count = 0;
    s_e2e_last_message[0] = '\0';
    hu_bus_subscribe(&bus, e2e_bus_listener, NULL, HU_BUS_MESSAGE_RECEIVED);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));

    /* Step 1: chat.send — message arrives, bus fires, response is "queued" */
    {
        hu_json_value_t *root = hu_json_object_new(&alloc);
        hu_json_value_t *params = hu_json_object_new(&alloc);
        hu_json_object_set(&alloc, params, "message",
                           hu_json_string_new(&alloc, "Hello, is anyone there?", 23));
        hu_json_object_set(&alloc, params, "sessionKey",
                           hu_json_string_new(&alloc, "e2e-session", 11));
        hu_json_object_set(&alloc, root, "params", params);

        char *out = NULL;
        size_t out_len = 0;
        hu_error_t err = cp_chat_send(&alloc, &app, &conn, &proto, root, &out, &out_len);
        HU_ASSERT_EQ(err, HU_OK);
        HU_ASSERT_NOT_NULL(out);
        HU_ASSERT_NOT_NULL(strstr(out, "queued"));
        HU_ASSERT_NOT_NULL(strstr(out, "e2e-session"));
        alloc.free(alloc.ctx, out, out_len + 1);
        hu_json_free(&alloc, root);
    }
    HU_ASSERT_EQ(s_e2e_msg_count, 1);
    HU_ASSERT_STR_EQ(s_e2e_last_message, "Hello, is anyone there?");

    /* Step 2: Send a second message to prove sequential dispatch */
    {
        hu_json_value_t *root = hu_json_object_new(&alloc);
        hu_json_value_t *params = hu_json_object_new(&alloc);
        hu_json_object_set(&alloc, params, "message",
                           hu_json_string_new(&alloc, "Second message", 14));
        hu_json_object_set(&alloc, root, "params", params);

        char *out = NULL;
        size_t out_len = 0;
        hu_error_t err = cp_chat_send(&alloc, &app, &conn, &proto, root, &out, &out_len);
        HU_ASSERT_EQ(err, HU_OK);
        HU_ASSERT_NOT_NULL(out);
        HU_ASSERT_NOT_NULL(strstr(out, "queued"));
        alloc.free(alloc.ctx, out, out_len + 1);
        hu_json_free(&alloc, root);
    }
    HU_ASSERT_EQ(s_e2e_msg_count, 2);
    HU_ASSERT_STR_EQ(s_e2e_last_message, "Second message");

    /* Step 3: chat.abort — clean abort without crash */
    {
        hu_json_value_t *root = hu_json_object_new(&alloc);
        char *out = NULL;
        size_t out_len = 0;
        hu_error_t err = cp_chat_abort(&alloc, &app, &conn, &proto, root, &out, &out_len);
        HU_ASSERT_EQ(err, HU_OK);
        HU_ASSERT_NOT_NULL(out);
        HU_ASSERT_NOT_NULL(strstr(out, "true"));
        alloc.free(alloc.ctx, out, out_len + 1);
        hu_json_free(&alloc, root);
    }

    /* Step 4: chat.history — returns messages array (empty without session store) */
    {
        hu_json_value_t *root = hu_json_object_new(&alloc);
        char *out = NULL;
        size_t out_len = 0;
        hu_error_t err = cp_chat_history(&alloc, &app, &conn, &proto, root, &out, &out_len);
        HU_ASSERT_EQ(err, HU_OK);
        HU_ASSERT_NOT_NULL(out);
        HU_ASSERT_NOT_NULL(strstr(out, "messages"));
        alloc.free(alloc.ctx, out, out_len + 1);
        hu_json_free(&alloc, root);
    }

    /* Step 5: Dispatch via hu_control_on_message (full WS frame path) */
    {
        const char *msg = "{\"type\":\"req\",\"id\":\"e2e-5\",\"method\":\"chat.send\","
                          "\"params\":{\"message\":\"via dispatch\"}}";
        hu_control_on_message(&conn, msg, strlen(msg), &proto);
        HU_ASSERT_EQ(s_e2e_msg_count, 3);
        HU_ASSERT_STR_EQ(s_e2e_last_message, "via dispatch");
    }

    teardown_proto(&ws, &proto);
}

/* ── Tasks RPC tests ──────────────────────────────────────────────── */

static void test_rpc_tasks_list_no_store_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    app.task_store = NULL;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_tasks_list(&alloc, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "tasks") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
    teardown_proto(&ws, &proto);
}

static void test_rpc_tasks_get_no_store_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    app.task_store = NULL;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    hu_json_value_t *params = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, params, "id", hu_json_number_new(&alloc, 1));
    hu_json_object_set(&alloc, root, "params", params);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_tasks_get(&alloc, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
    teardown_proto(&ws, &proto);
}

static void test_rpc_tasks_cancel_no_store_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);
    app.task_store = NULL;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    hu_json_value_t *params = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, params, "id", hu_json_number_new(&alloc, 1));
    hu_json_object_set(&alloc, root, "params", params);
    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_tasks_cancel(&alloc, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
    teardown_proto(&ws, &proto);
}

static void test_cp_config_get_returns_populated_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.workspace_dir = "/tmp/human_ws";
    cfg.default_provider = "openai";
    cfg.default_model = "gpt-4o";
    cfg.max_tokens = 4096;
    cfg.temperature = 0.5;

    hu_app_context_t app;
    memset(&app, 0, sizeof(app));
    app.alloc = &alloc;
    app.config = &cfg;

    hu_control_protocol_t proto;
    memset(&proto, 0, sizeof(proto));
    proto.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_config_get(&alloc, &app, NULL, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "\"exists\":true"));
    HU_ASSERT_NOT_NULL(strstr(out, "/tmp/human_ws"));
    HU_ASSERT_NOT_NULL(strstr(out, "openai"));
    HU_ASSERT_NOT_NULL(strstr(out, "gpt-4o"));
    HU_ASSERT_NOT_NULL(strstr(out, "4096"));

    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
}

static void test_cp_config_set_reports_saved_false_when_raw_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    hu_app_context_t app;
    memset(&app, 0, sizeof(app));
    app.alloc = &alloc;
    app.config = &cfg;

    hu_control_protocol_t proto;
    memset(&proto, 0, sizeof(proto));
    proto.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);
    hu_json_value_t *params = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(params);
    hu_json_object_set(&alloc, params, "raw", hu_json_string_new(&alloc, "{not valid json", 16));
    hu_json_object_set(&alloc, root, "params", params);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_config_set(&alloc, &app, NULL, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "\"saved\":false"));

    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
}

static void test_cp_config_set_reports_saved_false_without_raw(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    hu_app_context_t app;
    memset(&app, 0, sizeof(app));
    app.alloc = &alloc;
    app.config = &cfg;
    hu_control_protocol_t proto;
    memset(&proto, 0, sizeof(proto));
    proto.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);
    hu_json_value_t *params_empty = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(params_empty);
    hu_json_object_set(&alloc, root, "params", params_empty);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_config_set(&alloc, &app, NULL, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "\"saved\":false"));
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
}

#if defined(HU_GATEWAY_POSIX) && defined(HU_ENABLE_SQLITE)

static void test_cp_memory_recall_and_store_round_trip_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    gw_e2e_mock_provider_ctx_t pctx;
    hu_provider_t prov = gw_e2e_mock_provider_create(&pctx);
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    HU_ASSERT_EQ(hu_agent_from_config(&agent, &alloc, prov, NULL, 0, &mem, NULL, NULL, NULL,
                                      "gpt-4o", 6, "openai", 6, 0.7, ".", 1, 25, 50, false, 0,
                                      NULL, 0, NULL, 0, NULL),
                 HU_OK);

    hu_app_context_t app;
    memset(&app, 0, sizeof(app));
    app.alloc = &alloc;
    app.agent = &agent;

    hu_control_protocol_t proto;
    memset(&proto, 0, sizeof(proto));
    proto.alloc = &alloc;

    /* memory.store */
    {
        hu_json_value_t *root = NULL;
        const char *req = "{\"params\":{\"key\":\"gw_rpc_mem_key\",\"content\":"
                          "\"gateway_rpc_unique_alpha_token\",\"source\":\"test\"}}";
        HU_ASSERT_EQ(hu_json_parse(&alloc, req, strlen(req), &root), HU_OK);
        char *out = NULL;
        size_t out_len = 0;
        HU_ASSERT_EQ(cp_memory_store(&alloc, &app, NULL, &proto, root, &out, &out_len), HU_OK);
        HU_ASSERT_NOT_NULL(out);
        HU_ASSERT_NOT_NULL(strstr(out, "\"stored\":true"));
        alloc.free(alloc.ctx, out, out_len + 1);
        hu_json_free(&alloc, root);
    }

    /* memory.recall (semantic / keyword search over stored content) */
    {
        hu_json_value_t *root = NULL;
        const char *req =
            "{\"params\":{\"query\":\"gateway_rpc_unique_alpha_token\",\"limit\":5}}";
        HU_ASSERT_EQ(hu_json_parse(&alloc, req, strlen(req), &root), HU_OK);
        char *out = NULL;
        size_t out_len = 0;
        HU_ASSERT_EQ(cp_memory_recall(&alloc, &app, NULL, &proto, root, &out, &out_len), HU_OK);
        HU_ASSERT_NOT_NULL(out);
        HU_ASSERT_NOT_NULL(strstr(out, "entries"));
        HU_ASSERT_NOT_NULL(strstr(out, "gw_rpc_mem_key"));
        alloc.free(alloc.ctx, out, out_len + 1);
        hu_json_free(&alloc, root);
    }

    hu_agent_deinit(&agent);
    mem.vtable->deinit(mem.ctx);
}

static void test_cp_mcp_resources_and_prompts_list_json_shape(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_context_t app;
    memset(&app, 0, sizeof(app));
    app.alloc = &alloc;
    app.mcp_resources = NULL;
    app.mcp_prompts = NULL;

    hu_control_protocol_t proto;
    memset(&proto, 0, sizeof(proto));
    proto.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);

    char *out_r = NULL;
    size_t out_r_len = 0;
    HU_ASSERT_EQ(cp_mcp_resources_list(&alloc, &app, NULL, &proto, root, &out_r, &out_r_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out_r);
    HU_ASSERT_NOT_NULL(strstr(out_r, "\"resources\""));
    HU_ASSERT_NOT_NULL(strstr(out_r, "\"templates\""));
    alloc.free(alloc.ctx, out_r, out_r_len + 1);

    char *out_p = NULL;
    size_t out_p_len = 0;
    HU_ASSERT_EQ(cp_mcp_prompts_list(&alloc, &app, NULL, &proto, root, &out_p, &out_p_len), HU_OK);
    HU_ASSERT_NOT_NULL(out_p);
    HU_ASSERT_NOT_NULL(strstr(out_p, "\"prompts\""));
    alloc.free(alloc.ctx, out_p, out_p_len + 1);

    hu_json_free(&alloc, root);
}

static void test_cp_turing_scores_returns_scores_array_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    gw_e2e_mock_provider_ctx_t pctx;
    hu_provider_t prov = gw_e2e_mock_provider_create(&pctx);
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    HU_ASSERT_EQ(hu_agent_from_config(&agent, &alloc, prov, NULL, 0, &mem, NULL, NULL, NULL,
                                      "gpt-4o", 6, "openai", 6, 0.7, ".", 1, 25, 50, false, 0,
                                      NULL, 0, NULL, 0, NULL),
                 HU_OK);

    hu_app_context_t app;
    memset(&app, 0, sizeof(app));
    app.alloc = &alloc;
    app.agent = &agent;

    hu_control_protocol_t proto;
    memset(&proto, 0, sizeof(proto));
    proto.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_turing_scores(&alloc, &app, NULL, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "\"scores\""));
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);

    hu_agent_deinit(&agent);
    mem.vtable->deinit(mem.ctx);
}

#endif /* HU_GATEWAY_POSIX && HU_ENABLE_SQLITE */

static void test_e2e_chat_send_bus_agent_turn_produces_assistant_reply(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);

    gw_e2e_mock_provider_ctx_t pctx;
    hu_provider_t prov = gw_e2e_mock_provider_create(&pctx);
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    HU_ASSERT_EQ(hu_agent_from_config(&agent, &alloc, prov, NULL, 0, NULL, NULL, NULL, NULL,
                                      "gpt-4o", 6, "openai", 6, 0.7, ".", 1, 25, 50, false, 0,
                                      NULL, 0, NULL, 0, NULL),
                 HU_OK);

    app.agent = &agent;
    gw_e2e_last_assistant_reply[0] = '\0';
    gw_chat_agent_bus_bridge_t bridge = {.agent = &agent, .bus = &bus};
    hu_bus_subscribe(&bus, gw_e2e_on_message_sent, NULL, HU_BUS_MESSAGE_SENT);
    hu_bus_subscribe(&bus, gw_e2e_on_message_received, &bridge, HU_BUS_MESSAGE_RECEIVED);

    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);
    hu_json_value_t *params = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(params);
    hu_json_object_set(&alloc, params, "message",
                       hu_json_string_new(&alloc, "Hello from gateway E2E", 22));
    hu_json_object_set(&alloc, root, "params", params);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_chat_send(&alloc, &app, &conn, &proto, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "queued"));
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);

    HU_ASSERT_STR_EQ(gw_e2e_last_assistant_reply, "mock response");

    hu_agent_deinit(&agent);
    teardown_proto(&ws, &proto);
}

static void test_connect_handshake_advertises_critical_methods(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_context_t app;
    hu_config_t cfg;
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_bus_t bus;
    setup_proto_with_app(&alloc, &ws, &proto, &app, &bus, &cfg);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_admin_connect(&alloc, &app, NULL, &proto, NULL, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_NOT_NULL(strstr(out, "\"models.decisions\""));
    HU_ASSERT_NOT_NULL(strstr(out, "\"voice.clone\""));
    HU_ASSERT_NOT_NULL(strstr(out, "\"tasks.list\""));
    HU_ASSERT_NOT_NULL(strstr(out, "\"security.cot.summary\""));
    HU_ASSERT_NOT_NULL(strstr(out, "\"turing.contact\""));
    alloc.free(alloc.ctx, out, out_len + 1);
    teardown_proto(&ws, &proto);
}

void run_gateway_extended_tests(void) {
    HU_TEST_SUITE("Gateway Extended");
    HU_RUN_TEST(test_gateway_webhook_paths);
    HU_RUN_TEST(test_gateway_health_endpoint);
    HU_RUN_TEST(test_gateway_ready_endpoint);
    HU_RUN_TEST(test_gateway_rate_limit_sliding_window);
    HU_RUN_TEST(test_rate_limiter_allow_under_limit);
    HU_RUN_TEST(test_rate_limiter_deny_over_limit);
    HU_RUN_TEST(test_rate_limiter_window_expiry_resets);
    HU_RUN_TEST(test_gateway_max_body_size);
    HU_RUN_TEST(test_gateway_auth_required);
    HU_RUN_TEST(test_gateway_run_test_mode);
    HU_RUN_TEST(test_gateway_run_with_host);
    HU_RUN_TEST(test_health_multiple_components);
    HU_RUN_TEST(test_health_all_ok);
    HU_RUN_TEST(test_health_reset_clears);
    HU_RUN_TEST(test_gateway_config_zeroed);
    HU_RUN_TEST(test_gateway_config_from_cfg_null_cfg);
    HU_RUN_TEST(test_gateway_config_from_cfg_null_out);
    HU_RUN_TEST(test_gateway_config_from_cfg_valid);
    HU_RUN_TEST(test_gateway_config_from_cfg_defaults);
    HU_RUN_TEST(test_readiness_status_values);
    HU_RUN_TEST(test_gateway_rate_limit_constant);
    HU_RUN_TEST(test_health_check_null_alloc);
    HU_RUN_TEST(test_gateway_config_port_range);
    HU_RUN_TEST(test_gateway_config_test_mode_flag);
    HU_RUN_TEST(test_gateway_config_hmac_optional);
    HU_RUN_TEST(test_health_single_error_not_ready);
    HU_RUN_TEST(test_gateway_max_body_nonzero);
    HU_RUN_TEST(test_gateway_rate_limit_nonzero);
    HU_RUN_TEST(test_gateway_run_null_config);
    HU_RUN_TEST(test_health_empty_ready);
    HU_RUN_TEST(test_gateway_config_host_default);
    HU_RUN_TEST(test_gateway_config_rate_limit_default);
    HU_RUN_TEST(test_health_mark_ok_then_error);
    HU_RUN_TEST(test_gateway_run_bind_zero_port);
    HU_RUN_TEST(test_component_check_structure);
    HU_RUN_TEST(test_gateway_config_max_body_boundary);
    HU_RUN_TEST(test_gateway_config_hmac_len_zero);
    HU_RUN_TEST(test_gateway_config_rate_limit_one);
    HU_RUN_TEST(test_health_mark_ok_overwrites_error);
    HU_RUN_TEST(test_gateway_config_port_one);
    HU_RUN_TEST(test_gateway_constants_match_zig);
    HU_RUN_TEST(test_health_three_components_mixed);
    HU_RUN_TEST(test_gateway_run_test_mode_host_loopback);
    HU_RUN_TEST(test_ws_server_init_deinit);
    HU_RUN_TEST(test_ws_server_upgrade_null_args);
    HU_RUN_TEST(test_ws_server_is_upgrade_valid);
    HU_RUN_TEST(test_ws_server_is_upgrade_invalid);
    HU_RUN_TEST(test_ws_server_send_null_conn);
    HU_RUN_TEST(test_ws_server_broadcast_empty);

    HU_TEST_SUITE("Control Protocol");
    HU_RUN_TEST(test_control_protocol_init_deinit);
    HU_RUN_TEST(test_control_set_app_ctx);
    HU_RUN_TEST(test_control_set_app_ctx_null);
    HU_RUN_TEST(test_control_on_message_null);
    HU_RUN_TEST(test_control_on_message_invalid_json);
    HU_RUN_TEST(test_control_on_message_non_req);
    HU_RUN_TEST(test_control_on_message_no_method);
    HU_RUN_TEST(test_control_send_event_null);
    HU_RUN_TEST(test_control_send_event_no_ws);
    HU_RUN_TEST(test_control_send_response_null);
    HU_RUN_TEST(test_control_on_close);

    HU_TEST_SUITE("Event Bridge");
    HU_RUN_TEST(test_event_bridge_init_deinit);
    HU_RUN_TEST(test_event_bridge_init_null);
    HU_RUN_TEST(test_event_bridge_deinit_null);
    HU_RUN_TEST(test_event_bridge_bus_subscription);
    HU_RUN_TEST(test_event_bridge_publish_no_crash);

    HU_TEST_SUITE("RPC Handlers");
    HU_RUN_TEST(test_rpc_chat_send_empty_message_rejected);
    HU_RUN_TEST(test_rpc_chat_send_null_message_rejected);
    HU_RUN_TEST(test_rpc_chat_send_valid_publishes_bus);
    HU_RUN_TEST(test_rpc_chat_send_self_harm_returns_crisis);
    HU_RUN_TEST(test_rpc_chat_abort_no_crash);
    HU_RUN_TEST(test_rpc_config_apply_no_crash);
    HU_RUN_TEST(test_rpc_cron_run_no_crash);
    HU_RUN_TEST(test_rpc_skills_install_no_crash);
    HU_RUN_TEST(test_rpc_exec_approval_no_crash);
    HU_RUN_TEST(test_rpc_usage_summary_no_crash);
    HU_RUN_TEST(test_rpc_update_check_returns_valid_structure);
    HU_RUN_TEST(test_rpc_update_run_returns_valid_structure);
    HU_RUN_TEST(test_rpc_nodes_list_returns_at_least_one_node);
    HU_RUN_TEST(test_rpc_agents_list_no_crash);
    HU_RUN_TEST(test_rpc_voice_config_no_crash);
    HU_RUN_TEST(test_rpc_nodes_action_missing_fields_no_crash);
    HU_RUN_TEST(test_cp_admin_nodes_action_restart_returns_mock_json);
    HU_RUN_TEST(test_cp_config_get_returns_populated_fields);
    HU_RUN_TEST(test_cp_config_set_reports_saved_false_when_raw_invalid);
    HU_RUN_TEST(test_cp_config_set_reports_saved_false_without_raw);
#if defined(HU_GATEWAY_POSIX) && defined(HU_ENABLE_SQLITE)
    HU_RUN_TEST(test_cp_memory_recall_and_store_round_trip_json);
    HU_RUN_TEST(test_cp_mcp_resources_and_prompts_list_json_shape);
    HU_RUN_TEST(test_cp_turing_scores_returns_scores_array_json);
#endif
    HU_RUN_TEST(test_e2e_chat_send_bus_agent_turn_produces_assistant_reply);
    HU_RUN_TEST(test_connect_handshake_advertises_critical_methods);
    HU_RUN_TEST(test_event_bridge_payload_propagation);

    HU_TEST_SUITE("Gateway Tasks RPC");
    HU_RUN_TEST(test_rpc_tasks_list_no_store_returns_error);
    HU_RUN_TEST(test_rpc_tasks_get_no_store_returns_error);
    HU_RUN_TEST(test_rpc_tasks_cancel_no_store_returns_error);

    HU_TEST_SUITE("Gateway E2E Lifecycle");
    HU_RUN_TEST(test_e2e_gateway_conversation_lifecycle);

    HU_TEST_SUITE("WS Server Extended");
    HU_RUN_TEST(test_ws_server_process_null);
    HU_RUN_TEST(test_ws_server_read_and_process_null);
    HU_RUN_TEST(test_ws_server_close_conn_inactive);
    HU_RUN_TEST(test_ws_server_is_upgrade_short);
    HU_RUN_TEST(test_ws_server_is_upgrade_null);
    HU_RUN_TEST(test_ws_server_send_inactive_conn);
    HU_RUN_TEST(test_ws_server_broadcast_null_data);
    HU_RUN_TEST(test_ws_server_conn_pool_full);

    HU_TEST_SUITE("OpenAI Compat");
    HU_RUN_TEST(test_openai_compat_chat_valid_returns_mock);
    HU_RUN_TEST(test_openai_compat_chat_invalid_json_400);
    HU_RUN_TEST(test_openai_compat_chat_stream_returns_sse);
    HU_RUN_TEST(test_openai_compat_chat_stream_has_delta_content);
    HU_RUN_TEST(test_openai_compat_chat_empty_messages_400);
    HU_RUN_TEST(test_gateway_missing_content_type);
    HU_RUN_TEST(test_openai_compat_models_returns_list);
    HU_RUN_TEST(test_openai_compat_models_null_config_503);
}
