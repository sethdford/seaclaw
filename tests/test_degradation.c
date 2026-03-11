#include "human/core/allocator.h"
#include "human/core/string.h"
#include "human/memory/degradation.h"
#include "test_framework.h"
#include <string.h>

static void test_degradation_protected_commitment(void) {
    const char *s = "I promised to call";
    HU_ASSERT_TRUE(hu_degradation_is_protected(s, strlen(s)));
}

static void test_degradation_protected_emotional(void) {
    const char *s = "she died last year";
    HU_ASSERT_TRUE(hu_degradation_is_protected(s, strlen(s)));
}

static void test_degradation_not_protected_mundane(void) {
    const char *s = "we went to the store";
    HU_ASSERT_FALSE(hu_degradation_is_protected(s, strlen(s)));
}

static void test_degradation_roll_perfect(void) {
    hu_degradation_config_t config = {.perfect_rate = 0.90, .fuzz_rate = 0.05, .ask_rate = 0.05};
    size_t none_count = 0;
    for (uint32_t seed = 0; seed < 100; seed++) {
        if (hu_degradation_roll(&config, seed) == HU_DEGRADE_NONE)
            none_count++;
    }
    HU_ASSERT_TRUE(none_count >= 80);
}

static void test_degradation_roll_fuzz(void) {
    hu_degradation_config_t config = {.perfect_rate = 0.90, .fuzz_rate = 0.05, .ask_rate = 0.05};
    bool found = false;
    for (uint32_t seed = 0; seed < 10000 && !found; seed++) {
        if (hu_degradation_roll(&config, seed) == HU_DEGRADE_FUZZ)
            found = true;
    }
    HU_ASSERT_TRUE(found);
}

static void test_degradation_roll_ask(void) {
    hu_degradation_config_t config = {.perfect_rate = 0.90, .fuzz_rate = 0.05, .ask_rate = 0.05};
    bool found = false;
    for (uint32_t seed = 0; seed < 10000 && !found; seed++) {
        if (hu_degradation_roll(&config, seed) == HU_DEGRADE_ASK_REMIND)
            found = true;
    }
    HU_ASSERT_TRUE(found);
}

static void test_degradation_apply_none(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "We met on Tuesday";
    size_t len = strlen(content);
    size_t out_len = 0;
    char *r = hu_degradation_apply(&alloc, content, len, HU_DEGRADE_NONE, 0, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_STR_EQ(r, content);
    HU_ASSERT_EQ(out_len, len);
    hu_str_free(&alloc, r);
}

static void test_degradation_apply_fuzz_day(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "We met on Tuesday";
    size_t len = strlen(content);
    size_t out_len = 0;
    char *r = hu_degradation_apply(&alloc, content, len, HU_DEGRADE_FUZZ, 0, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_TRUE(strstr(r, "Wednesday") != NULL || strstr(r, "Monday") != NULL);
    HU_ASSERT_TRUE(strstr(r, "Tuesday") == NULL);
    hu_str_free(&alloc, r);
}

static void test_degradation_apply_ask_remind(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "We met on Tuesday at the cafe";
    size_t len = strlen(content);
    size_t out_len = 0;
    char *r = hu_degradation_apply(&alloc, content, len, HU_DEGRADE_ASK_REMIND, 0, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_TRUE(strstr(r, "remind me") != NULL);
    HU_ASSERT_TRUE(strstr(r, "what was") != NULL);
    hu_str_free(&alloc, r);
}

static void test_degradation_process_protected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "I promised to call back";
    size_t len = strlen(content);
    hu_degradation_config_t config = {.perfect_rate = 0.0, .fuzz_rate = 0.5, .ask_rate = 0.5};
    size_t out_len = 0;
    char *r = hu_degradation_process(&alloc, content, len, &config, 0, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_STR_EQ(r, content);
    hu_str_free(&alloc, r);
}

static void test_degradation_process_unprotected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *content = "we went to the store";
    size_t len = strlen(content);
    hu_degradation_config_t config = {.perfect_rate = 0.0, .fuzz_rate = 0.5, .ask_rate = 0.5};
    size_t out_len = 0;
    char *r = hu_degradation_process(&alloc, content, len, &config, 0, &out_len);
    HU_ASSERT_NOT_NULL(r);
    HU_ASSERT_TRUE(out_len > 0);
    hu_str_free(&alloc, r);
}

static void test_forgetting_retention_fresh(void) {
    hu_forgetting_config_t config = {
        .initial_retention = 1.0,
        .stability_factor = 0.7,
        .min_retention = 0.2,
        .rehearsal_boost = 2,
    };
    double r = hu_forgetting_retention(0.0, 1, &config);
    HU_ASSERT_TRUE(r > 0.99);
}

static void test_forgetting_retention_old(void) {
    hu_forgetting_config_t config = {
        .initial_retention = 1.0,
        .stability_factor = 0.7,
        .min_retention = 0.2,
        .rehearsal_boost = 2,
    };
    double r_fresh = hu_forgetting_retention(0.0, 1, &config);
    double r_old = hu_forgetting_retention(168.0, 1, &config);
    HU_ASSERT_TRUE(r_old < r_fresh);
}

static void test_forgetting_retention_rehearsed(void) {
    hu_forgetting_config_t config = {
        .initial_retention = 1.0,
        .stability_factor = 0.7,
        .min_retention = 0.001,
        .rehearsal_boost = 2,
    };
    double r_one = hu_forgetting_retention(24.0, 1, &config);
    double r_five = hu_forgetting_retention(24.0, 5, &config);
    HU_ASSERT_TRUE(r_five > r_one);
}

static void test_forgetting_retention_min(void) {
    hu_forgetting_config_t config = {
        .initial_retention = 1.0,
        .stability_factor = 0.7,
        .min_retention = 0.2,
        .rehearsal_boost = 2,
    };
    double r = hu_forgetting_retention(10000.0, 1, &config);
    HU_ASSERT_FLOAT_EQ(r, 0.2, 0.01);
}

static void test_forgetting_should_recall_fresh(void) {
    hu_forgetting_config_t config = {
        .initial_retention = 1.0,
        .stability_factor = 0.7,
        .min_retention = 0.2,
        .rehearsal_boost = 2,
    };
    size_t recalled = 0;
    for (uint32_t seed = 0; seed < 100; seed++) {
        if (hu_forgetting_should_recall(1.0, 1, &config, seed))
            recalled++;
    }
    HU_ASSERT_TRUE(recalled >= 90);
}

static void test_forgetting_should_recall_stale(void) {
    hu_forgetting_config_t config = {
        .initial_retention = 1.0,
        .stability_factor = 0.7,
        .min_retention = 0.2,
        .rehearsal_boost = 2,
    };
    size_t recalled = 0;
    for (uint32_t seed = 0; seed < 100; seed++) {
        if (hu_forgetting_should_recall(720.0, 0, &config, seed))
            recalled++;
    }
    HU_ASSERT_TRUE(recalled < 50);
}

static void test_forgetting_create_table_sql_valid(void) {
    char buf[1024];
    size_t len = 0;
    hu_error_t err = hu_forgetting_create_table_sql(buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_TRUE(strstr(buf, "CREATE TABLE") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "memory_recall_log") != NULL);
}

static void test_forgetting_log_recall_sql_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err = hu_forgetting_log_recall_sql(42, 1234567890ULL, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "INSERT INTO") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "memory_recall_log") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "42") != NULL);
}

static void test_forgetting_query_recalls_sql_valid(void) {
    char buf[256];
    size_t len = 0;
    hu_error_t err = hu_forgetting_query_recalls_sql(42, buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(buf, "SELECT") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "memory_recall_log") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "WHERE memory_id") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "ORDER BY recalled_at DESC") != NULL);
}

void run_degradation_tests(void) {
    HU_TEST_SUITE("degradation");
    HU_RUN_TEST(test_degradation_protected_commitment);
    HU_RUN_TEST(test_degradation_protected_emotional);
    HU_RUN_TEST(test_degradation_not_protected_mundane);
    HU_RUN_TEST(test_degradation_roll_perfect);
    HU_RUN_TEST(test_degradation_roll_fuzz);
    HU_RUN_TEST(test_degradation_roll_ask);
    HU_RUN_TEST(test_degradation_apply_none);
    HU_RUN_TEST(test_degradation_apply_fuzz_day);
    HU_RUN_TEST(test_degradation_apply_ask_remind);
    HU_RUN_TEST(test_degradation_process_protected);
    HU_RUN_TEST(test_degradation_process_unprotected);
    HU_RUN_TEST(test_forgetting_retention_fresh);
    HU_RUN_TEST(test_forgetting_retention_old);
    HU_RUN_TEST(test_forgetting_retention_rehearsed);
    HU_RUN_TEST(test_forgetting_retention_min);
    HU_RUN_TEST(test_forgetting_should_recall_fresh);
    HU_RUN_TEST(test_forgetting_should_recall_stale);
    HU_RUN_TEST(test_forgetting_create_table_sql_valid);
    HU_RUN_TEST(test_forgetting_log_recall_sql_valid);
    HU_RUN_TEST(test_forgetting_query_recalls_sql_valid);
}
