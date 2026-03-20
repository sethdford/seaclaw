#include "human/core/allocator.h"
#include "human/feeds/awareness.h"
#include "human/persona.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

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

void run_feed_awareness_tests(void) {
    HU_TEST_SUITE("feed_awareness");
    HU_RUN_TEST(feed_awareness_synthesize_test_returns_mock_topics);
    HU_RUN_TEST(feed_awareness_should_share_rejects_sensitive_overlap);
    HU_RUN_TEST(feed_awareness_should_share_accepts_shared_interest);
}
