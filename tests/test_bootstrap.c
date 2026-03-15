#include "human/bootstrap.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

static void bootstrap_null_ctx_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_EQ(hu_app_bootstrap(NULL, &alloc, NULL, false, false), HU_ERR_INVALID_ARGUMENT);
}

static void bootstrap_null_alloc_returns_error(void) {
    hu_app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    HU_ASSERT_EQ(hu_app_bootstrap(&ctx, NULL, NULL, false, false), HU_ERR_INVALID_ARGUMENT);
}

static void teardown_null_is_safe(void) {
    hu_app_teardown(NULL);
}

static void teardown_zero_ctx_is_safe(void) {
    hu_app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    hu_app_teardown(&ctx);
}

static void bootstrap_minimal_no_agent_no_channels(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    hu_error_t err = hu_app_bootstrap(&ctx, &alloc, NULL, false, false);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(ctx.alloc);
    HU_ASSERT_NOT_NULL(ctx.cfg);
    HU_ASSERT_NOT_NULL(ctx.tools);
    HU_ASSERT_TRUE(ctx.tools_count > 0);
    HU_ASSERT_TRUE(ctx.channel_count == 0);
    HU_ASSERT_FALSE(ctx.agent_ok);
    hu_app_teardown(&ctx);
}

static void bootstrap_with_agent(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    hu_error_t err = hu_app_bootstrap(&ctx, &alloc, NULL, true, false);
    if (err == HU_OK) {
        HU_ASSERT_NOT_NULL(ctx.provider);
        HU_ASSERT_NOT_NULL(ctx.memory);
        HU_ASSERT_TRUE(ctx.provider_ok);
        hu_app_teardown(&ctx);
    }
}

void run_bootstrap_tests(void) {
    HU_TEST_SUITE("Bootstrap");

    HU_RUN_TEST(bootstrap_null_ctx_returns_error);
    HU_RUN_TEST(bootstrap_null_alloc_returns_error);
    HU_RUN_TEST(teardown_null_is_safe);
    HU_RUN_TEST(teardown_zero_ctx_is_safe);
    HU_RUN_TEST(bootstrap_minimal_no_agent_no_channels);
    HU_RUN_TEST(bootstrap_with_agent);
}
