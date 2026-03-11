#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/visual/content.h"
#include "test_framework.h"
#include <string.h>

static void test_visual_create_table_sql_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_visual_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "visual_content") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "share_count") != NULL);
}

static void test_visual_insert_sql_escapes_quotes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_visual_candidate_t c = {
        .path = hu_strndup(&alloc, "/photos/o'brien.jpg", 18),
        .path_len = 18,
        .description = NULL,
        .description_len = 0,
        .relevance_score = 0.5,
        .sharing_context = NULL,
        .sharing_context_len = 0,
        .type = HU_VISUAL_PHOTO,
        .captured_at = 1000,
    };
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_visual_insert_sql(&c, "camera", 6, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "''") != NULL);
    hu_visual_candidate_deinit(&alloc, &c);
}

static void test_visual_query_recent_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_visual_query_recent_sql(1234567890ULL, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "captured_at >") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "ORDER BY captured_at DESC") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "LIMIT 50") != NULL);
}

static void test_visual_record_share_sql_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err =
        hu_visual_record_share_sql(42, "alice", 5, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "share_count = share_count + 1") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "42") != NULL);
}

static void test_visual_count_shares_today_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_visual_count_shares_today_sql("user_123", 8, 1000000ULL, buf,
                                                       sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "COUNT(*)") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "shared_with") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "indexed_at") != NULL);
}

static void test_visual_decide_no_budget(void) {
    hu_visual_type_t t = hu_visual_decide(0.9, 12, 2, 2, true, 0.15, 12345);
    HU_ASSERT_EQ(t, HU_VISUAL_NONE);
}

static void test_visual_decide_high_probability(void) {
    /* Try seeds until we get a non-NONE (high closeness, daytime, contact sends photos) */
    hu_visual_type_t t = HU_VISUAL_NONE;
    for (uint32_t seed = 0; seed < 1000 && t == HU_VISUAL_NONE; seed++) {
        t = hu_visual_decide(0.9, 14, 0, 2, true, 0.5, seed);
    }
    HU_ASSERT_TRUE(t != HU_VISUAL_NONE);
}

static void test_visual_decide_late_night_reduces(void) {
    /* At hour 2, probability is halved. Run many seeds and expect mostly NONE. */
    size_t non_none = 0;
    for (uint32_t seed = 0; seed < 500; seed++) {
        hu_visual_type_t t = hu_visual_decide(0.5, 2, 0, 2, true, 0.15, seed);
        if (t != HU_VISUAL_NONE)
            non_none++;
    }
    /* Late night + base 0.15 * 0.5 = 0.075, so we expect few hits */
    HU_ASSERT_TRUE(non_none < 100);
}

static void test_visual_decide_no_photos_contact(void) {
    /* contact_sends_photos=false multiplies prob by 0.3 */
    size_t non_none = 0;
    for (uint32_t seed = 0; seed < 500; seed++) {
        hu_visual_type_t t = hu_visual_decide(0.5, 14, 0, 2, false, 0.15, seed);
        if (t != HU_VISUAL_NONE)
            non_none++;
    }
    HU_ASSERT_TRUE(non_none < 80);
}

static void test_visual_score_relevance_exact_match(void) {
    double s = hu_visual_score_relevance("sunset at the lake", 18, "sunset lake", 11);
    HU_ASSERT_TRUE(s > 0.9);
}

static void test_visual_score_relevance_no_match(void) {
    double s = hu_visual_score_relevance("sunset at the lake", 18, "hiking trails", 13);
    HU_ASSERT_TRUE(s < 0.01);
}

static void test_visual_score_relevance_partial(void) {
    /* Topic "hiking weather" - only "hiking" matches in "article about hiking trails" */
    double s = hu_visual_score_relevance("article about hiking trails", 27, "hiking weather", 14);
    HU_ASSERT_TRUE(s > 0.3);
    HU_ASSERT_TRUE(s < 1.0);
}

static void test_visual_build_sharing_context_photo(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_visual_build_sharing_context(&alloc, HU_VISUAL_PHOTO, "sunset", 6, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    hu_str_free(&alloc, out);
}

static void test_visual_build_sharing_context_link(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_visual_build_sharing_context(&alloc, HU_VISUAL_LINK, "article", 7, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    hu_str_free(&alloc, out);
}

static void test_visual_build_prompt_with_candidates(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_visual_candidate_t c1 = {
        .path = hu_strndup(&alloc, "/p1.jpg", 7),
        .path_len = 7,
        .description = hu_strndup(&alloc, "sunset at the lake", 18),
        .description_len = 18,
        .relevance_score = 0.85,
        .sharing_context = hu_strndup(&alloc, "took this yesterday", 19),
        .sharing_context_len = 19,
        .type = HU_VISUAL_PHOTO_WITH_TEXT,
        .captured_at = 1000,
    };
    hu_visual_candidate_t c2 = {
        .path = hu_strndup(&alloc, "/link1", 6),
        .path_len = 6,
        .description = hu_strndup(&alloc, "article about hiking trails", 26),
        .description_len = 26,
        .relevance_score = 0.70,
        .sharing_context = hu_strndup(&alloc, "thought you'd like this", 24),
        .sharing_context_len = 24,
        .type = HU_VISUAL_LINK_WITH_TEXT,
        .captured_at = 2000,
    };
    hu_visual_candidate_t candidates[2] = {c1, c2};
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_visual_build_prompt(&alloc, candidates, 2, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "VISUAL CONTENT AVAILABLE") != NULL);
    HU_ASSERT_TRUE(strstr(out, "sunset at the lake") != NULL);
    HU_ASSERT_TRUE(strstr(out, "article") != NULL);
    HU_ASSERT_TRUE(strstr(out, "0.85") != NULL);
    hu_str_free(&alloc, out);
    hu_visual_candidate_deinit(&alloc, &c1);
    hu_visual_candidate_deinit(&alloc, &c2);
}

static void test_visual_build_prompt_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_visual_build_prompt(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "No visual content") != NULL);
    hu_str_free(&alloc, out);
}

static void test_visual_type_str_all_types(void) {
    HU_ASSERT_NOT_NULL(hu_visual_type_str(HU_VISUAL_NONE));
    HU_ASSERT_NOT_NULL(hu_visual_type_str(HU_VISUAL_PHOTO));
    HU_ASSERT_NOT_NULL(hu_visual_type_str(HU_VISUAL_LINK));
    HU_ASSERT_NOT_NULL(hu_visual_type_str(HU_VISUAL_SCREENSHOT));
    HU_ASSERT_NOT_NULL(hu_visual_type_str(HU_VISUAL_PHOTO_WITH_TEXT));
    HU_ASSERT_NOT_NULL(hu_visual_type_str(HU_VISUAL_LINK_WITH_TEXT));
}

static void test_visual_candidate_deinit_frees(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_visual_candidate_t c = {
        .path = hu_strndup(&alloc, "/p.jpg", 6),
        .path_len = 6,
        .description = hu_strndup(&alloc, "desc", 4),
        .description_len = 4,
        .relevance_score = 0.5,
        .sharing_context = hu_strndup(&alloc, "ctx", 3),
        .sharing_context_len = 3,
        .type = HU_VISUAL_PHOTO,
        .captured_at = 0,
    };
    hu_visual_candidate_deinit(&alloc, &c);
    HU_ASSERT_NULL(c.path);
    HU_ASSERT_NULL(c.description);
    HU_ASSERT_NULL(c.sharing_context);
}

void run_visual_content_tests(void) {
    HU_TEST_SUITE("visual_content");
    HU_RUN_TEST(test_visual_create_table_sql_valid);
    HU_RUN_TEST(test_visual_insert_sql_escapes_quotes);
    HU_RUN_TEST(test_visual_query_recent_sql_valid);
    HU_RUN_TEST(test_visual_record_share_sql_valid);
    HU_RUN_TEST(test_visual_count_shares_today_sql_valid);
    HU_RUN_TEST(test_visual_decide_no_budget);
    HU_RUN_TEST(test_visual_decide_high_probability);
    HU_RUN_TEST(test_visual_decide_late_night_reduces);
    HU_RUN_TEST(test_visual_decide_no_photos_contact);
    HU_RUN_TEST(test_visual_score_relevance_exact_match);
    HU_RUN_TEST(test_visual_score_relevance_no_match);
    HU_RUN_TEST(test_visual_score_relevance_partial);
    HU_RUN_TEST(test_visual_build_sharing_context_photo);
    HU_RUN_TEST(test_visual_build_sharing_context_link);
    HU_RUN_TEST(test_visual_build_prompt_with_candidates);
    HU_RUN_TEST(test_visual_build_prompt_empty);
    HU_RUN_TEST(test_visual_type_str_all_types);
    HU_RUN_TEST(test_visual_candidate_deinit_frees);
}
