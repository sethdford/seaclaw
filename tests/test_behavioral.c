#include "human/context/behavioral.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void double_text_default_config_low_closeness(void) {
    hu_double_text_config_t config = {
        .probability = 0.15,
        .min_gap_seconds = 30,
        .max_gap_seconds = 300,
        .only_close_friends = true,
    };
    bool result = hu_should_double_text(0.1, 1000, 2000, &config, 42);
    (void)result;
}

static void double_text_build_prompt_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_double_text_build_prompt(&alloc, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
}

void run_behavioral_tests(void) {
    HU_TEST_SUITE("behavioral");
    HU_RUN_TEST(double_text_default_config_low_closeness);
    HU_RUN_TEST(double_text_build_prompt_succeeds);
}
