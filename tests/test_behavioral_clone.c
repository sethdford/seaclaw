#include "human/calibration/clone.h"
#include "test_framework.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_behavioral_clone_extract_mock_populates_patterns(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_clone_patterns_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_behavioral_clone_extract(&alloc, NULL, NULL, &p), HU_OK);
    HU_ASSERT_EQ(p.topic_starter_count, 3u);
    HU_ASSERT_STR_EQ(p.topic_starters[0], "hey — quick question");
    HU_ASSERT_FLOAT_EQ(p.initiation_frequency_per_day, 2.5, 0.001);
    HU_ASSERT_EQ(p.sign_off_count, 2u);
    HU_ASSERT_FLOAT_EQ(p.double_text_probability, 0.18, 0.001);
    HU_ASSERT_EQ(p.double_text_median_delay_sec, 95);
    HU_ASSERT_EQ(p.read_to_response_median_sec, 420);
    HU_ASSERT_EQ(p.read_to_response_p95_sec, 2700);
    HU_ASSERT_FLOAT_EQ(p.response_length_by_depth[3], 1.0 - 0.04 * 3.0, 0.001);
}

static void test_behavioral_clone_delta_mock_differs_from_full_extract(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_clone_patterns_t full;
    hu_clone_patterns_t delta;
    memset(&full, 0, sizeof(full));
    memset(&delta, 0, sizeof(delta));
    HU_ASSERT_EQ(hu_behavioral_clone_extract(&alloc, NULL, NULL, &full), HU_OK);
    HU_ASSERT_EQ(hu_behavioral_clone_delta(&alloc, NULL, 1000, &delta), HU_OK);
    HU_ASSERT_FLOAT_EQ(delta.initiation_frequency_per_day, 0.8, 0.001);
    HU_ASSERT_TRUE(fabs(delta.initiation_frequency_per_day - full.initiation_frequency_per_day) >
                   0.01);
}

static void test_behavioral_clone_update_persona_writes_json_file(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_clone_patterns_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_behavioral_clone_extract(&alloc, NULL, NULL, &p), HU_OK);

    char tmp_path[] = "/tmp/hu_behavioral_clone_XXXXXX";
    int fd = mkstemp(tmp_path);
    HU_ASSERT(fd >= 0);
    close(fd);

    HU_ASSERT_EQ(hu_behavioral_clone_update_persona(&alloc, &p, tmp_path), HU_OK);

    FILE *f = fopen(tmp_path, "r");
    HU_ASSERT_NOT_NULL(f);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    unlink(tmp_path);
    buf[n] = '\0';

    HU_ASSERT_TRUE(strstr(buf, "\"schema\":\"human.behavioral_clone.v1\"") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "\"topic_starters\"") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "\"persona_recommendations\"") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "\"read_to_response_p95_sec\"") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "\"avg_message_length\"") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "\"emoji_frequency\"") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "\"avg_response_time_sec\"") != NULL);
}

static void test_behavioral_clone_null_args_return_invalid_argument(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_clone_patterns_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_behavioral_clone_extract(NULL, NULL, NULL, &p), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_behavioral_clone_extract(&alloc, NULL, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_behavioral_clone_delta(&alloc, NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_behavioral_clone_update_persona(&alloc, &p, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_behavioral_clone_update_persona(&alloc, &p, ""), HU_ERR_INVALID_ARGUMENT);
}

void run_behavioral_clone_tests(void) {
    HU_TEST_SUITE("behavioral_clone");
    HU_RUN_TEST(test_behavioral_clone_extract_mock_populates_patterns);
    HU_RUN_TEST(test_behavioral_clone_delta_mock_differs_from_full_extract);
    HU_RUN_TEST(test_behavioral_clone_update_persona_writes_json_file);
    HU_RUN_TEST(test_behavioral_clone_null_args_return_invalid_argument);
}
