#include "test_framework.h"
#include "human/security/moderation.h"

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

void run_moderation_tests(void) {
    HU_TEST_SUITE("Content Moderation");
    HU_RUN_TEST(test_mod_safe);
    HU_RUN_TEST(test_mod_violence);
    HU_RUN_TEST(test_mod_null);
}
