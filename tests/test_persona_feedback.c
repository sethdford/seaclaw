#ifdef HU_ENABLE_PERSONA

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/persona.h"
#include "test_framework.h"
#include <string.h>

static void feedback_record_null_alloc_returns_invalid(void) {
    const char *name = "test-persona";
    hu_persona_feedback_t feedback = {0};
    feedback.channel = "cli";
    feedback.channel_len = 3;
    feedback.original_response = "original";
    feedback.original_response_len = 8;
    feedback.corrected_response = "corrected";
    feedback.corrected_response_len = 9;
    feedback.context = "context";
    feedback.context_len = 7;

    hu_error_t err = hu_persona_feedback_record(NULL, name, strlen(name), &feedback);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void feedback_record_null_persona_name_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_feedback_t feedback = {0};
    feedback.corrected_response = "corrected";
    feedback.corrected_response_len = 9;

    hu_error_t err = hu_persona_feedback_record(&alloc, NULL, 5, &feedback);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void feedback_record_zero_name_len_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *name = "test";
    hu_persona_feedback_t feedback = {0};
    feedback.corrected_response = "corrected";
    feedback.corrected_response_len = 9;

    hu_error_t err = hu_persona_feedback_record(&alloc, name, 0, &feedback);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void feedback_record_null_feedback_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *name = "test-persona";

    hu_error_t err = hu_persona_feedback_record(&alloc, name, strlen(name), NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void feedback_record_valid_returns_ok_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *name = "test-persona";
    hu_persona_feedback_t feedback = {0};
    feedback.channel = "cli";
    feedback.channel_len = 3;
    feedback.original_response = "original response";
    feedback.original_response_len = 17;
    feedback.corrected_response = "corrected response";
    feedback.corrected_response_len = 18;
    feedback.context = "user correction";
    feedback.context_len = 15;

    hu_error_t err = hu_persona_feedback_record(&alloc, name, strlen(name), &feedback);
    HU_ASSERT_EQ(err, HU_OK);
}

static void feedback_record_empty_data_returns_ok_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *name = "x";
    hu_persona_feedback_t feedback = {0};
    feedback.channel = NULL;
    feedback.channel_len = 0;
    feedback.original_response = "";
    feedback.original_response_len = 0;
    feedback.corrected_response = "";
    feedback.corrected_response_len = 0;
    feedback.context = NULL;
    feedback.context_len = 0;

    hu_error_t err = hu_persona_feedback_record(&alloc, name, 1, &feedback);
    HU_ASSERT_EQ(err, HU_OK);
}

static void feedback_apply_null_alloc_returns_invalid(void) {
    const char *name = "test-persona";
    hu_error_t err = hu_persona_feedback_apply(NULL, name, strlen(name));
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void feedback_apply_null_persona_name_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_persona_feedback_apply(&alloc, NULL, 5);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void feedback_apply_zero_name_len_returns_invalid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *name = "test";
    hu_error_t err = hu_persona_feedback_apply(&alloc, name, 0);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void feedback_apply_valid_returns_ok_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *name = "test-persona";

    hu_error_t err = hu_persona_feedback_apply(&alloc, name, strlen(name));
    HU_ASSERT_EQ(err, HU_OK);
}

void run_persona_feedback_tests(void) {
    HU_TEST_SUITE("PersonaFeedback");

    HU_RUN_TEST(feedback_record_null_alloc_returns_invalid);
    HU_RUN_TEST(feedback_record_null_persona_name_returns_invalid);
    HU_RUN_TEST(feedback_record_zero_name_len_returns_invalid);
    HU_RUN_TEST(feedback_record_null_feedback_returns_invalid);
    HU_RUN_TEST(feedback_record_valid_returns_ok_under_test);
    HU_RUN_TEST(feedback_record_empty_data_returns_ok_under_test);

    HU_RUN_TEST(feedback_apply_null_alloc_returns_invalid);
    HU_RUN_TEST(feedback_apply_null_persona_name_returns_invalid);
    HU_RUN_TEST(feedback_apply_zero_name_len_returns_invalid);
    HU_RUN_TEST(feedback_apply_valid_returns_ok_under_test);
}

#else

void run_persona_feedback_tests(void) { (void)0; }

#endif /* HU_ENABLE_PERSONA */
