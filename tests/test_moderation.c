#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/security/moderation.h"
#include <string.h>

static void test_mod_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "hello how are you", 17, &result), HU_OK);
    HU_ASSERT(!result.flagged);
}

static void test_mod_violence(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "kill them with violence and murder", 34, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.violence);
}

static void test_mod_null(void) {
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(NULL, "text", 4, &result), HU_ERR_INVALID_ARGUMENT);
}

static void test_mod_empty_string(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    memset(&result, 0xff, sizeof(result));
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "", 0, &result), HU_OK);
    HU_ASSERT(!result.flagged);
}

static void test_mod_hate_speech(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "hate group rhetoric", 19, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.hate);
}

static void test_mod_self_harm(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "mentions suicide ideation", 25, &result), HU_OK);
    HU_ASSERT(result.flagged);
    HU_ASSERT(result.self_harm);
}

static void test_mod_null_result(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, "safe text", 9, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_mod_null_text(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, NULL, 0, &result), HU_ERR_INVALID_ARGUMENT);
}

static void test_mod_zero_length(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_moderation_result_t result;
    static const char buf[] = "violence";
    HU_ASSERT_EQ(hu_moderation_check_local(&alloc, buf, 0, &result), HU_OK);
    HU_ASSERT(!result.flagged);
}

void run_moderation_tests(void) {
    HU_TEST_SUITE("Content Moderation");
    HU_RUN_TEST(test_mod_safe);
    HU_RUN_TEST(test_mod_violence);
    HU_RUN_TEST(test_mod_null);
    HU_RUN_TEST(test_mod_empty_string);
    HU_RUN_TEST(test_mod_hate_speech);
    HU_RUN_TEST(test_mod_self_harm);
    HU_RUN_TEST(test_mod_null_result);
    HU_RUN_TEST(test_mod_null_text);
    HU_RUN_TEST(test_mod_zero_length);
}
