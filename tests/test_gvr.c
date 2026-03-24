#include "human/agent/gvr.h"
#include "test_framework.h"
#include <string.h>

static void test_gvr_check_pass(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_gvr_check_result_t result;

    const char *prompt = "What is 2+2?";
    const char *response = "The answer is 4.";
    hu_error_t err = hu_gvr_check(&alloc, &provider, "test-model", 10, prompt, strlen(prompt),
                                  response, strlen(response), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.verdict, (int)HU_GVR_PASS);
    HU_ASSERT_NULL(result.critique);
    hu_gvr_check_result_free(&alloc, &result);
}

static void test_gvr_check_fail(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_gvr_check_result_t result;

    const char *prompt = "What is 2+2?";
    const char *response = "The answer is error and wrong.";
    hu_error_t err = hu_gvr_check(&alloc, &provider, "test-model", 10, prompt, strlen(prompt),
                                  response, strlen(response), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.verdict, (int)HU_GVR_FAIL);
    HU_ASSERT_NOT_NULL(result.critique);
    HU_ASSERT_TRUE(result.critique_len > 0);
    hu_gvr_check_result_free(&alloc, &result);
}

static void test_gvr_revise(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};

    const char *prompt = "What is 2+2?";
    const char *response = "The answer is error.";
    const char *critique = "Contains factual error";
    char *revised = NULL;
    size_t revised_len = 0;

    hu_error_t err =
        hu_gvr_revise(&alloc, &provider, "test-model", 10, prompt, strlen(prompt), response,
                      strlen(response), critique, strlen(critique), &revised, &revised_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(revised);
    HU_ASSERT_TRUE(revised_len > 0);
    HU_ASSERT_STR_NOT_CONTAINS(revised, "error");
    alloc.free(alloc.ctx, revised, revised_len + 1);
}

static void test_gvr_pipeline_pass_no_revision(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_gvr_config_t config = {.enabled = true, .max_revisions = 2};
    hu_gvr_pipeline_result_t result;

    const char *prompt = "What is 2+2?";
    const char *response = "The answer is 4.";
    hu_error_t err = hu_gvr_pipeline(&alloc, &provider, &config, "test-model", 10, prompt,
                                     strlen(prompt), response, strlen(response), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.final_verdict, (int)HU_GVR_PASS);
    HU_ASSERT_EQ((int)result.revisions_performed, 0);
    HU_ASSERT_NOT_NULL(result.final_content);
    hu_gvr_pipeline_result_free(&alloc, &result);
}

static void test_gvr_pipeline_fail_then_pass(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_gvr_config_t config = {.enabled = true, .max_revisions = 3};
    hu_gvr_pipeline_result_t result;

    const char *prompt = "What is 2+2?";
    const char *response = "The answer is error.";
    hu_error_t err = hu_gvr_pipeline(&alloc, &provider, &config, "test-model", 10, prompt,
                                     strlen(prompt), response, strlen(response), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.final_verdict, (int)HU_GVR_PASS);
    HU_ASSERT_GT((int)result.revisions_performed, 0);
    HU_ASSERT_NOT_NULL(result.final_content);
    hu_gvr_pipeline_result_free(&alloc, &result);
}

static void test_gvr_pipeline_disabled(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_gvr_config_t config = {.enabled = false};
    hu_gvr_pipeline_result_t result;

    const char *prompt = "test";
    const char *response = "error wrong bad";
    hu_error_t err = hu_gvr_pipeline(&alloc, &provider, &config, "m", 1, prompt, strlen(prompt),
                                     response, strlen(response), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.final_verdict, (int)HU_GVR_PASS);
    HU_ASSERT_EQ((int)result.revisions_performed, 0);
    HU_ASSERT_STR_EQ(result.final_content, response);
    hu_gvr_pipeline_result_free(&alloc, &result);
}

static void test_gvr_pipeline_null_config(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_gvr_pipeline_result_t result;

    const char *prompt = "test";
    const char *response = "response";
    hu_error_t err = hu_gvr_pipeline(&alloc, &provider, NULL, "m", 1, prompt, strlen(prompt),
                                     response, strlen(response), &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.final_verdict, (int)HU_GVR_PASS);
    hu_gvr_pipeline_result_free(&alloc, &result);
}

static void test_gvr_check_null_args(void) {
    hu_gvr_check_result_t result;
    HU_ASSERT_EQ(hu_gvr_check(NULL, NULL, NULL, 0, NULL, 0, NULL, 0, &result),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_gvr_revise_null_args(void) {
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_gvr_revise(NULL, NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_gvr_pipeline_null_args(void) {
    hu_gvr_pipeline_result_t result;
    HU_ASSERT_EQ(hu_gvr_pipeline(NULL, NULL, NULL, NULL, 0, NULL, 0, NULL, 0, &result),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_gvr_check_empty_response(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {0};
    hu_gvr_check_result_t result;

    hu_error_t err = hu_gvr_check(&alloc, &provider, "m", 1, NULL, 0, NULL, 0, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.verdict, (int)HU_GVR_PASS);
    hu_gvr_check_result_free(&alloc, &result);
}

void run_gvr_tests(void) {
    HU_TEST_SUITE("GVR Pipeline");
    HU_RUN_TEST(test_gvr_check_pass);
    HU_RUN_TEST(test_gvr_check_fail);
    HU_RUN_TEST(test_gvr_revise);
    HU_RUN_TEST(test_gvr_pipeline_pass_no_revision);
    HU_RUN_TEST(test_gvr_pipeline_fail_then_pass);
    HU_RUN_TEST(test_gvr_pipeline_disabled);
    HU_RUN_TEST(test_gvr_pipeline_null_config);
    HU_RUN_TEST(test_gvr_check_null_args);
    HU_RUN_TEST(test_gvr_revise_null_args);
    HU_RUN_TEST(test_gvr_pipeline_null_args);
    HU_RUN_TEST(test_gvr_check_empty_response);
}
