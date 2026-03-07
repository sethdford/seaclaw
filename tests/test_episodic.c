/* Episodic memory tests — session summarization, store/load */
#include "seaclaw/agent/episodic.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/memory.h"
#include "seaclaw/provider.h"
#include "test_framework.h"
#include <string.h>

static void test_episodic_summarize_null(void) {
    SC_ASSERT_NULL(sc_episodic_summarize_session(NULL, NULL, NULL, 0, NULL));

    sc_allocator_t alloc = sc_system_allocator();
    SC_ASSERT_NULL(sc_episodic_summarize_session(&alloc, NULL, NULL, 0, NULL));
}

static void test_episodic_summarize_basic(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *msgs[] = {"How do I compile seaclaw?", "Run cmake --build build"};
    size_t lens[] = {25, 24};

    size_t out_len = 0;
    char *summary = sc_episodic_summarize_session(&alloc, msgs, lens, 2, &out_len);
    SC_ASSERT_NOT_NULL(summary);
    SC_ASSERT_TRUE(out_len > 0);
    SC_ASSERT_TRUE(strstr(summary, "Session topic:") != NULL);
    SC_ASSERT_TRUE(strstr(summary, "compile seaclaw") != NULL);
    alloc.free(alloc.ctx, summary, out_len + 1);
}

static void test_episodic_summarize_long_truncation(void) {
    sc_allocator_t alloc = sc_system_allocator();
    char long_msg[1024];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';

    const char *msgs[] = {long_msg};
    size_t lens[] = {sizeof(long_msg) - 1};

    size_t out_len = 0;
    char *summary = sc_episodic_summarize_session(&alloc, msgs, lens, 1, &out_len);
    SC_ASSERT_NOT_NULL(summary);
    SC_ASSERT_TRUE(out_len <= SC_EPISODIC_MAX_SUMMARY);
    alloc.free(alloc.ctx, summary, out_len + 1);
}

static void test_episodic_summarize_skips_empty_first(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *msgs[] = {"", "reply", "real question", "real answer"};
    size_t lens[] = {0, 5, 13, 11};

    size_t out_len = 0;
    char *summary = sc_episodic_summarize_session(&alloc, msgs, lens, 4, &out_len);
    SC_ASSERT_NOT_NULL(summary);
    SC_ASSERT_TRUE(strstr(summary, "real question") != NULL);
    alloc.free(alloc.ctx, summary, out_len + 1);
}

static void test_episodic_store_null_args(void) {
    sc_allocator_t alloc = sc_system_allocator();
    SC_ASSERT_EQ(sc_episodic_store(NULL, &alloc, "s1", 2, "sum", 3), SC_ERR_INVALID_ARGUMENT);
}

static void test_episodic_load_null_args(void) {
    SC_ASSERT_EQ(sc_episodic_load(NULL, NULL, NULL, NULL), SC_ERR_INVALID_ARGUMENT);

    sc_allocator_t alloc = sc_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_episodic_load(NULL, &alloc, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NULL(out);
}

static void test_episodic_store_with_session_id(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_none_memory_create(&alloc);

    sc_error_t err = sc_episodic_store(&mem, &alloc, "sess_abc", 8, "topic: test", 11);
    SC_ASSERT_EQ(err, SC_OK);
    if (mem.vtable && mem.vtable->deinit)
        mem.vtable->deinit(mem.ctx);
}

static void test_episodic_store_without_session_id(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_t mem = sc_none_memory_create(&alloc);

    sc_error_t err = sc_episodic_store(&mem, &alloc, NULL, 0, "global topic", 12);
    SC_ASSERT_EQ(err, SC_OK);
    if (mem.vtable && mem.vtable->deinit)
        mem.vtable->deinit(mem.ctx);
}

static void test_episodic_key_prefix(void) {
    SC_ASSERT_STR_EQ(SC_EPISODIC_KEY_PREFIX, "_ep:");
    SC_ASSERT_EQ(SC_EPISODIC_KEY_PREFIX_LEN, 4);
}

static void test_episodic_summarize_llm_null_args(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t prov = {0};
    const char *msgs[] = {"How do I build?", "Run cmake --build build"};
    size_t lens[] = {16, 24};

    SC_ASSERT_NULL(sc_episodic_summarize_session_llm(NULL, &prov, msgs, lens, 2, NULL));
    SC_ASSERT_NULL(sc_episodic_summarize_session_llm(&alloc, &prov, NULL, lens, 2, NULL));
}

static void test_episodic_summarize_llm_basic(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t prov = {0};
    const char *msgs[] = {"How do I compile seaclaw?", "Run cmake --build build"};
    size_t lens[] = {25, 24};

    size_t out_len = 0;
    char *summary = sc_episodic_summarize_session_llm(&alloc, &prov, msgs, lens, 2, &out_len);
    SC_ASSERT_NOT_NULL(summary);
    SC_ASSERT_TRUE(out_len > 0);
    SC_ASSERT_TRUE(strstr(summary, "LLM summary:") != NULL);
    SC_ASSERT_TRUE(strstr(summary, "compile seaclaw") != NULL);
    alloc.free(alloc.ctx, summary, out_len + 1);
}

static void test_episodic_summarize_llm_fallback(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *msgs[] = {"What is the build command?", "Use cmake --build build"};
    size_t lens[] = {25, 22};

    size_t out_len = 0;
    char *summary = sc_episodic_summarize_session_llm(&alloc, NULL, msgs, lens, 2, &out_len);
    SC_ASSERT_NOT_NULL(summary);
    SC_ASSERT_TRUE(out_len > 0);
    SC_ASSERT_TRUE(strstr(summary, "Session topic:") != NULL);
    SC_ASSERT_TRUE(strstr(summary, "build command") != NULL);
    alloc.free(alloc.ctx, summary, out_len + 1);
}

void run_episodic_tests(void) {
    SC_TEST_SUITE("Episodic");
    SC_RUN_TEST(test_episodic_summarize_null);
    SC_RUN_TEST(test_episodic_summarize_basic);
    SC_RUN_TEST(test_episodic_summarize_long_truncation);
    SC_RUN_TEST(test_episodic_summarize_skips_empty_first);
    SC_RUN_TEST(test_episodic_summarize_llm_null_args);
    SC_RUN_TEST(test_episodic_summarize_llm_basic);
    SC_RUN_TEST(test_episodic_summarize_llm_fallback);
    SC_RUN_TEST(test_episodic_store_null_args);
    SC_RUN_TEST(test_episodic_load_null_args);
    SC_RUN_TEST(test_episodic_store_with_session_id);
    SC_RUN_TEST(test_episodic_store_without_session_id);
    SC_RUN_TEST(test_episodic_key_prefix);
}
