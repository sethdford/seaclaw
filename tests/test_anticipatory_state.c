#include "test_framework.h"
#include "human/agent/inner_thoughts.h"
#include "human/core/allocator.h"
#include <string.h>

static void it_store_init_and_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    HU_ASSERT(hu_inner_thought_store_init(&store, &alloc) == HU_OK);
    HU_ASSERT(store.count == 0);
    HU_ASSERT(store.capacity > 0);
    hu_inner_thought_store_deinit(&store);
}

static void it_store_init_null_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT(hu_inner_thought_store_init(NULL, &alloc) == HU_ERR_INVALID_ARGUMENT);
    hu_inner_thought_store_t store;
    HU_ASSERT(hu_inner_thought_store_init(&store, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void it_accumulate_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    HU_ASSERT(hu_inner_thought_store_init(&store, &alloc) == HU_OK);

    HU_ASSERT(hu_inner_thought_accumulate(&store, "alice", 5, "yoga", 4,
                                           "saw article about her yoga class", 31,
                                           0.7, 1000000) == HU_OK);
    HU_ASSERT(store.count == 1);
    HU_ASSERT(store.items[0].relevance_score == 0.7);
    HU_ASSERT(store.items[0].accumulated_at == 1000000);
    HU_ASSERT(!store.items[0].surfaced);
    hu_inner_thought_store_deinit(&store);
}

static void it_accumulate_null_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    HU_ASSERT(hu_inner_thought_store_init(&store, &alloc) == HU_OK);

    HU_ASSERT(hu_inner_thought_accumulate(NULL, "a", 1, NULL, 0, "t", 1, 0.5, 0) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_inner_thought_accumulate(&store, NULL, 0, NULL, 0, "t", 1, 0.5, 0) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_inner_thought_accumulate(&store, "a", 1, NULL, 0, NULL, 0, 0.5, 0) == HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT(hu_inner_thought_accumulate(&store, "a", 1, NULL, 0, "t", 1, 1.5, 0) == HU_ERR_INVALID_ARGUMENT);
    hu_inner_thought_store_deinit(&store);
}

static void it_accumulate_multiple_contacts(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    HU_ASSERT(hu_inner_thought_store_init(&store, &alloc) == HU_OK);

    HU_ASSERT(hu_inner_thought_accumulate(&store, "alice", 5, NULL, 0,
                                           "thought about alice", 19, 0.5, 1000) == HU_OK);
    HU_ASSERT(hu_inner_thought_accumulate(&store, "bob", 3, NULL, 0,
                                           "thought about bob", 17, 0.6, 2000) == HU_OK);
    HU_ASSERT(store.count == 2);

    HU_ASSERT(hu_inner_thought_count_pending(&store, "alice", 5) == 1);
    HU_ASSERT(hu_inner_thought_count_pending(&store, "bob", 3) == 1);
    hu_inner_thought_store_deinit(&store);
}

static void it_should_surface_topic_match(void) {
    hu_inner_thought_t thought = {
        .contact_id = "alice",
        .contact_id_len = 5,
        .topic = "yoga",
        .topic_len = 4,
        .thought_text = "she mentioned yoga class",
        .thought_text_len = 24,
        .relevance_score = 0.4,
        .accumulated_at = 1000,
        .surfaced = false,
    };
    HU_ASSERT(hu_inner_thought_should_surface(&thought, "talking about yoga today", 23, 2000));
}

static void it_should_surface_high_relevance(void) {
    hu_inner_thought_t thought = {
        .contact_id = "bob",
        .contact_id_len = 3,
        .topic = "cooking",
        .topic_len = 7,
        .thought_text = "important thought",
        .thought_text_len = 17,
        .relevance_score = 0.8,
        .accumulated_at = 1000,
        .surfaced = false,
    };
    HU_ASSERT(hu_inner_thought_should_surface(&thought, "unrelated topic", 15, 2000));
}

static void it_should_not_surface_low_relevance(void) {
    hu_inner_thought_t thought = {
        .contact_id = "alice",
        .contact_id_len = 5,
        .topic = NULL,
        .topic_len = 0,
        .thought_text = "minor thought",
        .thought_text_len = 13,
        .relevance_score = 0.2,
        .accumulated_at = 1000,
        .surfaced = false,
    };
    HU_ASSERT(!hu_inner_thought_should_surface(&thought, "anything", 8, 2000));
}

static void it_should_not_surface_already_surfaced(void) {
    hu_inner_thought_t thought = {
        .contact_id = "alice",
        .contact_id_len = 5,
        .relevance_score = 0.9,
        .accumulated_at = 1000,
        .surfaced = true,
    };
    HU_ASSERT(!hu_inner_thought_should_surface(&thought, NULL, 0, 2000));
}

static void it_should_not_surface_stale(void) {
    hu_inner_thought_t thought = {
        .contact_id = "alice",
        .contact_id_len = 5,
        .relevance_score = 0.9,
        .accumulated_at = 0,
        .surfaced = false,
    };
    uint64_t ms_per_day = 86400ULL * 1000ULL;
    uint64_t far_future = 30ULL * ms_per_day;
    HU_ASSERT(!hu_inner_thought_should_surface(&thought, NULL, 0, far_future));
}

static void it_surface_returns_best(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    HU_ASSERT(hu_inner_thought_store_init(&store, &alloc) == HU_OK);

    hu_inner_thought_accumulate(&store, "alice", 5, "yoga", 4, "low", 3, 0.4, 1000);
    hu_inner_thought_accumulate(&store, "alice", 5, "yoga", 4, "high", 4, 0.9, 2000);
    hu_inner_thought_accumulate(&store, "alice", 5, "yoga", 4, "mid", 3, 0.7, 3000);

    hu_inner_thought_t *surfaced[2];
    size_t count = hu_inner_thought_surface(&store, "alice", 5, "yoga class", 10, 4000,
                                             surfaced, 2);
    HU_ASSERT(count == 2);
    HU_ASSERT(surfaced[0]->relevance_score >= surfaced[1]->relevance_score);
    HU_ASSERT(surfaced[0]->surfaced);
    HU_ASSERT(surfaced[1]->surfaced);
    hu_inner_thought_store_deinit(&store);
}

static void it_surface_null_returns_zero(void) {
    hu_inner_thought_t *surfaced[2];
    HU_ASSERT(hu_inner_thought_surface(NULL, "a", 1, NULL, 0, 0, surfaced, 2) == 0);
}

static void it_count_pending_filters_by_contact(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_inner_thought_store_t store;
    HU_ASSERT(hu_inner_thought_store_init(&store, &alloc) == HU_OK);

    hu_inner_thought_accumulate(&store, "alice", 5, NULL, 0, "t1", 2, 0.5, 1000);
    hu_inner_thought_accumulate(&store, "alice", 5, NULL, 0, "t2", 2, 0.6, 2000);
    hu_inner_thought_accumulate(&store, "bob", 3, NULL, 0, "t3", 2, 0.7, 3000);

    HU_ASSERT(hu_inner_thought_count_pending(&store, "alice", 5) == 2);
    HU_ASSERT(hu_inner_thought_count_pending(&store, "bob", 3) == 1);
    HU_ASSERT(hu_inner_thought_count_pending(&store, "unknown", 7) == 0);
    hu_inner_thought_store_deinit(&store);
}

static void it_count_pending_null_returns_zero(void) {
    HU_ASSERT(hu_inner_thought_count_pending(NULL, "a", 1) == 0);
}

static void it_deinit_null_safe(void) {
    hu_inner_thought_store_deinit(NULL);
}

void run_anticipatory_state_tests(void) {
    HU_TEST_SUITE("Inner Thoughts (Anticipatory State)");

    HU_RUN_TEST(it_store_init_and_deinit);
    HU_RUN_TEST(it_store_init_null_fails);
    HU_RUN_TEST(it_accumulate_succeeds);
    HU_RUN_TEST(it_accumulate_null_fails);
    HU_RUN_TEST(it_accumulate_multiple_contacts);
    HU_RUN_TEST(it_should_surface_topic_match);
    HU_RUN_TEST(it_should_surface_high_relevance);
    HU_RUN_TEST(it_should_not_surface_low_relevance);
    HU_RUN_TEST(it_should_not_surface_already_surfaced);
    HU_RUN_TEST(it_should_not_surface_stale);
    HU_RUN_TEST(it_surface_returns_best);
    HU_RUN_TEST(it_surface_null_returns_zero);
    HU_RUN_TEST(it_count_pending_filters_by_contact);
    HU_RUN_TEST(it_count_pending_null_returns_zero);
    HU_RUN_TEST(it_deinit_null_safe);
}
