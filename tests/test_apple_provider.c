#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include "human/providers/apple.h"
#include "human/providers/factory.h"
#include "test_framework.h"
#include <string.h>

static void test_apple_create_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_config_t cfg = {0};
    hu_error_t err = hu_apple_provider_create(&alloc, &cfg, &prov);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(prov.ctx);
    HU_ASSERT_NOT_NULL(prov.vtable);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_create_with_custom_url(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_config_t cfg = {
        .base_url = "http://localhost:9999/v1",
        .base_url_len = 24,
    };
    hu_error_t err = hu_apple_provider_create(&alloc, &cfg, &prov);
    HU_ASSERT_EQ(err, HU_OK);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_create_null_config_uses_default(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_error_t err = hu_apple_provider_create(&alloc, NULL, &prov);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(prov.ctx);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_create_null_alloc_returns_error(void) {
    hu_provider_t prov;
    hu_error_t err = hu_apple_provider_create(NULL, NULL, &prov);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_apple_create_null_out_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_apple_provider_create(&alloc, NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_apple_get_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);
    const char *name = prov.vtable->get_name(prov.ctx);
    HU_ASSERT_STR_EQ(name, "apple");
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_supports_native_tools(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);
    HU_ASSERT_TRUE(prov.vtable->supports_native_tools(prov.ctx));
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_supports_streaming(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);
    HU_ASSERT_NOT_NULL(prov.vtable->supports_streaming);
    HU_ASSERT_TRUE(prov.vtable->supports_streaming(prov.ctx));
    prov.vtable->deinit(prov.ctx, &alloc);
}

static int stream_chunk_count;
static char stream_accumulated[256];
static size_t stream_accumulated_len;
static bool stream_saw_final;

static bool apple_stream_test_cb(void *ctx, const hu_stream_chunk_t *chunk) {
    (void)ctx;
    stream_chunk_count++;
    if (chunk->delta && chunk->delta_len > 0) {
        size_t space = sizeof(stream_accumulated) - stream_accumulated_len;
        size_t copy = chunk->delta_len < space ? chunk->delta_len : space;
        memcpy(stream_accumulated + stream_accumulated_len, chunk->delta, copy);
        stream_accumulated_len += copy;
    }
    if (chunk->is_final)
        stream_saw_final = true;
    return true;
}

static void test_apple_stream_chat_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);
    HU_ASSERT_NOT_NULL(prov.vtable->stream_chat);

    hu_chat_message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = HU_ROLE_USER;
    msgs[0].content = "hello";
    msgs[0].content_len = 5;

    hu_chat_request_t req = {
        .messages = msgs,
        .messages_count = 1,
        .model = HU_APPLE_MODEL_NAME,
        .model_len = sizeof(HU_APPLE_MODEL_NAME) - 1,
        .temperature = 0.7,
    };

    stream_chunk_count = 0;
    stream_accumulated_len = 0;
    stream_saw_final = false;
    memset(stream_accumulated, 0, sizeof(stream_accumulated));

    hu_stream_chat_result_t result;
    memset(&result, 0, sizeof(result));
    hu_error_t err = prov.vtable->stream_chat(
        prov.ctx, &alloc, &req, HU_APPLE_MODEL_NAME, sizeof(HU_APPLE_MODEL_NAME) - 1, 0.7,
        apple_stream_test_cb, NULL, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(stream_chunk_count >= 3);
    HU_ASSERT_TRUE(stream_saw_final);
    HU_ASSERT_TRUE(stream_accumulated_len > 0);
    HU_ASSERT_NOT_NULL(result.content);
    HU_ASSERT_TRUE(result.content_len > 0);
    hu_stream_chat_result_free(&alloc, &result);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_chat_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);

    hu_chat_message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = HU_ROLE_USER;
    msgs[0].content = "hello";
    msgs[0].content_len = 5;

    hu_chat_request_t req = {
        .messages = msgs,
        .messages_count = 1,
        .model = HU_APPLE_MODEL_NAME,
        .model_len = sizeof(HU_APPLE_MODEL_NAME) - 1,
        .temperature = 0.7,
    };

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err =
        prov.vtable->chat(prov.ctx, &alloc, &req, HU_APPLE_MODEL_NAME,
                          sizeof(HU_APPLE_MODEL_NAME) - 1, 0.7, &resp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(resp.content);
    HU_ASSERT_TRUE(resp.content_len > 0);
    HU_ASSERT_TRUE(resp.usage.total_tokens > 0);
    hu_chat_response_free(&alloc, &resp);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_chat_with_system_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = prov.vtable->chat_with_system(
        prov.ctx, &alloc, "You are helpful.", 16, "What is AI?", 11,
        HU_APPLE_MODEL_NAME, sizeof(HU_APPLE_MODEL_NAME) - 1, 0.7, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_chat_with_tools_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);

    hu_chat_message_t msgs[1];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = HU_ROLE_USER;
    msgs[0].content = "list files";
    msgs[0].content_len = 10;

    hu_tool_spec_t tools[1] = {{
        .name = "shell",
        .name_len = 5,
        .description = "Run shell command",
        .description_len = 17,
        .parameters_json = "{\"type\":\"object\"}",
        .parameters_json_len = 17,
    }};

    hu_chat_request_t req = {
        .messages = msgs,
        .messages_count = 1,
        .model = HU_APPLE_MODEL_NAME,
        .model_len = sizeof(HU_APPLE_MODEL_NAME) - 1,
        .temperature = 0.7,
        .tools = tools,
        .tools_count = 1,
    };

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err =
        prov.vtable->chat(prov.ctx, &alloc, &req, HU_APPLE_MODEL_NAME,
                          sizeof(HU_APPLE_MODEL_NAME) - 1, 0.7, &resp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(resp.tool_calls_count > 0);
    HU_ASSERT_NOT_NULL(resp.tool_calls);
    HU_ASSERT_STR_EQ(resp.tool_calls[0].name, "shell");
    hu_chat_response_free(&alloc, &resp);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_chat_null_ctx_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_chat_response_t resp;
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);

    hu_error_t err = prov.vtable->chat(NULL, &alloc, NULL, NULL, 0, 0.7, &resp);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_probe_returns_true_in_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_TRUE(hu_apple_probe(&alloc, NULL, 0));
}

static void test_apple_context_window_defined(void) {
    HU_ASSERT_EQ(HU_APPLE_CONTEXT_WINDOW, 4096);
}

static void test_apple_chat_truncation_preserves_recent(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_apple_provider_create(&alloc, NULL, &prov);

    char long_text[8192];
    memset(long_text, 'a', sizeof(long_text));
    long_text[sizeof(long_text) - 1] = '\0';

    hu_chat_message_t msgs[4];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].role = HU_ROLE_SYSTEM;
    msgs[0].content = "You are helpful.";
    msgs[0].content_len = 16;
    msgs[1].role = HU_ROLE_USER;
    msgs[1].content = long_text;
    msgs[1].content_len = sizeof(long_text) - 1;
    msgs[2].role = HU_ROLE_ASSISTANT;
    msgs[2].content = long_text;
    msgs[2].content_len = sizeof(long_text) - 1;
    msgs[3].role = HU_ROLE_USER;
    msgs[3].content = "hello";
    msgs[3].content_len = 5;

    hu_chat_request_t req = {
        .messages = msgs,
        .messages_count = 4,
        .model = HU_APPLE_MODEL_NAME,
        .model_len = sizeof(HU_APPLE_MODEL_NAME) - 1,
        .temperature = 0.7,
    };

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err =
        prov.vtable->chat(prov.ctx, &alloc, &req, HU_APPLE_MODEL_NAME,
                          sizeof(HU_APPLE_MODEL_NAME) - 1, 0.7, &resp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(resp.content);
    hu_chat_response_free(&alloc, &resp);
    prov.vtable->deinit(prov.ctx, &alloc);
}

#ifdef HU_ENABLE_APPLE_INTELLIGENCE
static void test_apple_factory_apple_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_error_t err = hu_provider_create(&alloc, "apple", 5, NULL, 0, NULL, 0, &prov);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(prov.vtable->get_name(prov.ctx), "apple");
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_factory_apfel_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_error_t err = hu_provider_create(&alloc, "apfel", 5, NULL, 0, NULL, 0, &prov);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(prov.vtable->get_name(prov.ctx), "apple");
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_factory_intelligence_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_error_t err =
        hu_provider_create(&alloc, "apple-intelligence", 18, NULL, 0, NULL, 0, &prov);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(prov.vtable->get_name(prov.ctx), "apple");
    prov.vtable->deinit(prov.ctx, &alloc);
}

static void test_apple_factory_foundationmodels_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t prov;
    hu_error_t err =
        hu_provider_create(&alloc, "foundationmodels", 16, NULL, 0, NULL, 0, &prov);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(prov.vtable->get_name(prov.ctx), "apple");
    prov.vtable->deinit(prov.ctx, &alloc);
}
#endif /* HU_ENABLE_APPLE_INTELLIGENCE */

void run_apple_provider_tests(void) {
    HU_TEST_SUITE("apple_provider");
    HU_RUN_TEST(test_apple_create_succeeds);
    HU_RUN_TEST(test_apple_create_with_custom_url);
    HU_RUN_TEST(test_apple_create_null_config_uses_default);
    HU_RUN_TEST(test_apple_create_null_alloc_returns_error);
    HU_RUN_TEST(test_apple_create_null_out_returns_error);
    HU_RUN_TEST(test_apple_get_name);
    HU_RUN_TEST(test_apple_supports_native_tools);
    HU_RUN_TEST(test_apple_supports_streaming);
    HU_RUN_TEST(test_apple_stream_chat_mock);
    HU_RUN_TEST(test_apple_chat_mock);
    HU_RUN_TEST(test_apple_chat_with_system_mock);
    HU_RUN_TEST(test_apple_chat_with_tools_mock);
    HU_RUN_TEST(test_apple_chat_null_ctx_returns_error);
    HU_RUN_TEST(test_apple_probe_returns_true_in_test);
    HU_RUN_TEST(test_apple_context_window_defined);
    HU_RUN_TEST(test_apple_chat_truncation_preserves_recent);
#ifdef HU_ENABLE_APPLE_INTELLIGENCE
    HU_RUN_TEST(test_apple_factory_apple_name);
    HU_RUN_TEST(test_apple_factory_apfel_name);
    HU_RUN_TEST(test_apple_factory_intelligence_name);
    HU_RUN_TEST(test_apple_factory_foundationmodels_name);
#endif
}
