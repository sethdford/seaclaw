/* Gateway WebSocket RPC authentication and pairing tests. */
#include "seaclaw/bus.h"
#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/arena.h"
#include "seaclaw/core/json.h"
#include "seaclaw/gateway/control_protocol.h"
#include "seaclaw/gateway/ws_server.h"
#include "seaclaw/security.h"
#include "test_framework.h"
#include <string.h>

#ifdef SC_GATEWAY_POSIX
#include <sys/socket.h>
#include <unistd.h>
#endif

static char s_last_response[1024];
static size_t s_last_response_len;

static void capture_response(sc_ws_conn_t *conn, const char *data, size_t data_len, void *ctx) {
    (void)conn;
    (void)ctx;
    if (data_len < sizeof(s_last_response)) {
        memcpy(s_last_response, data, data_len);
        s_last_response[data_len] = '\0';
        s_last_response_len = data_len;
    }
}

/* Send RPC message and capture response via socketpair (control protocol sends to conn->fd).
 * The response is a WebSocket text frame; we read the raw bytes and search for JSON. */
static void send_and_capture(sc_ws_conn_t *conn, sc_control_protocol_t *proto, const char *msg) {
#ifdef SC_GATEWAY_POSIX
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        s_last_response_len = 0;
        return;
    }
    conn->fd = fds[1];
    conn->active = true;
    s_last_response_len = 0;
    sc_control_on_message(conn, msg, strlen(msg), proto);
    shutdown(fds[1], SHUT_WR);
    close(fds[1]);
    conn->fd = -1;
    ssize_t n = read(fds[0], s_last_response, sizeof(s_last_response) - 1);
    close(fds[0]);
    if (n > 0) {
        s_last_response[n] = '\0';
        s_last_response_len = (size_t)n;
    } else {
        s_last_response_len = 0;
    }
#else
    (void)conn;
    (void)proto;
    (void)msg;
    s_last_response_len = 0;
#endif
}

static void setup_proto_with_auth(sc_allocator_t *alloc, sc_ws_server_t *ws,
                                  sc_control_protocol_t *proto, sc_app_context_t *app,
                                  sc_bus_t *bus, sc_config_t *cfg, bool require_pairing,
                                  sc_pairing_guard_t *pairing_guard, const char *auth_token) {
    sc_ws_server_init(ws, alloc, capture_response, NULL, proto);
    sc_control_protocol_init(proto, alloc, ws);
    memset(app, 0, sizeof(*app));
    memset(cfg, 0, sizeof(*cfg));
    sc_arena_t *arena = sc_arena_create(*alloc);
    if (arena) {
        cfg->arena = arena;
        cfg->allocator = sc_arena_allocator(arena);
    }
    sc_bus_init(bus);
    app->config = cfg;
    app->alloc = alloc;
    app->bus = bus;
    sc_control_set_app_ctx(proto, app);
    sc_control_set_auth(proto, require_pairing, pairing_guard, auth_token);
}

static void teardown_proto(sc_ws_server_t *ws, sc_control_protocol_t *proto,
                           sc_pairing_guard_t *guard, sc_config_t *cfg) {
    sc_control_protocol_deinit(proto);
    sc_ws_server_deinit(ws);
    if (guard)
        sc_pairing_guard_destroy(guard);
    if (cfg && cfg->arena) {
        sc_arena_destroy(cfg->arena);
        cfg->arena = NULL;
    }
}

static void test_rpc_unauthorized_when_pairing_required(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, NULL, NULL);

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    const char *msg = "{\"type\":\"req\",\"id\":\"1\",\"method\":\"config.set\","
                      "\"params\":{\"raw\":\"{}\"}}";
    send_and_capture(&conn, &proto, msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":false") != NULL);
    SC_ASSERT_TRUE(strstr(s_last_response, "unauthorized") != NULL);
    SC_ASSERT_TRUE(strstr(s_last_response, "Authentication required") != NULL);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_rpc_health_no_auth_needed(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, NULL, NULL);

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    const char *msg = "{\"type\":\"req\",\"id\":\"2\",\"method\":\"health\"}";
    send_and_capture(&conn, &proto, msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);
    SC_ASSERT_TRUE(strstr(s_last_response, "ok") != NULL);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_rpc_auth_with_valid_token(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, NULL, "test-secret-token");

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    const char *msg = "{\"type\":\"req\",\"id\":\"3\",\"method\":\"auth.token\","
                      "\"params\":{\"token\":\"test-secret-token\"}}";
    send_and_capture(&conn, &proto, msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);
    SC_ASSERT_TRUE(strstr(s_last_response, "authenticated") != NULL);
    SC_ASSERT_TRUE(conn.authenticated);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_rpc_auth_with_invalid_token(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, NULL, "valid-token");

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    const char *msg = "{\"type\":\"req\",\"id\":\"4\",\"method\":\"auth.token\","
                      "\"params\":{\"token\":\"wrong-token\"}}";
    send_and_capture(&conn, &proto, msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":false") != NULL);
    SC_ASSERT_TRUE(strstr(s_last_response, "invalid_token") != NULL);
    SC_ASSERT_FALSE(conn.authenticated);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_rpc_no_auth_when_pairing_disabled(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, false, NULL, NULL);

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    const char *msg = "{\"type\":\"req\",\"id\":\"5\",\"method\":\"config.set\","
                      "\"params\":{\"raw\":\"{}\"}}";
    send_and_capture(&conn, &proto, msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_rpc_connect_no_auth_needed(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, NULL, NULL);

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    const char *msg = "{\"type\":\"req\",\"id\":\"6\",\"method\":\"connect\"}";
    send_and_capture(&conn, &proto, msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);
    SC_ASSERT_TRUE(strstr(s_last_response, "hello-ok") != NULL);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_rpc_capabilities_no_auth_needed(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, NULL, NULL);

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    const char *msg = "{\"type\":\"req\",\"id\":\"7\",\"method\":\"capabilities\"}";
    send_and_capture(&conn, &proto, msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_rpc_auth_token_after_valid_allows_protected(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, NULL, "my-token");

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    /* First: auth.token to authenticate */
    const char *auth_msg = "{\"type\":\"req\",\"id\":\"a1\",\"method\":\"auth.token\","
                           "\"params\":{\"token\":\"my-token\"}}";
    send_and_capture(&conn, &proto, auth_msg);
    SC_ASSERT_TRUE(conn.authenticated);

    /* Then: config.set should now succeed */
    const char *set_msg = "{\"type\":\"req\",\"id\":\"a2\",\"method\":\"config.set\","
                          "\"params\":{\"raw\":\"{}\"}}";
    send_and_capture(&conn, &proto, set_msg);

    SC_ASSERT_TRUE(s_last_response_len > 0);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);

    teardown_proto(&ws, &proto, NULL, &cfg);
}

static void test_pairing_flow(void) {
#ifdef SC_GATEWAY_POSIX
    sc_allocator_t alloc = sc_system_allocator();
    sc_ws_server_t ws;
    sc_control_protocol_t proto;
    sc_app_context_t app;
    sc_bus_t bus;
    sc_config_t cfg;
    sc_pairing_guard_t *guard = sc_pairing_guard_create(&alloc, true, NULL, 0);
    SC_ASSERT_NOT_NULL(guard);

    const char *code = sc_pairing_guard_pairing_code(guard);
    SC_ASSERT_NOT_NULL(code);

    char *token = NULL;
    sc_pair_attempt_result_t result = sc_pairing_guard_attempt_pair(guard, code, &token);
    SC_ASSERT_EQ(result, SC_PAIR_PAIRED);
    SC_ASSERT_NOT_NULL(token);

    setup_proto_with_auth(&alloc, &ws, &proto, &app, &bus, &cfg, true, guard, NULL);

    sc_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.active = true;
    conn.authenticated = false;

    /* auth.token with paired token (token is hex, no JSON escaping needed) */
    char auth_msg[320];
    int n = snprintf(auth_msg, sizeof(auth_msg),
                     "{\"type\":\"req\",\"id\":\"pf1\",\"method\":\"auth.token\","
                     "\"params\":{\"token\":\"%s\"}}",
                     token);
    SC_ASSERT_TRUE(n > 0 && (size_t)n < sizeof(auth_msg));

    send_and_capture(&conn, &proto, auth_msg);
    SC_ASSERT_TRUE(conn.authenticated);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);

    /* config.set should now succeed */
    const char *set_msg = "{\"type\":\"req\",\"id\":\"pf2\",\"method\":\"config.set\","
                          "\"params\":{\"raw\":\"{}\"}}";
    send_and_capture(&conn, &proto, set_msg);
    SC_ASSERT_TRUE(strstr(s_last_response, "\"ok\":true") != NULL);

    alloc.free(alloc.ctx, token, strlen(token) + 1);
    teardown_proto(&ws, &proto, guard, &cfg);
#else
    (void)0; /* skip */
#endif
}

void run_gateway_auth_tests(void) {
    SC_TEST_SUITE("Gateway Auth");
    SC_RUN_TEST(test_rpc_unauthorized_when_pairing_required);
    SC_RUN_TEST(test_rpc_health_no_auth_needed);
    SC_RUN_TEST(test_rpc_auth_with_valid_token);
    SC_RUN_TEST(test_rpc_auth_with_invalid_token);
    SC_RUN_TEST(test_rpc_no_auth_when_pairing_disabled);
    SC_RUN_TEST(test_rpc_connect_no_auth_needed);
    SC_RUN_TEST(test_rpc_capabilities_no_auth_needed);
    SC_RUN_TEST(test_rpc_auth_token_after_valid_allows_protected);
    SC_RUN_TEST(test_pairing_flow);
}
