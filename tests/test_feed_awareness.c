#include "human/core/allocator.h"
#include "human/feeds/awareness.h"
#include "human/feeds/processor.h"
#include "human/persona.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HU_ENABLE_SQLITE
#include "human/memory.h"
#include <sqlite3.h>
#endif

static void feed_awareness_synthesize_test_returns_mock_topics(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_awareness_topic_t *topics = NULL;
    size_t n = 0;
    hu_error_t e = hu_feed_awareness_synthesize(&alloc, NULL, 0, NULL, &topics, &n);
    HU_ASSERT_EQ(e, HU_OK);
    HU_ASSERT_NOT_NULL(topics);
    HU_ASSERT_EQ(n, 2u);
    HU_ASSERT_STR_EQ(topics[0].source, "sports_feed");
    HU_ASSERT_TRUE(topics[0].relevance > 0.89 && topics[0].relevance < 0.91);
    HU_ASSERT_TRUE(strstr(topics[0].text, "Warriors") != NULL);
    HU_ASSERT_STR_EQ(topics[1].source, "local_news");
    HU_ASSERT_TRUE(topics[1].relevance > 0.69 && topics[1].relevance < 0.71);
    hu_feed_awareness_topics_free(&alloc, topics, n);
}

static void feed_awareness_should_share_rejects_sensitive_overlap(void) {
    hu_awareness_topic_t topic = {0};
    (void)snprintf(topic.text, sizeof(topic.text), "%s", "Did you see the election news today");
    (void)snprintf(topic.source, sizeof(topic.source), "%s", "news_rss: test");
    topic.relevance = 0.9;

    char sens_buf[] = "election politics";
    char *sensitive_ptrs[] = {sens_buf};
    hu_contact_profile_t cp = {0};
    cp.sensitive_topics = sensitive_ptrs;
    cp.sensitive_topics_count = 1;

    HU_ASSERT_FALSE(hu_feed_awareness_should_share(&topic, &cp));
}

static void feed_awareness_synthesize_non_empty_items_returns_ok_under_test(void) {
    static const char k_src[] = "rss:example_feed";
    static const char k_body[] = "Local team wins championship in overtime thriller";
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_item_t items[2];
    memset(items, 0, sizeof(items));
    items[0].type = HU_FEED_NEWS_RSS;
    items[0].source = (char *)(void *)k_src;
    items[0].source_len = sizeof(k_src) - 1u;
    items[0].content = (char *)(void *)k_body;
    items[0].content_len = sizeof(k_body) - 1u;
    items[0].fetched_at = 1u;
    items[1] = items[0];

    hu_awareness_topic_t *topics = NULL;
    size_t n = 0;
    hu_error_t e =
        hu_feed_awareness_synthesize(&alloc, items, 2, NULL, &topics, &n);
    HU_ASSERT_EQ(e, HU_OK);
    HU_ASSERT_NOT_NULL(topics);
    HU_ASSERT_EQ(n, 2u);
    HU_ASSERT_STR_EQ(topics[0].source, "sports_feed");
    hu_feed_awareness_topics_free(&alloc, topics, n);
}

static void feed_awareness_synthesize_null_feed_items_nonzero_count_ok_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_awareness_topic_t *topics = NULL;
    size_t n = 0;
    hu_error_t e = hu_feed_awareness_synthesize(&alloc, NULL, 3, NULL, &topics, &n);
    HU_ASSERT_EQ(e, HU_OK);
    HU_ASSERT_NOT_NULL(topics);
    HU_ASSERT_EQ(n, 2u);
    hu_feed_awareness_topics_free(&alloc, topics, n);
}

static void feed_awareness_synthesize_null_out_outputs_returns_invalid_argument(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_awareness_topic_t *topics = NULL;
    size_t n = 0;
    HU_ASSERT_EQ(hu_feed_awareness_synthesize(&alloc, NULL, 0, NULL, NULL, &n),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_feed_awareness_synthesize(&alloc, NULL, 0, NULL, &topics, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void feed_awareness_should_share_null_inputs_returns_false(void) {
    hu_awareness_topic_t topic = {0};
    hu_contact_profile_t cp = {0};
    HU_ASSERT_FALSE(hu_feed_awareness_should_share(NULL, &cp));
    HU_ASSERT_FALSE(hu_feed_awareness_should_share(&topic, NULL));
}

static void feed_awareness_should_share_accepts_shared_interest(void) {
    hu_awareness_topic_t topic = {0};
    (void)snprintf(topic.text, sizeof(topic.text), "%s",
                   "Did you see the Warriors game last night? Curry went off");
    (void)snprintf(topic.source, sizeof(topic.source), "%s", "sports_feed");
    topic.relevance = 0.9;

    char intr_buf[] = "Warriors";
    char *interest_ptrs[] = {intr_buf};
    hu_contact_profile_t cp = {0};
    cp.interests = interest_ptrs;
    cp.interests_count = 1;

    HU_ASSERT_TRUE(hu_feed_awareness_should_share(&topic, &cp));
}

#ifdef HU_ENABLE_SQLITE
static void feed_awareness_item_from_stored_maps_borrowed_fields(void) {
    hu_feed_item_stored_t stored;
    memset(&stored, 0, sizeof(stored));
    (void)snprintf(stored.source, sizeof(stored.source), "%s", "rss:fixture");
    stored.content_len = (size_t)snprintf(stored.content, sizeof(stored.content), "%s",
                                          "Headline about climate policy summit");
    (void)snprintf(stored.content_type, sizeof(stored.content_type), "%s",
                   hu_feed_type_str(HU_FEED_NEWS_RSS));
    stored.ingested_at = 1700000000;

    hu_feed_item_t out;
    memset(&out, 0, sizeof(out));
    hu_feed_awareness_item_from_stored(&stored, &out);
    HU_ASSERT_TRUE(out.content == stored.content);
    HU_ASSERT_EQ(out.content_len, stored.content_len);
    HU_ASSERT_TRUE(out.source == stored.source);
    HU_ASSERT_EQ(out.type, HU_FEED_NEWS_RSS);
    HU_ASSERT_EQ(out.fetched_at, 1700000000u);
}

static void feed_awareness_item_from_stored_null_no_crash(void) {
    hu_feed_item_t out;
    memset(&out, 0xAB, sizeof(out));
    hu_feed_awareness_item_from_stored(NULL, &out);
    hu_feed_item_stored_t stored;
    memset(&stored, 0, sizeof(stored));
    hu_feed_awareness_item_from_stored(&stored, NULL);
}

static void feed_awareness_contact_has_high_topics_true_when_contact_matches_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_feed_processor_t proc = {.alloc = &alloc, .db = db};
    hu_feed_item_stored_t row = {0};
    (void)snprintf(row.source, sizeof(row.source), "%s", "rss");
    (void)snprintf(row.content_type, sizeof(row.content_type), "%s", "article");
    row.content_len = (size_t)snprintf(row.content, sizeof(row.content), "%s",
                                       "placeholder ingested row for awareness path");
    row.ingested_at = (int64_t)time(NULL);
    HU_ASSERT_EQ(hu_feed_processor_store_item(&proc, &row), HU_OK);

    hu_persona_t persona;
    memset(&persona, 0, sizeof(persona));
    char intr[] = "Warriors";
    char *intr_ptrs[] = {intr};
    hu_contact_profile_t cp = {0};
    cp.interests = intr_ptrs;
    cp.interests_count = 1;

    HU_ASSERT_TRUE(hu_feed_awareness_contact_has_high_topics(&alloc, db, &persona, &cp, 0.5));
    mem.vtable->deinit(mem.ctx);
}

static void feed_awareness_contact_has_high_topics_false_when_min_above_mock_relevance(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_feed_processor_t proc = {.alloc = &alloc, .db = db};
    hu_feed_item_stored_t row = {0};
    (void)snprintf(row.source, sizeof(row.source), "%s", "rss");
    (void)snprintf(row.content_type, sizeof(row.content_type), "%s", "article");
    row.content_len = (size_t)snprintf(row.content, sizeof(row.content), "%s", "another row");
    row.ingested_at = (int64_t)time(NULL);
    HU_ASSERT_EQ(hu_feed_processor_store_item(&proc, &row), HU_OK);

    hu_persona_t persona;
    memset(&persona, 0, sizeof(persona));
    char intr[] = "Warriors";
    char *intr_ptrs[] = {intr};
    hu_contact_profile_t cp = {0};
    cp.interests = intr_ptrs;
    cp.interests_count = 1;

    HU_ASSERT_FALSE(
        hu_feed_awareness_contact_has_high_topics(&alloc, db, &persona, &cp, 0.95));
    mem.vtable->deinit(mem.ctx);
}

static void feed_awareness_contact_has_high_topics_invalid_args_false(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_memory_t mem = hu_sqlite_memory_create(&alloc, ":memory:");
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    HU_ASSERT_NOT_NULL(db);

    hu_persona_t persona;
    memset(&persona, 0, sizeof(persona));
    hu_contact_profile_t cp = {0};

    HU_ASSERT_FALSE(hu_feed_awareness_contact_has_high_topics(NULL, db, &persona, &cp, 0.5));
    HU_ASSERT_FALSE(hu_feed_awareness_contact_has_high_topics(&alloc, NULL, &persona, &cp, 0.5));
    HU_ASSERT_FALSE(hu_feed_awareness_contact_has_high_topics(&alloc, db, NULL, &cp, 0.5));
    HU_ASSERT_FALSE(hu_feed_awareness_contact_has_high_topics(&alloc, db, &persona, NULL, 0.5));
    HU_ASSERT_FALSE(hu_feed_awareness_contact_has_high_topics(&alloc, db, &persona, &cp, 0.0));

    mem.vtable->deinit(mem.ctx);
}
#endif /* HU_ENABLE_SQLITE */

void run_feed_awareness_tests(void) {
    HU_TEST_SUITE("feed_awareness");
    HU_RUN_TEST(feed_awareness_synthesize_test_returns_mock_topics);
    HU_RUN_TEST(feed_awareness_synthesize_non_empty_items_returns_ok_under_test);
    HU_RUN_TEST(feed_awareness_synthesize_null_feed_items_nonzero_count_ok_under_test);
    HU_RUN_TEST(feed_awareness_synthesize_null_out_outputs_returns_invalid_argument);
    HU_RUN_TEST(feed_awareness_should_share_null_inputs_returns_false);
    HU_RUN_TEST(feed_awareness_should_share_rejects_sensitive_overlap);
    HU_RUN_TEST(feed_awareness_should_share_accepts_shared_interest);
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(feed_awareness_item_from_stored_maps_borrowed_fields);
    HU_RUN_TEST(feed_awareness_item_from_stored_null_no_crash);
    HU_RUN_TEST(feed_awareness_contact_has_high_topics_true_when_contact_matches_mock);
    HU_RUN_TEST(feed_awareness_contact_has_high_topics_false_when_min_above_mock_relevance);
    HU_RUN_TEST(feed_awareness_contact_has_high_topics_invalid_args_false);
#endif
}
