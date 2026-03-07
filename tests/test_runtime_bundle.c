#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/provider.h"
#include "seaclaw/providers/runtime_bundle.h"
#include "test_framework.h"
#include <string.h>

/* Mock provider for testing */
static const char *mock_get_name(void *ctx) {
    (void)ctx;
    return "mock";
}

static void mock_deinit(void *ctx, sc_allocator_t *a) {
    (void)ctx;
    (void)a;
}

static sc_error_t mock_chat(void *ctx, sc_allocator_t *alloc, const sc_chat_request_t *request,
                            const char *model, size_t model_len, double temperature,
                            sc_chat_response_t *out) {
    (void)ctx;
    (void)request;
    (void)model;
    (void)model_len;
    (void)temperature;
    (void)alloc;
    memset(out, 0, sizeof(*out));
    return SC_OK;
}

static sc_error_t mock_chat_with_system(void *ctx, sc_allocator_t *alloc, const char *system_prompt,
                                        size_t system_prompt_len, const char *message,
                                        size_t message_len, const char *model, size_t model_len,
                                        double temperature, char **out, size_t *out_len) {
    (void)ctx;
    (void)alloc;
    (void)system_prompt;
    (void)system_prompt_len;
    (void)message;
    (void)message_len;
    (void)model;
    (void)model_len;
    (void)temperature;
    *out = NULL;
    *out_len = 0;
    return SC_OK;
}

static bool mock_supports_native_tools(void *ctx) {
    (void)ctx;
    return false;
}

static const sc_provider_vtable_t mock_vtable = {
    .chat_with_system = mock_chat_with_system,
    .chat = mock_chat,
    .supports_native_tools = mock_supports_native_tools,
    .get_name = mock_get_name,
    .deinit = mock_deinit,
};

static sc_provider_t make_mock_provider(void) {
    return (sc_provider_t){.ctx = NULL, .vtable = &mock_vtable};
}

/* ─────────────────────────────────────────────────────────────────────────
 * Tests
 * ───────────────────────────────────────────────────────────────────────── */

static void test_init_null_alloc_returns_invalid_argument(void) {
    sc_provider_t primary = make_mock_provider();
    sc_runtime_bundle_t bundle = {0};
    sc_error_t err = sc_runtime_bundle_init(NULL, primary, 0, 0, &bundle);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_init_null_out_returns_invalid_argument(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t primary = make_mock_provider();
    sc_error_t err = sc_runtime_bundle_init(&alloc, primary, 0, 0, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_init_no_wrapping_stores_primary_directly(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t primary = make_mock_provider();
    sc_runtime_bundle_t bundle = {0};

    sc_error_t err = sc_runtime_bundle_init(&alloc, primary, 0, 0, &bundle);
    SC_ASSERT_EQ(err, SC_OK);

    sc_provider_t got = sc_runtime_bundle_provider(&bundle);
    SC_ASSERT_EQ(got.ctx, primary.ctx);
    SC_ASSERT_EQ(got.vtable, primary.vtable);
    SC_ASSERT_STR_EQ(got.vtable->get_name(got.ctx), "mock");

    sc_runtime_bundle_deinit(&bundle, &alloc);
}

static void test_init_with_retries_wraps_with_reliable(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t primary = make_mock_provider();
    sc_runtime_bundle_t bundle = {0};

    sc_error_t err = sc_runtime_bundle_init(&alloc, primary, 2, 50, &bundle);
    SC_ASSERT_EQ(err, SC_OK);

    sc_provider_t got = sc_runtime_bundle_provider(&bundle);
    SC_ASSERT_NOT_NULL(got.ctx);
    SC_ASSERT_NOT_NULL(got.vtable);
    SC_ASSERT_STR_EQ(got.vtable->get_name(got.ctx), "mock");

    sc_runtime_bundle_deinit(&bundle, &alloc);
}

static void test_provider_accessor_returns_bundle_provider(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t primary = make_mock_provider();
    sc_runtime_bundle_t bundle = {0};

    sc_runtime_bundle_init(&alloc, primary, 0, 0, &bundle);
    sc_provider_t got = sc_runtime_bundle_provider(&bundle);
    SC_ASSERT_EQ(got.vtable, &mock_vtable);
    SC_ASSERT_STR_EQ(got.vtable->get_name(got.ctx), "mock");

    sc_runtime_bundle_deinit(&bundle, &alloc);
}

static void test_provider_accessor_null_bundle_returns_zeroed(void) {
    sc_provider_t got = sc_runtime_bundle_provider(NULL);
    SC_ASSERT_NULL(got.ctx);
    SC_ASSERT_NULL(got.vtable);
}

static void test_deinit_null_no_ops(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_runtime_bundle_deinit(NULL, &alloc);
    /* No crash */
}

static void test_deinit_calls_vtable_and_zeroes(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t primary = make_mock_provider();
    sc_runtime_bundle_t bundle = {0};

    sc_runtime_bundle_init(&alloc, primary, 0, 0, &bundle);
    sc_runtime_bundle_deinit(&bundle, &alloc);

    SC_ASSERT_NULL(bundle.provider.ctx);
    SC_ASSERT_NULL(bundle.provider.vtable);
    SC_ASSERT_NULL(bundle.inner_ctx);
    SC_ASSERT_NULL(bundle.inner_vtable);
}

static void test_deinit_wrapped_provider_cleans_up(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t primary = make_mock_provider();
    sc_runtime_bundle_t bundle = {0};

    sc_runtime_bundle_init(&alloc, primary, 1, 50, &bundle);
    sc_runtime_bundle_deinit(&bundle, &alloc);

    SC_ASSERT_NULL(bundle.provider.ctx);
    SC_ASSERT_NULL(bundle.provider.vtable);
}

void run_runtime_bundle_tests(void) {
    SC_TEST_SUITE("runtime_bundle");

    SC_RUN_TEST(test_init_null_alloc_returns_invalid_argument);
    SC_RUN_TEST(test_init_null_out_returns_invalid_argument);
    SC_RUN_TEST(test_init_no_wrapping_stores_primary_directly);
    SC_RUN_TEST(test_init_with_retries_wraps_with_reliable);
    SC_RUN_TEST(test_provider_accessor_returns_bundle_provider);
    SC_RUN_TEST(test_provider_accessor_null_bundle_returns_zeroed);
    SC_RUN_TEST(test_deinit_null_no_ops);
    SC_RUN_TEST(test_deinit_calls_vtable_and_zeroes);
    SC_RUN_TEST(test_deinit_wrapped_provider_cleans_up);
}
