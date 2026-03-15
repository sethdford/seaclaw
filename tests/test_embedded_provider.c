#include "test_framework.h"
#include "human/providers/embedded.h"

static void test_embedded_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_embedded_config_t config = { .model_path = "test.gguf", .context_size = 2048, .threads = 4, .use_gpu = false };
    hu_provider_t provider;
    HU_ASSERT_EQ(hu_embedded_provider_create(&alloc, &config, &provider), HU_OK);
    HU_ASSERT(provider.vtable != NULL);
    HU_ASSERT_STR_EQ(provider.vtable->get_name(provider.ctx), "embedded");
    provider.vtable->deinit(provider.ctx, &alloc);
}

static void test_embedded_null(void) {
    hu_provider_t provider;
    HU_ASSERT_EQ(hu_embedded_provider_create(NULL, NULL, &provider), HU_ERR_INVALID_ARGUMENT);
}

void run_embedded_provider_tests(void) {
    HU_TEST_SUITE("Embedded Provider");
    HU_RUN_TEST(test_embedded_create);
    HU_RUN_TEST(test_embedded_null);
}
