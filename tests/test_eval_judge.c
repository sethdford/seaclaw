#include "test_framework.h"
#include "human/eval_judge.h"
#include <string.h>

static void eval_judge_scores_correct_answer_high(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "What is the capital?", 20,
        "Paris is the capital of France", 30,
        "Paris", 5, NULL, 0, 3, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.score >= 4);
    HU_ASSERT_TRUE(result.passed);
    hu_eval_judge_result_free(&alloc, &result);
}

static void eval_judge_scores_wrong_answer_low(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "What is 2+2?", 12,
        "The weather is nice today", 25,
        "4", 1, NULL, 0, 3, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.score <= 2);
    HU_ASSERT_TRUE(!result.passed);
    hu_eval_judge_result_free(&alloc, &result);
}

static void eval_judge_scores_partial_answer(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "Name three fruits", 18,
        "apple banana", 12,
        "apple banana cherry", 19, NULL, 0, 3, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.score >= 2 && result.score <= 4);
    hu_eval_judge_result_free(&alloc, &result);
}

static void eval_judge_uses_custom_rubric(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    const char *rubric = "Must mention concurrency and memory safety";
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "Why use Rust?", 13,
        "Rust provides memory safety and concurrency", 44,
        "memory safety concurrency", 25,
        rubric, strlen(rubric), 3, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.score >= 3);
    hu_eval_judge_result_free(&alloc, &result);
}

static void eval_judge_caches_results(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_cache_t cache;
    HU_ASSERT_EQ(hu_eval_judge_cache_create(&alloc, &cache), HU_OK);

    hu_eval_judge_result_t r1;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "q", 1, "correct answer", 14, "correct", 7, NULL, 0, 3, &cache, &r1);
    HU_ASSERT_EQ(err, HU_OK);

    hu_eval_judge_result_t r2;
    bool found = hu_eval_judge_cache_lookup(&cache, "correct answer", 14,
                                             "correct", 7, NULL, 0, &r2);
    HU_ASSERT_TRUE(found);
    HU_ASSERT_EQ(r2.score, r1.score);

    hu_eval_judge_result_free(&alloc, &r1);
    hu_eval_judge_result_free(&alloc, &r2);
    hu_eval_judge_cache_destroy(&cache);
}

static void eval_judge_null_args_rejected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    HU_ASSERT_EQ(hu_eval_judge_check(NULL, NULL, NULL, 0, NULL, 0, "a", 1,
                                      "b", 1, NULL, 0, 3, NULL, &result),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_eval_judge_check(&alloc, NULL, NULL, 0, NULL, 0, NULL, 0,
                                      "b", 1, NULL, 0, 3, NULL, &result),
                 HU_ERR_INVALID_ARGUMENT);
}

static void eval_judge_threshold_boundary(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_eval_judge_result_t r_low;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "q", 1, "xyz", 3, "abc def ghi jkl", 15, NULL, 0, 3, NULL, &r_low);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(r_low.score <= 2);
    HU_ASSERT_TRUE(!r_low.passed);
    hu_eval_judge_result_free(&alloc, &r_low);

    hu_eval_judge_result_t r_high;
    err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "q", 1, "abc def ghi jkl", 15, "abc def ghi jkl", 15, NULL, 0, 3, NULL, &r_high);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(r_high.score == 5);
    HU_ASSERT_TRUE(r_high.passed);
    hu_eval_judge_result_free(&alloc, &r_high);
}

static void eval_judge_reasoning_populated(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "q", 1, "the answer", 10, "the answer", 10, NULL, 0, 3, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.reasoning != NULL);
    HU_ASSERT_TRUE(result.reasoning_len > 0);
    hu_eval_judge_result_free(&alloc, &result);
}

static void eval_judge_cache_miss_different_input(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_cache_t cache;
    HU_ASSERT_EQ(hu_eval_judge_cache_create(&alloc, &cache), HU_OK);

    hu_eval_judge_result_t r1;
    (void)hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "q", 1, "alpha", 5, "alpha", 5, NULL, 0, 3, &cache, &r1);

    hu_eval_judge_result_t r2;
    bool found = hu_eval_judge_cache_lookup(&cache, "beta", 4, "alpha", 5,
                                             NULL, 0, &r2);
    HU_ASSERT_TRUE(!found);

    hu_eval_judge_result_free(&alloc, &r1);
    hu_eval_judge_cache_destroy(&cache);
}

static void eval_judge_raw_score_normalized(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "q", 1, "correct", 7, "correct", 7, NULL, 0, 3, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.raw_score >= 0.0 && result.raw_score <= 1.0);
    hu_eval_judge_result_free(&alloc, &result);
}

static void eval_judge_default_threshold_is_3(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_eval_judge_result_t result;
    hu_error_t err = hu_eval_judge_check(&alloc, NULL, NULL, 0,
        "q", 1, "abc def", 7, "abc def ghi jkl mno pqr", 23, NULL, 0, 0, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.passed == (result.score >= 3));
    hu_eval_judge_result_free(&alloc, &result);
}

void run_eval_judge_tests(void) {
    HU_TEST_SUITE("Eval Judge");
    HU_RUN_TEST(eval_judge_scores_correct_answer_high);
    HU_RUN_TEST(eval_judge_scores_wrong_answer_low);
    HU_RUN_TEST(eval_judge_scores_partial_answer);
    HU_RUN_TEST(eval_judge_uses_custom_rubric);
    HU_RUN_TEST(eval_judge_caches_results);
    HU_RUN_TEST(eval_judge_null_args_rejected);
    HU_RUN_TEST(eval_judge_threshold_boundary);
    HU_RUN_TEST(eval_judge_reasoning_populated);
    HU_RUN_TEST(eval_judge_cache_miss_different_input);
    HU_RUN_TEST(eval_judge_raw_score_normalized);
    HU_RUN_TEST(eval_judge_default_threshold_is_3);
}
