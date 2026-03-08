#include "seaclaw/core/allocator.h"
#include "seaclaw/memory/stm.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

static void stm_init_sets_session_id(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_error_t err = sc_stm_init(&buf, alloc, "sess-123", 8);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(buf.session_id);
    SC_ASSERT_EQ(buf.session_id_len, 8);
    SC_ASSERT_EQ(memcmp(buf.session_id, "sess-123", 8), 0);
    sc_stm_deinit(&buf);
}

static void stm_record_turn_stores_content(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s1", 2);

    sc_error_t err = sc_stm_record_turn(&buf, "user", 4, "Hello there", 10, 1000);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(sc_stm_count(&buf), 1);

    const sc_stm_turn_t *t = sc_stm_get(&buf, 0);
    SC_ASSERT_NOT_NULL(t);
    SC_ASSERT_STR_EQ(t->role, "user");
    SC_ASSERT_EQ(t->content_len, 10);
    SC_ASSERT_EQ(memcmp(t->content, "Hello there", 10), 0);
    SC_ASSERT_EQ(t->timestamp_ms, 1000);

    sc_stm_deinit(&buf);
}

static void stm_wraps_at_max_turns(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s2", 2);

    for (size_t i = 0; i < SC_STM_MAX_TURNS + 5; i++) {
        char content[32];
        int n = snprintf(content, sizeof(content), "turn-%zu", i);
        sc_stm_record_turn(&buf, "user", 4, content, (size_t)n, (uint64_t)i);
    }

    SC_ASSERT_EQ(sc_stm_count(&buf), SC_STM_MAX_TURNS);
    /* Oldest should be turn-5 (the 21st we added, 0-indexed = 20, but we keep last 20 so oldest is
     * turn 5) */
    const sc_stm_turn_t *oldest = sc_stm_get(&buf, 0);
    SC_ASSERT_NOT_NULL(oldest);
    SC_ASSERT_STR_EQ(oldest->content, "turn-5");

    sc_stm_deinit(&buf);
}

static void stm_build_context_formats_turns(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s3", 2);

    sc_stm_record_turn(&buf, "user", 4, "Hi", 2, 1000);
    sc_stm_record_turn(&buf, "assistant", 9, "Hello!", 6, 1001);

    char *ctx = NULL;
    size_t ctx_len = 0;
    sc_error_t err = sc_stm_build_context(&buf, &alloc, &ctx, &ctx_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_TRUE(strstr(ctx, "## Session Context") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "**user**: Hi") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "**assistant**: Hello!") != NULL);

    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    sc_stm_deinit(&buf);
}

static void stm_clear_resets_buffer(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s4", 2);

    sc_stm_record_turn(&buf, "user", 4, "test", 4, 1000);
    SC_ASSERT_EQ(sc_stm_count(&buf), 1);

    sc_stm_clear(&buf);
    SC_ASSERT_EQ(sc_stm_count(&buf), 0);
    SC_ASSERT_NULL(sc_stm_get(&buf, 0));

    sc_stm_deinit(&buf);
}

static void stm_get_returns_null_for_out_of_range(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s5", 2);

    sc_stm_record_turn(&buf, "user", 4, "one", 3, 1000);
    SC_ASSERT_NULL(sc_stm_get(&buf, 1));
    SC_ASSERT_NULL(sc_stm_get(&buf, 99));

    sc_stm_deinit(&buf);
}

static void stm_context_includes_emotions(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s6", 2);

    sc_stm_record_turn(&buf, "user", 4, "Hello", 5, 1000);
    sc_error_t err = sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_JOY, 0.8);
    SC_ASSERT_EQ(err, SC_OK);

    char *ctx = NULL;
    size_t ctx_len = 0;
    err = sc_stm_build_context(&buf, &alloc, &ctx, &ctx_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_NOT_NULL(strstr(ctx, "joy"));
    SC_ASSERT_NOT_NULL(strstr(ctx, "high"));

    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    sc_stm_deinit(&buf);
}

static void stm_context_no_emotions_skips_section(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s7", 2);

    sc_stm_record_turn(&buf, "user", 4, "Hello", 5, 1000);

    char *ctx = NULL;
    size_t ctx_len = 0;
    sc_error_t err = sc_stm_build_context(&buf, &alloc, &ctx, &ctx_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_NULL(strstr(ctx, "Emotional"));

    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    sc_stm_deinit(&buf);
}

static void stm_context_multiple_emotions(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s8", 2);

    sc_stm_record_turn(&buf, "user", 4, "Hi", 2, 1000);
    sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_JOY, 0.8);
    sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_ANXIETY, 0.5);

    char *ctx = NULL;
    size_t ctx_len = 0;
    sc_error_t err = sc_stm_build_context(&buf, &alloc, &ctx, &ctx_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_NOT_NULL(strstr(ctx, "joy"));
    SC_ASSERT_NOT_NULL(strstr(ctx, "anxiety"));

    alloc.free(alloc.ctx, ctx, ctx_len + 1);
    sc_stm_deinit(&buf);
}

static void stm_init_null_session_id_ok(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_error_t err = sc_stm_init(&buf, alloc, NULL, 0);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NULL(buf.session_id);
    SC_ASSERT_EQ(buf.session_id_len, 0u);
    sc_stm_deinit(&buf);
}

static void stm_record_turn_null_content_fails(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s", 1);

    sc_error_t err = sc_stm_record_turn(&buf, "user", 4, NULL, 0, 1000);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_EQ(sc_stm_count(&buf), 0u);

    sc_stm_deinit(&buf);
}

static void stm_turn_add_entity_at_max(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s", 1);
    sc_stm_record_turn(&buf, "user", 4, "Hi", 2, 1000);

    for (size_t i = 0; i < SC_STM_MAX_ENTITIES; i++) {
        char name[16];
        int n = snprintf(name, sizeof(name), "ent%zu", i);
        sc_error_t err = sc_stm_turn_add_entity(&buf, 0, name, (size_t)n, "person", 6, 1);
        SC_ASSERT_EQ(err, SC_OK);
    }

    sc_error_t err = sc_stm_turn_add_entity(&buf, 0, "extra", 5, "person", 6, 1);
    SC_ASSERT_EQ(err, SC_ERR_OUT_OF_MEMORY);

    sc_stm_deinit(&buf);
}

static void stm_turn_add_emotion_deduplicates(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s", 1);
    sc_stm_record_turn(&buf, "user", 4, "Hi", 2, 1000);

    sc_error_t err1 = sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_JOY, 0.8);
    SC_ASSERT_EQ(err1, SC_OK);
    sc_error_t err2 = sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_JOY, 0.9);
    SC_ASSERT_EQ(err2, SC_OK);

    const sc_stm_turn_t *t = sc_stm_get(&buf, 0);
    SC_ASSERT_NOT_NULL(t);
    SC_ASSERT_EQ(t->emotion_count, 1u);

    sc_stm_deinit(&buf);
}

static void stm_set_primary_topic(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s", 1);
    sc_stm_record_turn(&buf, "user", 4, "Hello", 5, 1000);

    sc_error_t err = sc_stm_turn_set_primary_topic(&buf, 0, "work", 4);
    SC_ASSERT_EQ(err, SC_OK);

    const sc_stm_turn_t *t = sc_stm_get(&buf, 0);
    SC_ASSERT_NOT_NULL(t);
    SC_ASSERT_NOT_NULL(t->primary_topic);
    SC_ASSERT_EQ(strlen(t->primary_topic), 4u);
    SC_ASSERT_EQ(memcmp(t->primary_topic, "work", 4), 0);

    sc_stm_deinit(&buf);
}

static void stm_set_primary_topic_out_of_range(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s", 1);
    sc_stm_record_turn(&buf, "user", 4, "Hi", 2, 1000);

    sc_error_t err = sc_stm_turn_set_primary_topic(&buf, 99, "work", 4);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);

    sc_stm_deinit(&buf);
}

static void stm_emotion_overflow(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s", 1);
    sc_stm_record_turn(&buf, "user", 4, "Hi", 2, 1000);

    sc_emotion_tag_t tags[] = {SC_EMOTION_JOY, SC_EMOTION_SADNESS, SC_EMOTION_ANGER, SC_EMOTION_FEAR,
                               SC_EMOTION_SURPRISE};
    for (size_t i = 0; i < SC_STM_MAX_EMOTIONS; i++) {
        sc_error_t err = sc_stm_turn_add_emotion(&buf, 0, tags[i], 0.8);
        SC_ASSERT_EQ(err, SC_OK);
    }

    sc_error_t err = sc_stm_turn_add_emotion(&buf, 0, SC_EMOTION_FRUSTRATION, 0.8);
    SC_ASSERT_EQ(err, SC_ERR_OUT_OF_MEMORY);

    sc_stm_deinit(&buf);
}

static void stm_build_context_null_alloc(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_stm_buffer_t buf;
    sc_stm_init(&buf, alloc, "s", 1);
    sc_stm_record_turn(&buf, "user", 4, "Hi", 2, 1000);

    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_stm_build_context(&buf, NULL, &out, &out_len);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_NULL(out);

    sc_stm_deinit(&buf);
}

void run_stm_tests(void) {
    SC_TEST_SUITE("stm");
    SC_RUN_TEST(stm_init_sets_session_id);
    SC_RUN_TEST(stm_record_turn_stores_content);
    SC_RUN_TEST(stm_wraps_at_max_turns);
    SC_RUN_TEST(stm_build_context_formats_turns);
    SC_RUN_TEST(stm_clear_resets_buffer);
    SC_RUN_TEST(stm_get_returns_null_for_out_of_range);
    SC_RUN_TEST(stm_context_includes_emotions);
    SC_RUN_TEST(stm_context_no_emotions_skips_section);
    SC_RUN_TEST(stm_context_multiple_emotions);
    SC_RUN_TEST(stm_init_null_session_id_ok);
    SC_RUN_TEST(stm_record_turn_null_content_fails);
    SC_RUN_TEST(stm_turn_add_entity_at_max);
    SC_RUN_TEST(stm_turn_add_emotion_deduplicates);
    SC_RUN_TEST(stm_set_primary_topic);
    SC_RUN_TEST(stm_set_primary_topic_out_of_range);
    SC_RUN_TEST(stm_emotion_overflow);
    SC_RUN_TEST(stm_build_context_null_alloc);
}
