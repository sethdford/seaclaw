#include "human/agent/data_quality.h"
#include "test_framework.h"
#include <string.h>

static void test_dq_valid_fragments(void) {
    hu_dq_config_t config = HU_DQ_CONFIG_DEFAULT;
    hu_dq_fragment_t frags[] = {
        {.content = "fragment one", .content_len = 12, .source = "memory", .source_len = 6},
        {.content = "fragment two", .content_len = 12, .source = "tool", .source_len = 4},
    };
    hu_dq_result_t result;
    HU_ASSERT_EQ(hu_dq_check(&config, frags, 2, &result), HU_OK);
    HU_ASSERT_TRUE(result.passed);
    HU_ASSERT_EQ((int)result.fragments_checked, 2);
    HU_ASSERT_EQ((int)result.fragments_passed, 2);
    HU_ASSERT_EQ((int)result.issue_count, 0);
}

static void test_dq_detect_empty(void) {
    hu_dq_config_t config = HU_DQ_CONFIG_DEFAULT;
    hu_dq_fragment_t frags[] = {
        {.content = NULL, .content_len = 0, .source = "mem", .source_len = 3},
    };
    hu_dq_result_t result;
    HU_ASSERT_EQ(hu_dq_check(&config, frags, 1, &result), HU_OK);
    HU_ASSERT_FALSE(result.passed);
    HU_ASSERT_EQ((int)result.issue_count, 1);
    HU_ASSERT_EQ((int)result.issues[0].type, (int)HU_DQ_EMPTY);
}

static void test_dq_detect_duplicate(void) {
    hu_dq_config_t config = HU_DQ_CONFIG_DEFAULT;
    hu_dq_fragment_t frags[] = {
        {.content = "same content", .content_len = 12, .source = "a", .source_len = 1},
        {.content = "same content", .content_len = 12, .source = "b", .source_len = 1},
    };
    hu_dq_result_t result;
    HU_ASSERT_EQ(hu_dq_check(&config, frags, 2, &result), HU_OK);
    HU_ASSERT_FALSE(result.passed);
    HU_ASSERT_EQ((int)result.duplicates_found, 1);
}

static void test_dq_detect_too_long(void) {
    hu_dq_config_t config = HU_DQ_CONFIG_DEFAULT;
    config.max_fragment_len = 5;
    hu_dq_fragment_t frags[] = {
        {.content = "this is too long", .content_len = 16, .source = "x", .source_len = 1},
    };
    hu_dq_result_t result;
    HU_ASSERT_EQ(hu_dq_check(&config, frags, 1, &result), HU_OK);
    HU_ASSERT_FALSE(result.passed);
    HU_ASSERT_EQ((int)result.issues[0].type, (int)HU_DQ_TOO_LONG);
}

static void test_dq_valid_utf8(void) {
    HU_ASSERT_TRUE(hu_dq_is_valid_utf8("hello", 5));
    HU_ASSERT_TRUE(hu_dq_is_valid_utf8("\xc3\xa9", 2));
    HU_ASSERT_TRUE(hu_dq_is_valid_utf8("\xe2\x9c\x93", 3));
    HU_ASSERT_TRUE(hu_dq_is_valid_utf8(NULL, 0));
}

static void test_dq_invalid_utf8(void) {
    HU_ASSERT_FALSE(hu_dq_is_valid_utf8("\xff\xfe", 2));
    HU_ASSERT_FALSE(hu_dq_is_valid_utf8("\xc0\x80", 2));
}

static void test_dq_encoding_check(void) {
    hu_dq_config_t config = HU_DQ_CONFIG_DEFAULT;
    config.check_encoding = true;
    hu_dq_fragment_t frags[] = {
        {.content = "\xff\xfe", .content_len = 2, .source = "x", .source_len = 1},
    };
    hu_dq_result_t result;
    HU_ASSERT_EQ(hu_dq_check(&config, frags, 1, &result), HU_OK);
    HU_ASSERT_FALSE(result.passed);
    HU_ASSERT_EQ((int)result.issues[0].type, (int)HU_DQ_ENCODING_ERROR);
}

static void test_dq_disabled(void) {
    hu_dq_config_t config = HU_DQ_CONFIG_DEFAULT;
    config.enabled = false;
    hu_dq_fragment_t frags[] = {
        {.content = NULL, .content_len = 0},
    };
    hu_dq_result_t result;
    HU_ASSERT_EQ(hu_dq_check(&config, frags, 1, &result), HU_OK);
    HU_ASSERT_TRUE(result.passed);
}

static void test_dq_report(void) {
    hu_dq_result_t result = {
        .fragments_checked = 10,
        .fragments_passed = 8,
        .issue_count = 2,
        .duplicates_found = 1,
        .passed = false,
    };
    char buf[512];
    size_t len = hu_dq_report(&result, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_CONTAINS(buf, "10 checked");
    HU_ASSERT_STR_CONTAINS(buf, "FAIL");
}

static void test_dq_null_args(void) {
    HU_ASSERT_EQ(hu_dq_check(NULL, NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_data_quality_tests(void) {
    HU_TEST_SUITE("Data Quality");
    HU_RUN_TEST(test_dq_valid_fragments);
    HU_RUN_TEST(test_dq_detect_empty);
    HU_RUN_TEST(test_dq_detect_duplicate);
    HU_RUN_TEST(test_dq_detect_too_long);
    HU_RUN_TEST(test_dq_valid_utf8);
    HU_RUN_TEST(test_dq_invalid_utf8);
    HU_RUN_TEST(test_dq_encoding_check);
    HU_RUN_TEST(test_dq_disabled);
    HU_RUN_TEST(test_dq_report);
    HU_RUN_TEST(test_dq_null_args);
}
