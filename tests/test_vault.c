/*
 * Vault tests — SC_IS_TEST uses in-memory storage.
 */
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/security/vault.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

static void test_vault_set_and_get(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    sc_error_t err = sc_vault_set(v, "my_secret", "secret_value_123");
    SC_ASSERT_EQ(err, SC_OK);

    char out[256];
    err = sc_vault_get(v, "my_secret", out, sizeof(out));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_STR_EQ(out, "secret_value_123");

    sc_vault_destroy(v);
}

static void test_vault_get_nonexistent_returns_not_found(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    char out[256];
    sc_error_t err = sc_vault_get(v, "nonexistent_key", out, sizeof(out));
    SC_ASSERT_EQ(err, SC_ERR_NOT_FOUND);

    sc_vault_destroy(v);
}

static void test_vault_delete_removes_key(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    sc_vault_set(v, "to_delete", "value");
    sc_error_t err = sc_vault_delete(v, "to_delete");
    SC_ASSERT_EQ(err, SC_OK);

    char out[256];
    err = sc_vault_get(v, "to_delete", out, sizeof(out));
    SC_ASSERT_EQ(err, SC_ERR_NOT_FOUND);

    sc_vault_destroy(v);
}

static void test_vault_delete_nonexistent_returns_not_found(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    sc_error_t err = sc_vault_delete(v, "nonexistent");
    SC_ASSERT_EQ(err, SC_ERR_NOT_FOUND);

    sc_vault_destroy(v);
}

static void test_vault_list_keys_returns_correct_count(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    sc_vault_set(v, "a", "1");
    sc_vault_set(v, "b", "2");
    sc_vault_set(v, "c", "3");

    char *keys[16];
    size_t count = 0;
    sc_error_t err = sc_vault_list_keys(v, keys, 16, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 3u);

    for (size_t i = 0; i < count; i++)
        alloc.free(alloc.ctx, keys[i], strlen(keys[i]) + 1);

    sc_vault_destroy(v);
}

static void test_vault_no_key_still_works(void) {
    /* With SEACLAW_VAULT_KEY unset, vault uses base64 obfuscation. */
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    sc_error_t err = sc_vault_set(v, "obfuscated", "hello");
    SC_ASSERT_EQ(err, SC_OK);

    char out[256];
    err = sc_vault_get(v, "obfuscated", out, sizeof(out));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_STR_EQ(out, "hello");

    sc_vault_destroy(v);
}

static void test_vault_get_api_key_from_vault(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    sc_vault_set(v, "openai_api_key", "sk-vault-key");
    char *api_key = NULL;
    sc_error_t err = sc_vault_get_api_key(v, &alloc, "openai", NULL, &api_key);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(api_key);
    SC_ASSERT_STR_EQ(api_key, "sk-vault-key");
    alloc.free(alloc.ctx, api_key, strlen(api_key) + 1);

    sc_vault_destroy(v);
}

static void test_vault_get_api_key_fallback_config(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    char *api_key = NULL;
    sc_error_t err = sc_vault_get_api_key(v, &alloc, "openai", "sk-config-key", &api_key);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_STR_EQ(api_key, "sk-config-key");
    alloc.free(alloc.ctx, api_key, strlen(api_key) + 1);

    sc_vault_destroy(v);
}

static void test_vault_get_api_key_not_found(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_vault_t *v = sc_vault_create(&alloc, NULL);
    SC_ASSERT_NOT_NULL(v);

    char *api_key = NULL;
    sc_error_t err = sc_vault_get_api_key(v, &alloc, "openai", NULL, &api_key);
    SC_ASSERT_EQ(err, SC_ERR_NOT_FOUND);
    SC_ASSERT_NULL(api_key);

    sc_vault_destroy(v);
}

void run_vault_tests(void) {
    SC_TEST_SUITE("vault");
    SC_RUN_TEST(test_vault_set_and_get);
    SC_RUN_TEST(test_vault_get_nonexistent_returns_not_found);
    SC_RUN_TEST(test_vault_delete_removes_key);
    SC_RUN_TEST(test_vault_delete_nonexistent_returns_not_found);
    SC_RUN_TEST(test_vault_list_keys_returns_correct_count);
    SC_RUN_TEST(test_vault_no_key_still_works);
    SC_RUN_TEST(test_vault_get_api_key_from_vault);
    SC_RUN_TEST(test_vault_get_api_key_fallback_config);
    SC_RUN_TEST(test_vault_get_api_key_not_found);
}
