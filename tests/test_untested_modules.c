/*
 * Tests for previously untested or lightly tested modules:
 * voice_channel, skill_registry, crontab, update, memory/engines/api.
 * Uses SC_IS_TEST paths (no real network/IO).
 */
#include "test_framework.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/string.h"
#include <string.h>

#if defined(SC_HAS_SONATA)
#include "seaclaw/channels/voice_channel.h"
#include "seaclaw/channel.h"
#endif

#include "seaclaw/skill_registry.h"
#include "seaclaw/crontab.h"
#include "seaclaw/update.h"
#include "seaclaw/memory/engines.h"
#include "seaclaw/memory.h"

/* ─── voice_channel (only when built with SC_HAS_SONATA) ──────────────────── */
#if defined(SC_HAS_SONATA)
static void test_voice_channel_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_t ch = {0};
    sc_error_t err = sc_channel_voice_create(&alloc, NULL, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ch.ctx);
    SC_ASSERT_NOT_NULL(ch.vtable);
    SC_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "voice");
    sc_channel_voice_destroy(&ch);
}

static void test_voice_channel_create_null_alloc_fails(void) {
    sc_channel_t ch = {0};
    sc_error_t err = sc_channel_voice_create(NULL, NULL, &ch);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_voice_channel_create_null_out_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_channel_voice_create(&alloc, NULL, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_voice_channel_create_with_config_applies_defaults(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_channel_voice_config_t config = {0};
    config.speaker_id = "test-speaker";
    sc_channel_t ch = {0};
    sc_error_t err = sc_channel_voice_create(&alloc, &config, &ch);
    SC_ASSERT_EQ(err, SC_OK);
    sc_channel_voice_destroy(&ch);
}
#endif

/* ─── skill_registry ──────────────────────────────────────────────────────── */
static void test_skill_registry_search_null_alloc_fails(void) {
    sc_skill_registry_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = sc_skill_registry_search(NULL, "query", &entries, &count);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_skill_registry_search_null_out_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_error_t err = sc_skill_registry_search(&alloc, "query", NULL, NULL);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_skill_registry_search_mock_returns_valid_entries(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skill_registry_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = sc_skill_registry_search(&alloc, "email", &entries, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(entries);
    SC_ASSERT_TRUE(count >= 1);
    sc_skill_registry_entries_free(&alloc, entries, count);
}

/* ─── crontab ────────────────────────────────────────────────────────────── */
static void test_crontab_get_path_null_alloc_fails(void) {
    char *path = NULL;
    size_t path_len = 0;
    sc_error_t err = sc_crontab_get_path(NULL, &path, &path_len);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_crontab_load_nonexistent_returns_empty(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_crontab_entry_t *entries = NULL;
    size_t count = 99;
    sc_error_t err = sc_crontab_load(&alloc, "/nonexistent/crontab.json", &entries, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);
}

static void test_crontab_entries_free_null_safe(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_crontab_entries_free(&alloc, NULL, 0);
    sc_crontab_entries_free(&alloc, NULL, 5);
}

/* ─── update ─────────────────────────────────────────────────────────────── */
static void test_update_check_null_buf_fails(void) {
    sc_error_t err = sc_update_check(NULL, 64);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_update_check_zero_size_fails(void) {
    char buf[64];
    sc_error_t err = sc_update_check(buf, 0);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_update_check_mock_returns_version(void) {
    char buf[64];
    sc_error_t err = sc_update_check(buf, sizeof(buf));
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(strlen(buf) > 0);
}

/* ─── memory/engines/api ────────────────────────────────────────────────── */
#if defined(SC_IS_TEST) && SC_IS_TEST
static void test_api_memory_create_null_alloc_returns_null(void) {
    sc_memory_t mem = sc_api_memory_create(NULL, "https://api.example.com", NULL, 5000);
    SC_ASSERT_NULL(mem.ctx);
    SC_ASSERT_NULL(mem.vtable);
}

static void test_api_memory_create_null_base_url_returns_null(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_api_memory_create(&alloc, NULL, NULL, 5000);
    SC_ASSERT_NULL(mem.ctx);
    SC_ASSERT_NULL(mem.vtable);
}

static void test_api_memory_store_recall_mock(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_api_memory_create(&alloc, "https://api.example.com", "key", 5000);
    SC_ASSERT_NOT_NULL(mem.vtable);

    sc_memory_category_t cat = { .tag = SC_MEMORY_CATEGORY_CORE };
    sc_error_t err = mem.vtable->store(mem.ctx, "k1", 2, "content", 7, &cat, NULL, 0);
    SC_ASSERT_EQ(err, SC_OK);

    sc_memory_entry_t *out = NULL;
    size_t count = 0;
    err = mem.vtable->recall(mem.ctx, &alloc, "con", 3, 10, NULL, 0, &out, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(count >= 1);
    if (out) {
        for (size_t i = 0; i < count; i++)
            sc_memory_entry_free_fields(&alloc, &out[i]);
        alloc.free(alloc.ctx, out, count * sizeof(sc_memory_entry_t));
    }
    mem.vtable->deinit(mem.ctx);
}
#endif

void run_untested_modules_tests(void) {
    SC_TEST_SUITE("Untested modules");

#if defined(SC_HAS_SONATA)
    SC_RUN_TEST(test_voice_channel_create_destroy);
    SC_RUN_TEST(test_voice_channel_create_null_alloc_fails);
    SC_RUN_TEST(test_voice_channel_create_null_out_fails);
    SC_RUN_TEST(test_voice_channel_create_with_config_applies_defaults);
#endif

    SC_RUN_TEST(test_skill_registry_search_null_alloc_fails);
    SC_RUN_TEST(test_skill_registry_search_null_out_fails);
    SC_RUN_TEST(test_skill_registry_search_mock_returns_valid_entries);

    SC_RUN_TEST(test_crontab_get_path_null_alloc_fails);
    SC_RUN_TEST(test_crontab_load_nonexistent_returns_empty);
    SC_RUN_TEST(test_crontab_entries_free_null_safe);

    SC_RUN_TEST(test_update_check_null_buf_fails);
    SC_RUN_TEST(test_update_check_zero_size_fails);
    SC_RUN_TEST(test_update_check_mock_returns_version);

#if defined(SC_IS_TEST) && SC_IS_TEST
    SC_RUN_TEST(test_api_memory_create_null_alloc_returns_null);
    SC_RUN_TEST(test_api_memory_create_null_base_url_returns_null);
    SC_RUN_TEST(test_api_memory_store_recall_mock);
#endif
}
