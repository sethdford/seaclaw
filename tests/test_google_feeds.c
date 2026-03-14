typedef int hu_test_google_feeds_unused_;

#ifdef HU_ENABLE_FEEDS

#include "human/core/allocator.h"
#include "human/feeds/google.h"
#include "test_framework.h"
#include <string.h>

static void google_photos_fetch_mock_returns_correct_source(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_ingest_item_t items[4];
    size_t count = 0;
    hu_error_t err = hu_google_photos_fetch(&alloc, NULL, "token", 5,
        items, 4, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2u);
    HU_ASSERT_STR_EQ(items[0].source, "google_photos");
    HU_ASSERT_STR_EQ(items[1].source, "google_photos");
}

static void google_photos_fetch_item_count_matches_expected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_ingest_item_t items[4];
    size_t count = 0;
    hu_error_t err = hu_google_photos_fetch(&alloc, NULL, "token", 5,
        items, 4, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, 2u);
}

static void google_photos_fetch_url_field_populated(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_ingest_item_t items[4];
    size_t count = 0;
    hu_error_t err = hu_google_photos_fetch(&alloc, NULL, "token", 5,
        items, 4, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 2u);
    HU_ASSERT_TRUE(items[0].url[0] != '\0');
    HU_ASSERT_TRUE(strstr(items[0].url, "photos.google.com") != NULL);
}

static void google_photos_fetch_insufficient_cap_rejects(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_ingest_item_t items[1];
    size_t count = 0;
    hu_error_t err = hu_google_photos_fetch(&alloc, NULL, "token", 5,
        items, 1, &count);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void google_photos_fetch_content_type_set(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_feed_ingest_item_t items[4];
    size_t count = 0;
    hu_error_t err = hu_google_photos_fetch(&alloc, NULL, "tok", 3,
        items, 4, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 1u);
    HU_ASSERT_TRUE(items[0].content_type[0] != '\0');
}

void run_google_feeds_tests(void) {
    HU_TEST_SUITE("google feeds");
    HU_RUN_TEST(google_photos_fetch_mock_returns_correct_source);
    HU_RUN_TEST(google_photos_fetch_item_count_matches_expected);
    HU_RUN_TEST(google_photos_fetch_url_field_populated);
    HU_RUN_TEST(google_photos_fetch_insufficient_cap_rejects);
    HU_RUN_TEST(google_photos_fetch_content_type_set);
}

#endif /* HU_ENABLE_FEEDS */
