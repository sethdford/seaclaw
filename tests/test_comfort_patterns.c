#ifdef HU_ENABLE_SQLITE

#include "human/context/conversation.h"
#include "human/core/allocator.h"
#include "human/memory.h"
#include "human/memory/comfort_patterns.h"
#include "test_framework.h"
#include <string.h>

static void comfort_pattern_record_three_distraction_returns_distraction(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_error_t err;
    err = hu_comfort_pattern_record(&mem, "contact_a", 9, "sad", 3, "distraction", 11, 0.8f);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_comfort_pattern_record(&mem, "contact_a", 9, "sad", 3, "distraction", 11, 0.7f);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_comfort_pattern_record(&mem, "contact_a", 9, "sad", 3, "distraction", 11, 0.9f);
    HU_ASSERT_EQ(err, HU_OK);

    char out_type[32];
    size_t out_len = 0;
    err = hu_comfort_pattern_get_preferred(&alloc, &mem, "contact_a", 9, "sad", 3, out_type,
                                           sizeof(out_type), &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(out_len, 11u);
    HU_ASSERT_STR_EQ(out_type, "distraction");

    mem.vtable->deinit(mem.ctx);
}

static void comfort_pattern_no_data_returns_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    char out_type[32];
    size_t out_len = 1;
    hu_error_t err = hu_comfort_pattern_get_preferred(&alloc, &mem, "contact_b", 9, "anxious", 7,
                                                      out_type, sizeof(out_type), &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(out_len, 0u);

    mem.vtable->deinit(mem.ctx);
}

static void comfort_pattern_different_emotions_separate_patterns(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    HU_ASSERT_NOT_NULL(mem.ctx);

    hu_comfort_pattern_record(&mem, "contact_c", 9, "sad", 3, "empathy", 7, 0.8f);
    hu_comfort_pattern_record(&mem, "contact_c", 9, "sad", 3, "empathy", 7, 0.7f);
    hu_comfort_pattern_record(&mem, "contact_c", 9, "anxious", 7, "distraction", 11, 0.9f);
    hu_comfort_pattern_record(&mem, "contact_c", 9, "anxious", 7, "distraction", 11, 0.85f);

    char out_type[32];
    size_t out_len = 0;
    hu_comfort_pattern_get_preferred(&alloc, &mem, "contact_c", 9, "sad", 3, out_type,
                                     sizeof(out_type), &out_len);
    HU_ASSERT_EQ(out_len, 7u);
    HU_ASSERT_STR_EQ(out_type, "empathy");

    out_len = 0;
    hu_comfort_pattern_get_preferred(&alloc, &mem, "contact_c", 9, "anxious", 7, out_type,
                                     sizeof(out_type), &out_len);
    HU_ASSERT_EQ(out_len, 11u);
    HU_ASSERT_STR_EQ(out_type, "distraction");

    mem.vtable->deinit(mem.ctx);
}

static void comfort_directive_produces_expected_output(void) {
    char buf[256];
    size_t len =
        hu_conversation_build_comfort_directive("distraction", 11, "sad", 3, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_EQ(buf, "[COMFORT: This contact responds well to distraction when sad.]");
}

static void comfort_directive_invalid_args_returns_zero(void) {
    char buf[256];
    size_t len = hu_conversation_build_comfort_directive(NULL, 11, "sad", 3, buf, sizeof(buf));
    HU_ASSERT_EQ(len, 0u);

    len = hu_conversation_build_comfort_directive("empathy", 7, NULL, 3, buf, sizeof(buf));
    HU_ASSERT_EQ(len, 0u);

    len = hu_conversation_build_comfort_directive("empathy", 7, "sad", 3, NULL, 256);
    HU_ASSERT_EQ(len, 0u);
}

static void comfort_pattern_none_memory_returns_not_supported(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_none_memory_create(&alloc);

    hu_error_t err = hu_comfort_pattern_record(&mem, "x", 1, "y", 1, "z", 1, 0.5f);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);

    char out_type[32];
    size_t out_len = 0;
    err = hu_comfort_pattern_get_preferred(&alloc, &mem, "x", 1, "y", 1, out_type, sizeof(out_type),
                                           &out_len);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);

    mem.vtable->deinit(mem.ctx);
}

void run_comfort_patterns_tests(void) {
    HU_TEST_SUITE("comfort_patterns");
    HU_RUN_TEST(comfort_pattern_record_three_distraction_returns_distraction);
    HU_RUN_TEST(comfort_pattern_no_data_returns_empty);
    HU_RUN_TEST(comfort_pattern_different_emotions_separate_patterns);
    HU_RUN_TEST(comfort_directive_produces_expected_output);
    HU_RUN_TEST(comfort_directive_invalid_args_returns_zero);
    HU_RUN_TEST(comfort_pattern_none_memory_returns_not_supported);
}

#else

void run_comfort_patterns_tests(void) {
    (void)0;
}

#endif /* HU_ENABLE_SQLITE */
