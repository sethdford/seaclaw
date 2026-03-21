#include "test_framework.h"
#include "human/core/allocator.h"
#include "human/provider.h"
#include <string.h>

#if defined(HU_ENABLE_COREML)
#include "human/providers/coreml.h"
#include "human/providers/factory.h"
#endif

#if defined(HU_ENABLE_COREML)
static void coreml_provider_create_rejects_null_out(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_coreml_config_t cfg = {0};
    HU_ASSERT_EQ(hu_coreml_provider_create(&alloc, &cfg, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void coreml_provider_create_deinit_under_test_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_coreml_config_t cfg = {0};
    hu_provider_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_coreml_provider_create(&alloc, &cfg, &p), HU_OK);
    HU_ASSERT_NOT_NULL(p.vtable);
    p.vtable->deinit(p.ctx, &alloc);
}

static void coreml_chat_returns_mock_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_coreml_config_t cfg = {0};
    hu_provider_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_coreml_provider_create(&alloc, &cfg, &p), HU_OK);

    hu_chat_message_t msg = {.role = HU_ROLE_USER,
                             .content = "hello",
                             .content_len = 5,
                             .name = NULL,
                             .name_len = 0,
                             .tool_call_id = NULL,
                             .tool_call_id_len = 0,
                             .content_parts = NULL,
                             .content_parts_count = 0,
                             .tool_calls = NULL,
                             .tool_calls_count = 0};
    hu_chat_request_t req = {.messages = &msg,
                             .messages_count = 1,
                             .model = "mlx-community/Llama-3.2-3B-Instruct-4bit",
                             .model_len = strlen("mlx-community/Llama-3.2-3B-Instruct-4bit"),
                             .temperature = 0.7,
                             .max_tokens = 1024};

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    HU_ASSERT_EQ(p.vtable->chat(p.ctx, &alloc, &req, req.model, req.model_len, 0.7, &resp), HU_OK);
    HU_ASSERT_NOT_NULL(resp.content);
    HU_ASSERT_STR_EQ(resp.content, "CoreML mock response");
    hu_chat_response_free(&alloc, &resp);
    p.vtable->deinit(p.ctx, &alloc);
}

static void coreml_get_name_is_coreml(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_coreml_config_t cfg = {0};
    hu_provider_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_coreml_provider_create(&alloc, &cfg, &p), HU_OK);
    HU_ASSERT_STR_EQ(p.vtable->get_name(p.ctx), "coreml");
    HU_ASSERT_FALSE(p.vtable->supports_native_tools(p.ctx));
    p.vtable->deinit(p.ctx, &alloc);
}

static void factory_accepts_coreml_and_mlx_aliases(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t p;
    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_provider_create(&alloc, "coreml", 7, NULL, 0, NULL, 0, &p), HU_OK);
    HU_ASSERT_STR_EQ(p.vtable->get_name(p.ctx), "coreml");
    p.vtable->deinit(p.ctx, &alloc);

    memset(&p, 0, sizeof(p));
    HU_ASSERT_EQ(hu_provider_create(&alloc, "mlx", 3, NULL, 0, NULL, 0, &p), HU_OK);
    HU_ASSERT_STR_EQ(p.vtable->get_name(p.ctx), "coreml");
    p.vtable->deinit(p.ctx, &alloc);
}

/* Production chat on non-macOS uses HU_ERR_NOT_SUPPORTED; test builds always take the mock path. */
#endif

void run_coreml_provider_tests(void) {
#if defined(HU_ENABLE_COREML)
    HU_TEST_SUITE("coreml_provider");
    HU_RUN_TEST(coreml_provider_create_rejects_null_out);
    HU_RUN_TEST(coreml_provider_create_deinit_under_test_ok);
    HU_RUN_TEST(coreml_chat_returns_mock_under_test);
    HU_RUN_TEST(coreml_get_name_is_coreml);
    HU_RUN_TEST(factory_accepts_coreml_and_mlx_aliases);
#endif
}
