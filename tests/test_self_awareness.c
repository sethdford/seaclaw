#include "human/context/self_awareness.h"
#include "human/core/allocator.h"
#include "human/core/string.h"
#include "test_framework.h"
#include <string.h>

static void self_awareness_create_table_sql_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_self_awareness_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "self_awareness_stats") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "reciprocity_scores") != NULL);
}

static void self_awareness_record_send_sql_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_self_awareness_record_send_sql("contact_a", 9, true, "work", 4, buf,
                                                       sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "INSERT") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "contact_a") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "ON CONFLICT") != NULL);
}

static void self_awareness_query_sql_valid(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err = hu_self_awareness_query_sql("user_123", 8, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "SELECT") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "user_123") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "self_awareness_stats") != NULL);
}

static void self_awareness_initiation_ratio_balanced(void) {
    hu_self_stats_t stats = {0};
    stats.initiations_week = 5;
    stats.their_initiations_week = 5;
    double r = hu_self_awareness_initiation_ratio(&stats);
    HU_ASSERT_TRUE(r > 0.49 && r < 0.51);
}

static void self_awareness_initiation_ratio_low(void) {
    hu_self_stats_t stats = {0};
    stats.initiations_week = 2;
    stats.their_initiations_week = 8;
    double r = hu_self_awareness_initiation_ratio(&stats);
    HU_ASSERT_TRUE(r > 0.19 && r < 0.21);
}

static void self_awareness_initiation_ratio_zero_total(void) {
    hu_self_stats_t stats = {0};
    stats.initiations_week = 0;
    stats.their_initiations_week = 0;
    double r = hu_self_awareness_initiation_ratio(&stats);
    HU_ASSERT_TRUE(r > 0.49 && r < 0.51);
}

static void self_awareness_topic_repeating_true(void) {
    hu_self_stats_t stats = {0};
    stats.topic_repeat_count = 4;
    HU_ASSERT_TRUE(hu_self_awareness_topic_repeating(&stats, 3));
}

static void self_awareness_topic_repeating_false(void) {
    hu_self_stats_t stats = {0};
    stats.topic_repeat_count = 1;
    HU_ASSERT_FALSE(hu_self_awareness_topic_repeating(&stats, 3));
}

static void self_awareness_directive_quiet(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_self_stats_t stats = {0};
    stats.initiations_week = 2;
    stats.their_initiations_week = 8;
    stats.topic_repeat_count = 0;
    stats.days_since_contact = 0;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_self_awareness_build_directive(&alloc, &stats, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "quiet") != NULL);
    hu_str_free(&alloc, out);
}

static void self_awareness_directive_topic_repeat(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_self_stats_t stats = {0};
    stats.initiations_week = 5;
    stats.their_initiations_week = 5;
    stats.topic_repeat_count = 4;
    stats.last_topic = "work";
    stats.last_topic_len = 4;
    stats.days_since_contact = 0;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_self_awareness_build_directive(&alloc, &stats, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "keep") != NULL);
    hu_str_free(&alloc, out);
}

static void self_awareness_directive_long_absence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_self_stats_t stats = {0};
    stats.initiations_week = 5;
    stats.their_initiations_week = 5;
    stats.topic_repeat_count = 0;
    stats.days_since_contact = 10;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_self_awareness_build_directive(&alloc, &stats, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "while") != NULL);
    hu_str_free(&alloc, out);
}

static void self_awareness_directive_nothing(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_self_stats_t stats = {0};
    stats.initiations_week = 5;
    stats.their_initiations_week = 5;
    stats.topic_repeat_count = 1;
    stats.days_since_contact = 3;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_self_awareness_build_directive(&alloc, &stats, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
}

static void reciprocity_compute_balanced(void) {
    hu_reciprocity_metrics_t m = hu_reciprocity_compute(5, 5, 5, 5, 5, 5, 5, 10);
    HU_ASSERT_TRUE(m.initiation_ratio > 0.49 && m.initiation_ratio < 0.51);
    HU_ASSERT_TRUE(m.question_balance > 0.49 && m.question_balance < 0.51);
    HU_ASSERT_TRUE(m.share_balance > 0.49 && m.share_balance < 0.51);
}

static void reciprocity_compute_imbalanced(void) {
    hu_reciprocity_metrics_t m = hu_reciprocity_compute(8, 2, 5, 5, 5, 5, 5, 10);
    HU_ASSERT_TRUE(m.initiation_ratio > 0.79 && m.initiation_ratio < 0.81);
}

static void reciprocity_upsert_sql_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_reciprocity_metrics_t m = {0.5, 0.5, 0.5, 0.8};
    hu_error_t err = hu_reciprocity_upsert_sql("contact_x", 9, &m, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "INSERT OR REPLACE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "reciprocity_scores") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "contact_x") != NULL);
}

static void reciprocity_directive_low_initiation(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_reciprocity_metrics_t m = {0.3, 0.5, 0.5, 0.5};

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_reciprocity_build_directive(&alloc, &m, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "receiving") != NULL);
    hu_str_free(&alloc, out);
}

static void reciprocity_directive_low_questions(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_reciprocity_metrics_t m = {0.5, 0.35, 0.5, 0.5};

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_reciprocity_build_directive(&alloc, &m, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "asking") != NULL);
    hu_str_free(&alloc, out);
}

static void reciprocity_directive_over_sharing(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_reciprocity_metrics_t m = {0.5, 0.5, 0.7, 0.5};

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_reciprocity_build_directive(&alloc, &m, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "sharing") != NULL);
    hu_str_free(&alloc, out);
}

static void reciprocity_directive_balanced(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_reciprocity_metrics_t m = {0.5, 0.5, 0.5, 0.5};

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_reciprocity_build_directive(&alloc, &m, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0u);
}

static void self_stats_deinit_frees(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_self_stats_t stats = {0};
    stats.contact_id = hu_strndup(&alloc, "test_contact", 12);
    stats.contact_id_len = 12;
    stats.last_topic = hu_strndup(&alloc, "work", 4);
    stats.last_topic_len = 4;

    hu_self_stats_deinit(&alloc, &stats);
    HU_ASSERT_NULL(stats.contact_id);
    HU_ASSERT_NULL(stats.last_topic);
}

void run_self_awareness_tests(void) {
    HU_TEST_SUITE("self_awareness");
    HU_RUN_TEST(self_awareness_create_table_sql_valid);
    HU_RUN_TEST(self_awareness_record_send_sql_valid);
    HU_RUN_TEST(self_awareness_query_sql_valid);
    HU_RUN_TEST(self_awareness_initiation_ratio_balanced);
    HU_RUN_TEST(self_awareness_initiation_ratio_low);
    HU_RUN_TEST(self_awareness_initiation_ratio_zero_total);
    HU_RUN_TEST(self_awareness_topic_repeating_true);
    HU_RUN_TEST(self_awareness_topic_repeating_false);
    HU_RUN_TEST(self_awareness_directive_quiet);
    HU_RUN_TEST(self_awareness_directive_topic_repeat);
    HU_RUN_TEST(self_awareness_directive_long_absence);
    HU_RUN_TEST(self_awareness_directive_nothing);
    HU_RUN_TEST(reciprocity_compute_balanced);
    HU_RUN_TEST(reciprocity_compute_imbalanced);
    HU_RUN_TEST(reciprocity_upsert_sql_valid);
    HU_RUN_TEST(reciprocity_directive_low_initiation);
    HU_RUN_TEST(reciprocity_directive_low_questions);
    HU_RUN_TEST(reciprocity_directive_over_sharing);
    HU_RUN_TEST(reciprocity_directive_balanced);
    HU_RUN_TEST(self_stats_deinit_frees);
}
