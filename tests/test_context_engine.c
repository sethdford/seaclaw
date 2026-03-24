#include "human/context_engine.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>
#include <time.h>

static void legacy_create_and_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_context_engine_t engine;
    hu_error_t err = hu_context_engine_legacy_create(&alloc, &engine);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(engine.ctx);
    HU_ASSERT_NOT_NULL(engine.vtable);
    HU_ASSERT_STR_EQ(engine.vtable->get_name(engine.ctx), "legacy");
    engine.vtable->deinit(engine.ctx, &alloc);
}

static void legacy_bootstrap_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_context_engine_t engine;
    hu_error_t err = hu_context_engine_legacy_create(&alloc, &engine);
    HU_ASSERT_EQ(err, HU_OK);
    err = engine.vtable->bootstrap(engine.ctx, &alloc);
    HU_ASSERT_EQ(err, HU_OK);
    engine.vtable->deinit(engine.ctx, &alloc);
}

static void legacy_ingest_stores_messages(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_context_engine_t engine;
    hu_context_engine_legacy_create(&alloc, &engine);

    hu_context_message_t msg = {
        .role = "user",
        .role_len = 4,
        .content = "hello world",
        .content_len = 11,
        .timestamp = (int64_t)time(NULL),
    };
    hu_error_t err = engine.vtable->ingest(engine.ctx, &alloc, &msg);
    HU_ASSERT_EQ(err, HU_OK);

    hu_context_message_t msg2 = {
        .role = "assistant",
        .role_len = 9,
        .content = "hi there",
        .content_len = 8,
        .timestamp = (int64_t)time(NULL),
    };
    err = engine.vtable->ingest(engine.ctx, &alloc, &msg2);
    HU_ASSERT_EQ(err, HU_OK);

    engine.vtable->deinit(engine.ctx, &alloc);
}

static void legacy_assemble_respects_budget(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_context_engine_t engine;
    hu_context_engine_legacy_create(&alloc, &engine);

    for (int i = 0; i < 10; i++) {
        hu_context_message_t msg = {
            .role = "user",
            .role_len = 4,
            .content = "test message for budget checking",
            .content_len = 32,
            .timestamp = (int64_t)time(NULL) + i,
        };
        engine.vtable->ingest(engine.ctx, &alloc, &msg);
    }

    hu_context_budget_t budget = {
        .max_tokens = 50,
        .reserved_tokens = 10,
        .used_tokens = 0,
    };
    hu_assembled_context_t assembled;
    hu_error_t err = engine.vtable->assemble(engine.ctx, &alloc, &budget, &assembled);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(assembled.total_tokens <= 40);
    hu_assembled_context_free(&alloc, &assembled);

    engine.vtable->deinit(engine.ctx, &alloc);
}

static void legacy_compact_reduces_history(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_context_engine_t engine;
    hu_context_engine_legacy_create(&alloc, &engine);

    for (int i = 0; i < 20; i++) {
        hu_context_message_t msg = {
            .role = "user",
            .role_len = 4,
            .content = "a message that takes space in the context window",
            .content_len = 49,
            .timestamp = (int64_t)time(NULL) + i,
        };
        engine.vtable->ingest(engine.ctx, &alloc, &msg);
    }

    hu_error_t err = engine.vtable->compact(engine.ctx, &alloc, 30);
    HU_ASSERT_EQ(err, HU_OK);

    hu_context_budget_t budget = {.max_tokens = 10000, .reserved_tokens = 0};
    hu_assembled_context_t assembled;
    err = engine.vtable->assemble(engine.ctx, &alloc, &budget, &assembled);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(assembled.messages_count < 20);
    hu_assembled_context_free(&alloc, &assembled);

    engine.vtable->deinit(engine.ctx, &alloc);
}

static void legacy_after_turn_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_context_engine_t engine;
    hu_context_engine_legacy_create(&alloc, &engine);

    hu_context_message_t user = {
        .role = "user",
        .role_len = 4,
        .content = "hi",
        .content_len = 2,
        .timestamp = (int64_t)time(NULL),
    };
    hu_context_message_t assistant = {
        .role = "assistant",
        .role_len = 9,
        .content = "hello",
        .content_len = 5,
        .timestamp = (int64_t)time(NULL),
    };
    hu_error_t err = engine.vtable->after_turn(engine.ctx, &alloc, &user, &assistant);
    HU_ASSERT_EQ(err, HU_OK);

    engine.vtable->deinit(engine.ctx, &alloc);
}

static void legacy_subagent_returns_not_supported(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_context_engine_t engine;
    hu_context_engine_legacy_create(&alloc, &engine);

    hu_error_t err = engine.vtable->prepare_subagent(engine.ctx, &alloc, NULL);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);

    err = engine.vtable->merge_subagent(engine.ctx, &alloc, NULL);
    HU_ASSERT_EQ(err, HU_ERR_NOT_SUPPORTED);

    engine.vtable->deinit(engine.ctx, &alloc);
}

static void create_null_args_returns_error(void) {
    hu_error_t err = hu_context_engine_legacy_create(NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

void run_context_engine_tests(void) {
    HU_TEST_SUITE("context_engine");
    HU_RUN_TEST(legacy_create_and_destroy);
    HU_RUN_TEST(legacy_bootstrap_succeeds);
    HU_RUN_TEST(legacy_ingest_stores_messages);
    HU_RUN_TEST(legacy_assemble_respects_budget);
    HU_RUN_TEST(legacy_compact_reduces_history);
    HU_RUN_TEST(legacy_after_turn_succeeds);
    HU_RUN_TEST(legacy_subagent_returns_not_supported);
    HU_RUN_TEST(create_null_args_returns_error);
}
