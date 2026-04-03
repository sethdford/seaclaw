#include "human/oauth.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ── PKCE Verifier Generation ──────────────────────────────────────────── */

static void test_pkce_verifier_generation(void) {
    hu_oauth_pkce_t pkce;
    memset(&pkce, 0, sizeof(pkce));

    hu_error_t err = hu_mcp_oauth_pkce_generate(&pkce);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(pkce.verifier);

    size_t len = strlen(pkce.verifier);
    HU_ASSERT(len >= 43 && len <= 128);
}

static void test_pkce_verifier_charset(void) {
    hu_oauth_pkce_t pkce;
    memset(&pkce, 0, sizeof(pkce));

    hu_error_t err = hu_mcp_oauth_pkce_generate(&pkce);
    HU_ASSERT_EQ(err, HU_OK);

    /* Verify all characters are from [A-Za-z0-9-._~] */
    for (const char *p = pkce.verifier; *p; p++) {
        char c = *p;
        bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
        HU_ASSERT(valid);
    }
}

static void test_pkce_different_verifiers(void) {
    hu_oauth_pkce_t p1, p2;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));

    hu_error_t err1 = hu_mcp_oauth_pkce_generate(&p1);
    hu_error_t err2 = hu_mcp_oauth_pkce_generate(&p2);

    HU_ASSERT_EQ(err1, HU_OK);
    HU_ASSERT_EQ(err2, HU_OK);

    /* Verifiers should be different (extremely high probability) */
    HU_ASSERT(strcmp(p1.verifier, p2.verifier) != 0);
}

/* ── PKCE Challenge (S256) ──────────────────────────────────────────────── */

static void test_pkce_challenge_generation(void) {
    hu_oauth_pkce_t pkce;
    memset(&pkce, 0, sizeof(pkce));

    hu_error_t err = hu_mcp_oauth_pkce_generate(&pkce);
    HU_ASSERT_EQ(err, HU_OK);

    char challenge[64];
    err = hu_mcp_oauth_pkce_challenge(pkce.verifier, challenge, sizeof(challenge));
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(challenge[0] != '\0');
}

static void test_pkce_challenge_deterministic(void) {
    /* Same verifier should produce same challenge */
    const char *verifier = "TestVerifier123456789012345678901234567890";

    char challenge1[64], challenge2[64];
    hu_error_t err1 = hu_mcp_oauth_pkce_challenge(verifier, challenge1, sizeof(challenge1));
    hu_error_t err2 = hu_mcp_oauth_pkce_challenge(verifier, challenge2, sizeof(challenge2));

    HU_ASSERT_EQ(err1, HU_OK);
    HU_ASSERT_EQ(err2, HU_OK);
    HU_ASSERT_STR_EQ(challenge1, challenge2);
}

static void test_pkce_challenge_different_verifiers(void) {
    const char *v1 = "Verifier1234567890123456789012345678901234";
    const char *v2 = "Verifier1234567890123456789012345678901245";

    char c1[64], c2[64];
    hu_error_t err1 = hu_mcp_oauth_pkce_challenge(v1, c1, sizeof(c1));
    hu_error_t err2 = hu_mcp_oauth_pkce_challenge(v2, c2, sizeof(c2));

    HU_ASSERT_EQ(err1, HU_OK);
    HU_ASSERT_EQ(err2, HU_OK);
    HU_ASSERT(strcmp(c1, c2) != 0);
}

static void test_pkce_challenge_buffer_too_small(void) {
    const char *verifier = "Test123456789012345678901234567890123456";
    char challenge[10];  /* Too small */

    hu_error_t err = hu_mcp_oauth_pkce_challenge(verifier, challenge, sizeof(challenge));
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* ── Base64url Encoding ─────────────────────────────────────────────────── */

static void test_base64url_simple(void) {
    const uint8_t input[] = {0x01, 0x02, 0x03};
    char output[16];
    size_t len = 0;

    hu_error_t err = hu_mcp_base64url_encode(input, sizeof(input), output, sizeof(output), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(output[0] != '\0');
    HU_ASSERT(len > 0);
}

static void test_base64url_no_padding(void) {
    /* base64url should NOT include '=' padding */
    const uint8_t input[] = {0x01, 0x02};
    char output[16];
    size_t len = 0;

    hu_error_t err = hu_mcp_base64url_encode(input, sizeof(input), output, sizeof(output), &len);
    HU_ASSERT_EQ(err, HU_OK);

    /* Check for no padding characters */
    HU_ASSERT(strchr(output, '=') == NULL);
}

static void test_base64url_charset(void) {
    /* Verify base64url uses - and _ instead of + and / */
    const uint8_t input[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA};
    char output[32];
    size_t len = 0;

    hu_error_t err = hu_mcp_base64url_encode(input, sizeof(input), output, sizeof(output), &len);
    HU_ASSERT_EQ(err, HU_OK);

    /* Verify all characters are valid base64url ([A-Za-z0-9-_]) */
    for (size_t i = 0; i < len; i++) {
        char c = output[i];
        bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '_';
        HU_ASSERT(valid);
    }
}

static void test_base64url_buffer_too_small(void) {
    const uint8_t input[] = {0x01, 0x02, 0x03, 0x04};
    char output[4];
    size_t len = 0;

    hu_error_t err = hu_mcp_base64url_encode(input, sizeof(input), output, sizeof(output), &len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* ── OAuth Authorization URL ────────────────────────────────────────────── */

static void test_oauth_build_auth_url(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_oauth_config_t config = {
        .client_id = "test_client",
        .client_id_len = strlen("test_client"),
        .auth_url = "https://example.com/oauth/authorize",
        .auth_url_len = strlen("https://example.com/oauth/authorize"),
        .redirect_uri = "http://localhost:8888/callback",
        .redirect_uri_len = strlen("http://localhost:8888/callback"),
        .scopes = "read write",
        .scopes_len = strlen("read write"),
    };

    hu_oauth_pkce_t pkce;
    memset(&pkce, 0, sizeof(pkce));
    strcpy(pkce.challenge, "test_challenge_xxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    char *url = NULL;
    size_t url_len = 0;
    hu_error_t err = hu_mcp_oauth_build_auth_url(&alloc, &config, &pkce, "state_xyz", &url, &url_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(url);
    HU_ASSERT(url_len > 0);

    /* Verify URL contains expected parameters */
    HU_ASSERT_STR_CONTAINS(url, "client_id=test_client");
    HU_ASSERT_STR_CONTAINS(url, "redirect_uri=http://localhost:8888/callback");
    HU_ASSERT_STR_CONTAINS(url, "state=state_xyz");
    HU_ASSERT_STR_CONTAINS(url, "code_challenge=test_challenge");
    HU_ASSERT_STR_CONTAINS(url, "code_challenge_method=S256");

    alloc.free(alloc.ctx, url, url_len + 1);
}

static void test_oauth_build_auth_url_all_params(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_oauth_config_t config = {
        .client_id = "github_app",
        .client_id_len = strlen("github_app"),
        .auth_url = "https://github.com/login/oauth/authorize",
        .auth_url_len = strlen("https://github.com/login/oauth/authorize"),
        .redirect_uri = "http://localhost:9999/auth/callback",
        .redirect_uri_len = strlen("http://localhost:9999/auth/callback"),
        .scopes = "repo user gist",
        .scopes_len = strlen("repo user gist"),
    };

    hu_oauth_pkce_t pkce;
    hu_error_t err = hu_mcp_oauth_pkce_generate(&pkce);
    HU_ASSERT_EQ(err, HU_OK);

    char *url = NULL;
    size_t url_len = 0;
    err = hu_mcp_oauth_build_auth_url(&alloc, &config, &pkce, "random_state_123", &url, &url_len);

    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(url);
    HU_ASSERT(url_len > 100);  /* Should be substantial length */

    alloc.free(alloc.ctx, url, url_len + 1);
}

/* ── Token Expiration ────────────────────────────────────────────────────── */

static void test_token_not_expired(void) {
    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));

    token.expires_at = (int64_t)time(NULL) + 3600;  /* 1 hour from now */
    HU_ASSERT_FALSE(hu_mcp_oauth_token_is_expired(&token));
}

static void test_token_is_expired(void) {
    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));

    token.expires_at = (int64_t)time(NULL) - 3600;  /* 1 hour ago */
    HU_ASSERT_TRUE(hu_mcp_oauth_token_is_expired(&token));
}

static void test_token_never_expires(void) {
    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));

    token.expires_at = 0;  /* Never expires */
    HU_ASSERT_FALSE(hu_mcp_oauth_token_is_expired(&token));
}

/* ── Token Free (No Leaks) ──────────────────────────────────────────────── */

static void test_token_free_no_leaks(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));

    /* Allocate token fields */
    token.access_token = (char *)alloc.alloc(alloc.ctx, 64);
    strcpy(token.access_token, "test_access_token");
    token.access_token_len = strlen(token.access_token);

    token.refresh_token = (char *)alloc.alloc(alloc.ctx, 64);
    strcpy(token.refresh_token, "test_refresh_token");
    token.refresh_token_len = strlen(token.refresh_token);

    token.token_type = (char *)alloc.alloc(alloc.ctx, 16);
    strcpy(token.token_type, "Bearer");
    token.token_type_len = strlen(token.token_type);

    (void)hu_tracking_allocator_total_allocated(ta);
    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 3);

    /* Free token */
    hu_mcp_oauth_token_free(&alloc, &token);

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    HU_ASSERT(hu_tracking_allocator_total_freed(ta) > 0);

    hu_tracking_allocator_destroy(ta);
}

static void test_token_free_null_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));

    /* Should not crash when fields are NULL */
    hu_mcp_oauth_token_free(&alloc, &token);
    HU_ASSERT(true);
}

static void test_token_free_idempotent(void) {
    hu_tracking_allocator_t *ta = hu_tracking_allocator_create();
    hu_allocator_t alloc = hu_tracking_allocator_allocator(ta);

    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));

    token.access_token = (char *)alloc.alloc(alloc.ctx, 32);
    strcpy(token.access_token, "token");
    token.access_token_len = 5;

    hu_mcp_oauth_token_free(&alloc, &token);
    hu_mcp_oauth_token_free(&alloc, &token);  /* Second free should be safe */

    HU_ASSERT_EQ(hu_tracking_allocator_leaks(ta), 0);
    hu_tracking_allocator_destroy(ta);
}

/* ── Token Save/Load ───────────────────────────────────────────────────── */

static void test_token_save_creates_file(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char tmpfile[] = "/tmp/oauth_tokens_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd >= 0) close(fd);
    unlink(tmpfile);  /* Remove temp file, we'll create it */

    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));

    token.access_token = "test_access_token";
    token.access_token_len = strlen(token.access_token);
    token.token_type = "Bearer";
    token.token_type_len = strlen(token.token_type);
    token.expires_at = (int64_t)time(NULL) + 3600;

    hu_error_t err = hu_mcp_oauth_token_save(&alloc, tmpfile, "github", &token);
    HU_ASSERT_EQ(err, HU_OK);

    /* Check file exists */
    FILE *f = fopen(tmpfile, "rb");
    HU_ASSERT_NOT_NULL(f);
    if (f) fclose(f);

    unlink(tmpfile);
}

static void test_token_load_from_saved_file(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char tmpfile[] = "/tmp/oauth_tokens_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd >= 0) close(fd);
    unlink(tmpfile);

    /* Save token */
    hu_oauth_token_t saved_token;
    memset(&saved_token, 0, sizeof(saved_token));
    saved_token.access_token = "my_secret_access_token";
    saved_token.access_token_len = strlen(saved_token.access_token);
    saved_token.refresh_token = "my_refresh_token";
    saved_token.refresh_token_len = strlen(saved_token.refresh_token);
    saved_token.token_type = "Bearer";
    saved_token.token_type_len = strlen(saved_token.token_type);
    saved_token.expires_at = 1234567890;

    hu_error_t err = hu_mcp_oauth_token_save(&alloc, tmpfile, "slack", &saved_token);
    HU_ASSERT_EQ(err, HU_OK);

    /* Load token */
    hu_oauth_token_t loaded_token;
    err = hu_mcp_oauth_token_load(&alloc, tmpfile, "slack", &loaded_token);
    HU_ASSERT_EQ(err, HU_OK);

    /* Verify fields */
    HU_ASSERT_NOT_NULL(loaded_token.access_token);
    HU_ASSERT_STR_EQ(loaded_token.access_token, "my_secret_access_token");
    HU_ASSERT_NOT_NULL(loaded_token.refresh_token);
    HU_ASSERT_STR_EQ(loaded_token.refresh_token, "my_refresh_token");
    HU_ASSERT_NOT_NULL(loaded_token.token_type);
    HU_ASSERT_STR_EQ(loaded_token.token_type, "Bearer");
    HU_ASSERT_EQ(loaded_token.expires_at, 1234567890);

    hu_mcp_oauth_token_free(&alloc, &loaded_token);
    unlink(tmpfile);
}

static void test_token_save_overwrites_existing(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char tmpfile[] = "/tmp/oauth_tokens_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd >= 0) close(fd);
    unlink(tmpfile);

    /* Save first token */
    hu_oauth_token_t token1;
    memset(&token1, 0, sizeof(token1));
    token1.access_token = "token1_value";
    token1.access_token_len = strlen(token1.access_token);
    token1.token_type = "Bearer";
    token1.token_type_len = strlen(token1.token_type);

    hu_error_t err = hu_mcp_oauth_token_save(&alloc, tmpfile, "server1", &token1);
    HU_ASSERT_EQ(err, HU_OK);

    /* Save second token for same server */
    hu_oauth_token_t token2;
    memset(&token2, 0, sizeof(token2));
    token2.access_token = "token2_value_updated";
    token2.access_token_len = strlen(token2.access_token);
    token2.token_type = "Bearer";
    token2.token_type_len = strlen(token2.token_type);

    err = hu_mcp_oauth_token_save(&alloc, tmpfile, "server1", &token2);
    HU_ASSERT_EQ(err, HU_OK);

    /* Load and verify it's the updated token */
    hu_oauth_token_t loaded_token;
    err = hu_mcp_oauth_token_load(&alloc, tmpfile, "server1", &loaded_token);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(loaded_token.access_token, "token2_value_updated");

    hu_mcp_oauth_token_free(&alloc, &loaded_token);
    unlink(tmpfile);
}

static void test_token_load_not_found(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_oauth_token_t token;
    hu_error_t err = hu_mcp_oauth_token_load(&alloc, "/tmp/nonexistent_oauth_file_xyz", "server", &token);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);
}

static void test_token_load_wrong_server_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char tmpfile[] = "/tmp/oauth_tokens_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd >= 0) close(fd);
    unlink(tmpfile);

    /* Save token for "server1" */
    hu_oauth_token_t token;
    memset(&token, 0, sizeof(token));
    token.access_token = "test_token";
    token.access_token_len = strlen(token.access_token);
    token.token_type = "Bearer";
    token.token_type_len = strlen(token.token_type);

    hu_error_t err = hu_mcp_oauth_token_save(&alloc, tmpfile, "server1", &token);
    HU_ASSERT_EQ(err, HU_OK);

    /* Try to load with different server name */
    hu_oauth_token_t loaded_token;
    err = hu_mcp_oauth_token_load(&alloc, tmpfile, "server2", &loaded_token);
    HU_ASSERT_EQ(err, HU_ERR_NOT_FOUND);

    unlink(tmpfile);
}

static void test_token_save_multiple_servers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char tmpfile[] = "/tmp/oauth_tokens_test_XXXXXX";
    int fd = mkstemp(tmpfile);
    if (fd >= 0) close(fd);
    unlink(tmpfile);

    /* Save token for server1 */
    hu_oauth_token_t token1;
    memset(&token1, 0, sizeof(token1));
    token1.access_token = "token_for_server1";
    token1.access_token_len = strlen(token1.access_token);
    token1.token_type = "Bearer";
    token1.token_type_len = strlen(token1.token_type);

    hu_error_t err = hu_mcp_oauth_token_save(&alloc, tmpfile, "server1", &token1);
    HU_ASSERT_EQ(err, HU_OK);

    /* Save token for server2 */
    hu_oauth_token_t token2;
    memset(&token2, 0, sizeof(token2));
    token2.access_token = "token_for_server2";
    token2.access_token_len = strlen(token2.access_token);
    token2.token_type = "Bearer";
    token2.token_type_len = strlen(token2.token_type);

    err = hu_mcp_oauth_token_save(&alloc, tmpfile, "server2", &token2);
    HU_ASSERT_EQ(err, HU_OK);

    /* Load token1 */
    hu_oauth_token_t loaded1;
    err = hu_mcp_oauth_token_load(&alloc, tmpfile, "server1", &loaded1);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(loaded1.access_token, "token_for_server1");

    /* Load token2 */
    hu_oauth_token_t loaded2;
    err = hu_mcp_oauth_token_load(&alloc, tmpfile, "server2", &loaded2);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(loaded2.access_token, "token_for_server2");

    hu_mcp_oauth_token_free(&alloc, &loaded1);
    hu_mcp_oauth_token_free(&alloc, &loaded2);
    unlink(tmpfile);
}

/* ── Token Exchange ────────────────────────────────────────────────────── */

static void test_token_exchange_code_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_oauth_config_t config = {
        .client_id = "test_client",
        .client_id_len = strlen("test_client"),
        .token_url = "https://example.com/oauth/token",
        .token_url_len = strlen("https://example.com/oauth/token"),
        .redirect_uri = "http://localhost:8888/callback",
        .redirect_uri_len = strlen("http://localhost:8888/callback"),
    };

    hu_oauth_pkce_t pkce;
    memset(&pkce, 0, sizeof(pkce));
    strcpy(pkce.verifier, "test_verifier_1234567890123456789012345678");

    hu_oauth_token_t token;
    hu_error_t err = hu_mcp_oauth_exchange_code(&alloc, &config, &pkce, "auth_code_123", &token);

#ifdef HU_IS_TEST
    /* In test mode, we expect success with mock response */
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(token.access_token);
    HU_ASSERT(token.access_token_len > 0);
    HU_ASSERT_NOT_NULL(token.token_type);
    HU_ASSERT_STR_EQ(token.token_type, "Bearer");
    HU_ASSERT(token.expires_at > 0);
    hu_mcp_oauth_token_free(&alloc, &token);
#else
    /* Without curl and not in test mode, expect not supported */
    (void)err;
#endif
}

/* ── Test Suite ─────────────────────────────────────────────────────────── */

void run_oauth_tests(void) {
    HU_TEST_SUITE("oauth");

    /* PKCE Verifier */
    HU_RUN_TEST(test_pkce_verifier_generation);
    HU_RUN_TEST(test_pkce_verifier_charset);
    HU_RUN_TEST(test_pkce_different_verifiers);

    /* PKCE Challenge (S256) */
    HU_RUN_TEST(test_pkce_challenge_generation);
    HU_RUN_TEST(test_pkce_challenge_deterministic);
    HU_RUN_TEST(test_pkce_challenge_different_verifiers);
    HU_RUN_TEST(test_pkce_challenge_buffer_too_small);

    /* Base64url */
    HU_RUN_TEST(test_base64url_simple);
    HU_RUN_TEST(test_base64url_no_padding);
    HU_RUN_TEST(test_base64url_charset);
    HU_RUN_TEST(test_base64url_buffer_too_small);

    /* Authorization URL */
    HU_RUN_TEST(test_oauth_build_auth_url);
    HU_RUN_TEST(test_oauth_build_auth_url_all_params);

    /* Token Expiration */
    HU_RUN_TEST(test_token_not_expired);
    HU_RUN_TEST(test_token_is_expired);
    HU_RUN_TEST(test_token_never_expires);

    /* Token Free */
    HU_RUN_TEST(test_token_free_no_leaks);
    HU_RUN_TEST(test_token_free_null_safe);
    HU_RUN_TEST(test_token_free_idempotent);

    /* Token Save/Load */
    HU_RUN_TEST(test_token_save_creates_file);
    HU_RUN_TEST(test_token_load_from_saved_file);
    HU_RUN_TEST(test_token_save_overwrites_existing);
    HU_RUN_TEST(test_token_load_not_found);
    HU_RUN_TEST(test_token_load_wrong_server_name);
    HU_RUN_TEST(test_token_save_multiple_servers);

    /* Token Exchange */
    HU_RUN_TEST(test_token_exchange_code_mock);
}
