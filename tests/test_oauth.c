#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/gateway/oauth.h"
#include "test_framework.h"
#include <string.h>
#include <time.h>

static void test_oauth_init_and_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_oauth_config_t cfg = {.provider = "google",
                             .client_id = "cid",
                             .client_secret = "cs",
                             .redirect_uri = "https://example.com/cb"};
    sc_oauth_ctx_t *ctx = NULL;
    sc_error_t err = sc_oauth_init(&alloc, &cfg, &ctx);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ctx);
    sc_oauth_destroy(ctx);
}

static void test_oauth_init_null_fails(void) {
    sc_error_t err = sc_oauth_init(NULL, NULL, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_oauth_pkce_generation(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_oauth_config_t cfg = {.provider = "google"};
    sc_oauth_ctx_t *ctx = NULL;
    sc_oauth_init(&alloc, &cfg, &ctx);
    char verifier[64] = {0};
    char challenge[64] = {0};
    sc_error_t err =
        sc_oauth_generate_pkce(ctx, verifier, sizeof(verifier), challenge, sizeof(challenge));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(strlen(verifier) == 43);
    SC_ASSERT_TRUE(strlen(challenge) > 0);
    sc_oauth_destroy(ctx);
}

static void test_oauth_build_auth_url(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_oauth_config_t cfg = {.provider = "google",
                             .client_id = "test-id",
                             .redirect_uri = "https://localhost:3000/auth/callback",
                             .scopes = "openid email"};
    sc_oauth_ctx_t *ctx = NULL;
    sc_oauth_init(&alloc, &cfg, &ctx);
    char url[1024] = {0};
    sc_error_t err = sc_oauth_build_auth_url(ctx, "challenge", 9, "state123", 8, url, sizeof(url));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(strstr(url, "accounts.google.com") != NULL);
    SC_ASSERT_TRUE(strstr(url, "test-id") != NULL);
    SC_ASSERT_TRUE(strstr(url, "challenge") != NULL);
    SC_ASSERT_TRUE(strstr(url, "S256") != NULL);
    sc_oauth_destroy(ctx);
}

static void test_oauth_exchange_code_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_oauth_config_t cfg = {.provider = "google",
                             .client_id = "cid",
                             .client_secret = "cs",
                             .redirect_uri = "https://example.com/cb"};
    sc_oauth_ctx_t *ctx = NULL;
    sc_oauth_init(&alloc, &cfg, &ctx);
    sc_oauth_session_t session = {0};
    sc_error_t err = sc_oauth_exchange_code(ctx, "test-code", 9, "test-verifier", 13, &session);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(strlen(session.session_id) > 0);
    SC_ASSERT_TRUE(strlen(session.access_token) > 0);
    SC_ASSERT_TRUE(sc_oauth_session_valid(&session));
    sc_oauth_destroy(ctx);
}

static void test_oauth_refresh_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_oauth_config_t cfg = {.provider = "google"};
    sc_oauth_ctx_t *ctx = NULL;
    sc_oauth_init(&alloc, &cfg, &ctx);
    sc_oauth_session_t session = {0};
    snprintf(session.session_id, sizeof(session.session_id), "test-sess");
    snprintf(session.access_token, sizeof(session.access_token), "old-token");
    session.expires_at = (int64_t)time(NULL) - 100;
    SC_ASSERT_FALSE(sc_oauth_session_valid(&session));
    sc_error_t err = sc_oauth_refresh_token(ctx, &session);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(sc_oauth_session_valid(&session));
    sc_oauth_destroy(ctx);
}

static void test_oauth_session_expired(void) {
    sc_oauth_session_t session = {0};
    snprintf(session.session_id, sizeof(session.session_id), "expired");
    session.expires_at = (int64_t)time(NULL) - 1;
    SC_ASSERT_FALSE(sc_oauth_session_valid(&session));
}

static void test_oauth_session_empty_invalid(void) {
    sc_oauth_session_t session = {0};
    SC_ASSERT_FALSE(sc_oauth_session_valid(&session));
    SC_ASSERT_FALSE(sc_oauth_session_valid(NULL));
}

void run_oauth_tests(void) {
    SC_RUN_TEST(test_oauth_init_and_destroy);
    SC_RUN_TEST(test_oauth_init_null_fails);
    SC_RUN_TEST(test_oauth_pkce_generation);
    SC_RUN_TEST(test_oauth_build_auth_url);
    SC_RUN_TEST(test_oauth_exchange_code_mock);
    SC_RUN_TEST(test_oauth_refresh_mock);
    SC_RUN_TEST(test_oauth_session_expired);
    SC_RUN_TEST(test_oauth_session_empty_invalid);
}
