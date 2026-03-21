#include "test_framework.h"
#include "human/providers/embedded.h"

#include <string.h>

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

static void test_embedded_create_null_allocator_returns_error(void) {
    hu_embedded_config_t config = { .model_path = "test.gguf", .context_size = 2048, .threads = 4, .use_gpu = false };
    hu_provider_t provider;
    memset(&provider, 0, sizeof(provider));
    HU_ASSERT_EQ(hu_embedded_provider_create(NULL, &config, &provider), HU_ERR_INVALID_ARGUMENT);
}

static void test_embedded_provider_vtable_name_is_embedded(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_embedded_config_t config = { .model_path = "test.gguf", .context_size = 512, .threads = 1, .use_gpu = false };
    hu_provider_t provider;
    HU_ASSERT_EQ(hu_embedded_provider_create(&alloc, &config, &provider), HU_OK);
    HU_ASSERT_NOT_NULL(provider.vtable);
    HU_ASSERT_NOT_NULL(provider.vtable->get_name);
    HU_ASSERT_STR_EQ(provider.vtable->get_name(provider.ctx), "embedded");
    provider.vtable->deinit(provider.ctx, &alloc);
}

static void test_embedded_chat_with_system_null_message_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_embedded_config_t config = { .model_path = "test.gguf", .context_size = 512, .threads = 1, .use_gpu = false };
    hu_provider_t provider;
    HU_ASSERT_EQ(hu_embedded_provider_create(&alloc, &config, &provider), HU_OK);
    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(provider.vtable->chat_with_system(provider.ctx, &alloc, "sys", 3, NULL, 0, "m", 1, 0.7, &out,
                                                   &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(out, NULL);
    provider.vtable->deinit(provider.ctx, &alloc);
}

void run_embedded_provider_tests(void) {
    HU_TEST_SUITE("Embedded Provider");
    HU_RUN_TEST(test_embedded_create);
    HU_RUN_TEST(test_embedded_null);
    HU_RUN_TEST(test_embedded_create_null_allocator_returns_error);
    HU_RUN_TEST(test_embedded_provider_vtable_name_is_embedded);
    HU_RUN_TEST(test_embedded_chat_with_system_null_message_returns_error);
}

