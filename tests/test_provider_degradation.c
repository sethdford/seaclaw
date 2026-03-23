#include "test_framework.h"
#include "human/agent/degradation.h"
#include <string.h>

/* Mock provider: succeeds unless model starts with "fail" */
static hu_error_t mock_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *req,
                            const char *model, size_t model_len, double temperature,
                            hu_chat_response_t *out) {
    (void)ctx; (void)req; (void)temperature;
    memset(out, 0, sizeof(*out));
    if (model_len >= 4 && memcmp(model, "fail", 4) == 0)
        return HU_ERR_PROVIDER_UNAVAILABLE;

    const char *resp = "Mock response";
    size_t len = strlen(resp);
    char *content = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (!content) return HU_ERR_OUT_OF_MEMORY;
    memcpy(content, resp, len + 1);
    out->content = content;
    out->content_len = len;
    return HU_OK;
}

static hu_provider_vtable_t mock_vtable = {
    .chat = mock_chat,
};

static void test_degrade_primary_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &mock_vtable};

    hu_provider_degradation_config_t config = {.enabled = true, .max_retries = 1};
    hu_circuit_breaker_init(&config.breaker, 3, 5000);

    hu_chat_request_t req = {0};
    hu_degradation_result_t result;
    hu_error_t err = hu_provider_degrade_chat(&config, &provider, &alloc, &req,
                                              "good-model", 10, 0.7, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.strategy_used, (int)HU_DEGRADE_PRIMARY);
    HU_ASSERT_EQ((int)result.attempts, 1);
    HU_ASSERT_NOT_NULL(result.response.content);
    HU_ASSERT_STR_EQ(result.response.content, "Mock response");
    alloc.free(alloc.ctx, (void *)result.response.content, result.response.content_len + 1);
}

static void test_degrade_fallback_on_primary_fail(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &mock_vtable};

    char fallback[] = "good-fallback";
    hu_provider_degradation_config_t config = {
        .enabled = true, .max_retries = 1,
        .fallback_model = fallback, .fallback_model_len = strlen(fallback),
    };
    hu_circuit_breaker_init(&config.breaker, 3, 5000);

    hu_chat_request_t req = {0};
    hu_degradation_result_t result;
    hu_error_t err = hu_provider_degrade_chat(&config, &provider, &alloc, &req,
                                              "fail-model", 10, 0.7, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.strategy_used, (int)HU_DEGRADE_FALLBACK);
    HU_ASSERT_GT((int)result.attempts, 1);
    HU_ASSERT_NOT_NULL(result.response.content);
    alloc.free(alloc.ctx, (void *)result.response.content, result.response.content_len + 1);
}

static void test_degrade_honest_failure(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &mock_vtable};

    char fallback[] = "fail-fallback";
    hu_provider_degradation_config_t config = {
        .enabled = true, .max_retries = 1,
        .fallback_model = fallback, .fallback_model_len = strlen(fallback),
    };
    hu_circuit_breaker_init(&config.breaker, 3, 5000);

    hu_chat_request_t req = {0};
    hu_degradation_result_t result;
    hu_error_t err = hu_provider_degrade_chat(&config, &provider, &alloc, &req,
                                              "fail-model", 10, 0.7, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.strategy_used, (int)HU_DEGRADE_HONEST_FAILURE);
    HU_ASSERT_NOT_NULL(result.response.content);
    HU_ASSERT_STR_CONTAINS(result.response.content, "trouble connecting");
    alloc.free(alloc.ctx, (void *)result.response.content, result.response.content_len + 1);
}

static void test_degrade_circuit_breaker_trips(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &mock_vtable};

    hu_provider_degradation_config_t config = {
        .enabled = true, .max_retries = 1,
    };
    hu_circuit_breaker_init(&config.breaker, 2, 60000);

    hu_chat_request_t req = {0};
    hu_degradation_result_t result;

    /* Trip the breaker with failures */
    for (int i = 0; i < 3; i++) {
        memset(&result, 0, sizeof(result));
        hu_provider_degrade_chat(&config, &provider, &alloc, &req,
                                 "fail-model", 10, 0.7, &result);
        if (result.response.content)
            alloc.free(alloc.ctx, (void *)result.response.content, result.response.content_len + 1);
    }

    /* Now breaker should be open — immediate honest failure, 0 attempts */
    memset(&result, 0, sizeof(result));
    hu_error_t err = hu_provider_degrade_chat(&config, &provider, &alloc, &req,
                                              "good-model", 10, 0.7, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.strategy_used, (int)HU_DEGRADE_HONEST_FAILURE);
    HU_ASSERT_EQ((int)result.attempts, 0);
    HU_ASSERT_NOT_NULL(result.response.content);
    alloc.free(alloc.ctx, (void *)result.response.content, result.response.content_len + 1);
}

static void test_degrade_disabled_passthrough(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &mock_vtable};

    hu_provider_degradation_config_t config = {.enabled = false};

    hu_chat_request_t req = {0};
    hu_degradation_result_t result;
    hu_error_t err = hu_provider_degrade_chat(&config, &provider, &alloc, &req,
                                              "good-model", 10, 0.7, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.strategy_used, (int)HU_DEGRADE_PRIMARY);
    HU_ASSERT_NOT_NULL(result.response.content);
    alloc.free(alloc.ctx, (void *)result.response.content, result.response.content_len + 1);
}

static void test_degrade_null_config_passthrough(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &mock_vtable};

    hu_chat_request_t req = {0};
    hu_degradation_result_t result;
    hu_error_t err = hu_provider_degrade_chat(NULL, &provider, &alloc, &req,
                                              "good-model", 10, 0.7, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ((int)result.strategy_used, (int)HU_DEGRADE_PRIMARY);
    alloc.free(alloc.ctx, (void *)result.response.content, result.response.content_len + 1);
}

static void test_degrade_null_args(void) {
    hu_degradation_result_t result;
    HU_ASSERT_EQ(hu_provider_degrade_chat(NULL, NULL, NULL, NULL, NULL, 0, 0.0, &result),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_degrade_honest_failure_msg(void) {
    hu_allocator_t alloc = hu_system_allocator();
    size_t len = 0;
    char *msg = hu_degradation_honest_failure_msg(&alloc, &len);
    HU_ASSERT_NOT_NULL(msg);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_CONTAINS(msg, "trouble connecting");
    alloc.free(alloc.ctx, msg, len + 1);
}

void run_provider_degradation_tests(void) {
    HU_TEST_SUITE("Provider Degradation");
    HU_RUN_TEST(test_degrade_primary_succeeds);
    HU_RUN_TEST(test_degrade_fallback_on_primary_fail);
    HU_RUN_TEST(test_degrade_honest_failure);
    HU_RUN_TEST(test_degrade_circuit_breaker_trips);
    HU_RUN_TEST(test_degrade_disabled_passthrough);
    HU_RUN_TEST(test_degrade_null_config_passthrough);
    HU_RUN_TEST(test_degrade_null_args);
    HU_RUN_TEST(test_degrade_honest_failure_msg);
}
