#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/providers/api_key.h"
#include "test_framework.h"

static void test_api_key_valid_non_null_key_returns_true(void) {
    HU_ASSERT_TRUE(hu_api_key_valid("sk-test-key", 11));
}

static void test_api_key_valid_empty_string_returns_false(void) {
    HU_ASSERT_FALSE(hu_api_key_valid("", 0));
}

static void test_api_key_valid_whitespace_only_returns_false(void) {
    HU_ASSERT_FALSE(hu_api_key_valid("   ", 3));
    HU_ASSERT_FALSE(hu_api_key_valid("\t\n ", 3));
}

static void test_api_key_valid_trimmed_content_returns_true(void) {
    HU_ASSERT_TRUE(hu_api_key_valid("  sk-key  ", 10));
}

static void test_api_key_valid_null_key_returns_false(void) {
    HU_ASSERT_FALSE(hu_api_key_valid(NULL, 0));
}

static void test_api_key_mask_null_returns_no_key(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *m = hu_api_key_mask(&alloc, NULL, 0);
    HU_ASSERT_NOT_NULL(m);
    HU_ASSERT_STR_EQ(m, "[no key]");
    hu_str_free(&alloc, m);
}

static void test_api_key_mask_empty_returns_no_key(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *m = hu_api_key_mask(&alloc, "", 0);
    HU_ASSERT_NOT_NULL(m);
    HU_ASSERT_STR_EQ(m, "[no key]");
    hu_str_free(&alloc, m);
}

static void test_api_key_mask_short_key_returns_stars(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *m = hu_api_key_mask(&alloc, "ab", 2);
    HU_ASSERT_NOT_NULL(m);
    HU_ASSERT_STR_EQ(m, "****");
    hu_str_free(&alloc, m);
}

static void test_api_key_mask_four_char_key_returns_stars(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *m = hu_api_key_mask(&alloc, "abcd", 4);
    HU_ASSERT_NOT_NULL(m);
    HU_ASSERT_STR_EQ(m, "****");
    hu_str_free(&alloc, m);
}

static void test_api_key_mask_long_key_returns_first_four_plus_ellipsis(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *m = hu_api_key_mask(&alloc, "sk-abcdefgh", 11);
    HU_ASSERT_NOT_NULL(m);
    HU_ASSERT_STR_EQ(m, "sk-a...");
    hu_str_free(&alloc, m);
}

static void test_api_key_resolve_explicit_key_returns_trimmed(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *k = hu_api_key_resolve(&alloc, "openai", 6, "  sk-test  ", 11);
    HU_ASSERT_NOT_NULL(k);
    HU_ASSERT_STR_EQ(k, "sk-test");
    hu_str_free(&alloc, k);
}

static void test_api_key_resolve_explicit_key_no_trim_needed(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *k = hu_api_key_resolve(&alloc, "openai", 6, "sk-test", 7);
    HU_ASSERT_NOT_NULL(k);
    HU_ASSERT_STR_EQ(k, "sk-test");
    hu_str_free(&alloc, k);
}

static void test_api_key_resolve_null_key_no_env_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *k = hu_api_key_resolve(&alloc, "unknown-provider-xyz", 19, NULL, 0);
    HU_ASSERT_NULL(k);
}

static void test_api_key_resolve_empty_key_no_env_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *k = hu_api_key_resolve(&alloc, "openai", 6, "", 0);
    HU_ASSERT_NULL(k);
}

static void test_api_key_resolve_whitespace_only_key_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *k = hu_api_key_resolve(&alloc, "openai", 6, "   ", 3);
    HU_ASSERT_NULL(k);
}

static void test_api_key_resolve_zero_len_returns_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *k = hu_api_key_resolve(&alloc, "openai", 6, "sk-test", 0);
    HU_ASSERT_NULL(k);
}

void run_api_key_tests(void) {
    HU_TEST_SUITE("ApiKey");
    HU_RUN_TEST(test_api_key_valid_non_null_key_returns_true);
    HU_RUN_TEST(test_api_key_valid_empty_string_returns_false);
    HU_RUN_TEST(test_api_key_valid_whitespace_only_returns_false);
    HU_RUN_TEST(test_api_key_valid_trimmed_content_returns_true);
    HU_RUN_TEST(test_api_key_valid_null_key_returns_false);
    HU_RUN_TEST(test_api_key_mask_null_returns_no_key);
    HU_RUN_TEST(test_api_key_mask_empty_returns_no_key);
    HU_RUN_TEST(test_api_key_mask_short_key_returns_stars);
    HU_RUN_TEST(test_api_key_mask_four_char_key_returns_stars);
    HU_RUN_TEST(test_api_key_mask_long_key_returns_first_four_plus_ellipsis);
    HU_RUN_TEST(test_api_key_resolve_explicit_key_returns_trimmed);
    HU_RUN_TEST(test_api_key_resolve_explicit_key_no_trim_needed);
    HU_RUN_TEST(test_api_key_resolve_null_key_no_env_returns_null);
    HU_RUN_TEST(test_api_key_resolve_empty_key_no_env_returns_null);
    HU_RUN_TEST(test_api_key_resolve_whitespace_only_key_returns_null);
    HU_RUN_TEST(test_api_key_resolve_zero_len_returns_null);
}
