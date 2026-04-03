#include "human/agent/inner_thoughts.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

/* --- Store init/deinit --- */

static void test_inner_thought_store_init_null_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_EQ(hu_inner_thought_store_init(NULL, &alloc), HU_ERR_INVALID_ARGUMENT);
}

static void test_inner_thought_store_init_success(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    HU_ASSERT_EQ(hu_inner_thought_store_init(&store, &alloc), HU_OK);
    HU_ASSERT_NOT_NULL(store.items);
    HU_ASSERT_EQ(store.count, 0);
    HU_ASSERT_GT(store.capacity, 0);
    hu_inner_thought_store_deinit(&store);
}

/* --- Accumulate --- */

static void test_inner_thought_accumulate_null_store_returns_error(void) {
    HU_ASSERT_EQ(hu_inner_thought_accumulate(NULL, "c1", 2, "t", 1, "hello", 5, 0.8, 1000),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_inner_thought_accumulate_empty_contact_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);
    HU_ASSERT_EQ(hu_inner_thought_accumulate(&store, "", 0, "t", 1, "hi", 2, 0.5, 1000),
                 HU_ERR_INVALID_ARGUMENT);
    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_accumulate_empty_text_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);
    HU_ASSERT_EQ(hu_inner_thought_accumulate(&store, "c1", 2, "t", 1, "", 0, 0.5, 1000),
                 HU_ERR_INVALID_ARGUMENT);
    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_accumulate_invalid_relevance_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);
    HU_ASSERT_EQ(hu_inner_thought_accumulate(&store, "c1", 2, "t", 1, "hi", 2, 1.5, 1000),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_inner_thought_accumulate(&store, "c1", 2, "t", 1, "hi", 2, -0.1, 1000),
                 HU_ERR_INVALID_ARGUMENT);
    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_accumulate_stores_correctly(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    HU_ASSERT_EQ(hu_inner_thought_accumulate(&store, "user_a", 6, "cooking", 7,
                                             "Ask about the recipe they mentioned", 35, 0.8, 5000),
                 HU_OK);
    HU_ASSERT_EQ(store.count, 1);
    HU_ASSERT_STR_EQ(store.items[0].contact_id, "user_a");
    HU_ASSERT_STR_EQ(store.items[0].topic, "cooking");
    HU_ASSERT_STR_EQ(store.items[0].thought_text, "Ask about the recipe they mentioned");
    HU_ASSERT_FLOAT_EQ(store.items[0].relevance_score, 0.8, 0.001);
    HU_ASSERT_EQ(store.items[0].accumulated_at, 5000);
    HU_ASSERT_FALSE(store.items[0].surfaced);

    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_accumulate_null_topic_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    HU_ASSERT_EQ(
        hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "general thought", 15, 0.7, 1000),
        HU_OK);
    HU_ASSERT_EQ(store.count, 1);
    HU_ASSERT_NULL(store.items[0].topic);

    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_accumulate_grows_capacity(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    /* Add more than initial capacity */
    for (int i = 0; i < 20; i++) {
        char text[32];
        int n = snprintf(text, sizeof(text), "thought_%d", i);
        HU_ASSERT_EQ(
            hu_inner_thought_accumulate(&store, "user_a", 6, "t", 1, text, (size_t)n, 0.5, 1000),
            HU_OK);
    }
    HU_ASSERT_EQ(store.count, 20);

    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_accumulate_evicts_oldest_at_max_capacity(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    /* Fill to capacity (1024) with ascending timestamps */
    for (int i = 0; i < 1024; i++) {
        char text[32];
        int n = snprintf(text, sizeof(text), "thought_%d", i);
        HU_ASSERT_EQ(hu_inner_thought_accumulate(&store, "u", 1, NULL, 0, text, (size_t)n, 0.5,
                                                 (uint64_t)(i + 1) * 1000),
                     HU_OK);
    }
    HU_ASSERT_EQ(store.count, 1024u);

    /* Adding one more should evict the oldest (timestamp 1000, "thought_0") */
    HU_ASSERT_EQ(
        hu_inner_thought_accumulate(&store, "u", 1, NULL, 0, "new_thought", 11, 0.5, 2000000),
        HU_OK);
    HU_ASSERT_EQ(store.count, 1024u); /* still at cap, not 1025 */

    /* Verify oldest was evicted: "thought_0" should not be found */
    bool found_old = false;
    bool found_new = false;
    for (size_t i = 0; i < store.count; i++) {
        if (store.items[i].thought_text_len == 9 &&
            memcmp(store.items[i].thought_text, "thought_0", 9) == 0)
            found_old = true;
        if (store.items[i].thought_text_len == 11 &&
            memcmp(store.items[i].thought_text, "new_thought", 11) == 0)
            found_new = true;
    }
    HU_ASSERT_FALSE(found_old);
    HU_ASSERT_TRUE(found_new);

    hu_inner_thought_store_deinit(&store);
}

/* --- Should surface --- */

static void test_inner_thought_should_surface_null_returns_false(void) {
    HU_ASSERT_FALSE(hu_inner_thought_should_surface(NULL, "topic", 5, 1000));
}

static void test_inner_thought_should_surface_already_surfaced_false(void) {
    hu_inner_thought_t t = {.contact_id = "c1",
                            .contact_id_len = 2,
                            .topic = "cooking",
                            .topic_len = 7,
                            .thought_text = "ask about recipe",
                            .thought_text_len = 16,
                            .relevance_score = 0.9,
                            .accumulated_at = 1000,
                            .surfaced = true};
    HU_ASSERT_FALSE(hu_inner_thought_should_surface(&t, "cooking", 7, 2000));
}

static void test_inner_thought_should_surface_stale_false(void) {
    hu_inner_thought_t t = {.contact_id = "c1",
                            .contact_id_len = 2,
                            .relevance_score = 0.9,
                            .accumulated_at = 0,
                            .surfaced = false};
    /* 15 days later in ms = 15 * 86400 * 1000 */
    uint64_t now = 15ULL * 86400ULL * 1000ULL;
    HU_ASSERT_FALSE(hu_inner_thought_should_surface(&t, NULL, 0, now));
}

static void test_inner_thought_should_surface_low_relevance_suppressed(void) {
    hu_inner_thought_t t = {.contact_id = "c1",
                            .contact_id_len = 2,
                            .relevance_score = 0.2,
                            .accumulated_at = 1000,
                            .surfaced = false};
    HU_ASSERT_FALSE(hu_inner_thought_should_surface(&t, NULL, 0, 2000));
}

static void test_inner_thought_should_surface_high_relevance_no_topic(void) {
    hu_inner_thought_t t = {.contact_id = "c1",
                            .contact_id_len = 2,
                            .relevance_score = 0.8,
                            .accumulated_at = 1000,
                            .surfaced = false};
    HU_ASSERT_TRUE(hu_inner_thought_should_surface(&t, NULL, 0, 2000));
}

static void test_inner_thought_should_surface_medium_relevance_no_topic_false(void) {
    hu_inner_thought_t t = {.contact_id = "c1",
                            .contact_id_len = 2,
                            .relevance_score = 0.5,
                            .accumulated_at = 1000,
                            .surfaced = false};
    /* 0.5 relevance without topic match should NOT surface (threshold is 0.6) */
    HU_ASSERT_FALSE(hu_inner_thought_should_surface(&t, NULL, 0, 2000));
}

static void test_inner_thought_should_surface_topic_match_boosts(void) {
    hu_inner_thought_t t = {.contact_id = "c1",
                            .contact_id_len = 2,
                            .topic = "cooking",
                            .topic_len = 7,
                            .relevance_score = 0.4, /* below 0.6 threshold */
                            .accumulated_at = 1000,
                            .surfaced = false};
    /* Topic match should override the relevance threshold */
    HU_ASSERT_TRUE(hu_inner_thought_should_surface(&t, "talking about cooking today", 27, 2000));
}

/* --- Surface --- */

static void test_inner_thought_surface_per_contact_isolation(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "thought for A", 13, 0.9, 1000);
    hu_inner_thought_accumulate(&store, "user_b", 6, NULL, 0, "thought for B", 13, 0.9, 1000);

    hu_inner_thought_t *surfaced[3];
    size_t count = hu_inner_thought_surface(&store, "user_a", 6, NULL, 0, 2000, surfaced, 3);

    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_STR_EQ(surfaced[0]->thought_text, "thought for A");

    /* user_b thought should still be pending */
    HU_ASSERT_EQ(hu_inner_thought_count_pending(&store, "user_b", 6), 1);

    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_surface_marks_as_surfaced(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "ask about trip", 14, 0.9, 1000);

    hu_inner_thought_t *surfaced[3];
    size_t count = hu_inner_thought_surface(&store, "user_a", 6, NULL, 0, 2000, surfaced, 3);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_TRUE(surfaced[0]->surfaced);

    /* Second call should return 0 — already surfaced */
    count = hu_inner_thought_surface(&store, "user_a", 6, NULL, 0, 2000, surfaced, 3);
    HU_ASSERT_EQ(count, 0);

    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_surface_selects_highest_relevance(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "low thought", 11, 0.6, 1000);
    hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "high thought", 12, 0.95, 1000);
    hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "mid thought", 11, 0.7, 1000);

    hu_inner_thought_t *surfaced[1]; /* only request 1 */
    size_t count = hu_inner_thought_surface(&store, "user_a", 6, NULL, 0, 2000, surfaced, 1);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_STR_EQ(surfaced[0]->thought_text, "high thought");

    hu_inner_thought_store_deinit(&store);
}

static void test_inner_thought_surface_respects_max_count(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    for (int i = 0; i < 5; i++) {
        char text[32];
        int n = snprintf(text, sizeof(text), "thought_%d", i);
        hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, text, (size_t)n, 0.8, 1000);
    }

    hu_inner_thought_t *surfaced[3];
    size_t count = hu_inner_thought_surface(&store, "user_a", 6, NULL, 0, 2000, surfaced, 3);
    HU_ASSERT_EQ(count, 3);

    hu_inner_thought_store_deinit(&store);
}

/* --- Count pending --- */

static void test_inner_thought_count_pending_basic(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    hu_inner_thought_store_init(&store, &alloc);

    hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "t1", 2, 0.9, 1000);
    hu_inner_thought_accumulate(&store, "user_a", 6, NULL, 0, "t2", 2, 0.9, 1000);
    hu_inner_thought_accumulate(&store, "user_b", 6, NULL, 0, "t3", 2, 0.9, 1000);

    HU_ASSERT_EQ(hu_inner_thought_count_pending(&store, "user_a", 6), 2);
    HU_ASSERT_EQ(hu_inner_thought_count_pending(&store, "user_b", 6), 1);
    HU_ASSERT_EQ(hu_inner_thought_count_pending(&store, "user_c", 6), 0);

    hu_inner_thought_store_deinit(&store);
}

void run_inner_thoughts_tests(void) {
    HU_TEST_SUITE("inner_thoughts");
    HU_RUN_TEST(test_inner_thought_store_init_null_returns_error);
    HU_RUN_TEST(test_inner_thought_store_init_success);
    HU_RUN_TEST(test_inner_thought_accumulate_null_store_returns_error);
    HU_RUN_TEST(test_inner_thought_accumulate_empty_contact_returns_error);
    HU_RUN_TEST(test_inner_thought_accumulate_empty_text_returns_error);
    HU_RUN_TEST(test_inner_thought_accumulate_invalid_relevance_returns_error);
    HU_RUN_TEST(test_inner_thought_accumulate_stores_correctly);
    HU_RUN_TEST(test_inner_thought_accumulate_null_topic_ok);
    HU_RUN_TEST(test_inner_thought_accumulate_grows_capacity);
    HU_RUN_TEST(test_inner_thought_accumulate_evicts_oldest_at_max_capacity);
    HU_RUN_TEST(test_inner_thought_should_surface_null_returns_false);
    HU_RUN_TEST(test_inner_thought_should_surface_already_surfaced_false);
    HU_RUN_TEST(test_inner_thought_should_surface_stale_false);
    HU_RUN_TEST(test_inner_thought_should_surface_low_relevance_suppressed);
    HU_RUN_TEST(test_inner_thought_should_surface_high_relevance_no_topic);
    HU_RUN_TEST(test_inner_thought_should_surface_medium_relevance_no_topic_false);
    HU_RUN_TEST(test_inner_thought_should_surface_topic_match_boosts);
    HU_RUN_TEST(test_inner_thought_surface_per_contact_isolation);
    HU_RUN_TEST(test_inner_thought_surface_marks_as_surfaced);
    HU_RUN_TEST(test_inner_thought_surface_selects_highest_relevance);
    HU_RUN_TEST(test_inner_thought_surface_respects_max_count);
    HU_RUN_TEST(test_inner_thought_count_pending_basic);
}
