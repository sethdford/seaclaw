#include "human/agent/chaos.h"
#include "test_framework.h"
#include <string.h>

static void test_chaos_engine_init(void) {
    hu_chaos_engine_t e;
    hu_chaos_config_t cfg = HU_CHAOS_CONFIG_DEFAULT;
    hu_chaos_engine_init(&e, &cfg);
    HU_ASSERT_EQ(e.stats.total_calls, 0);
    HU_ASSERT_EQ(e.stats.faults_injected, 0);
    HU_ASSERT_FALSE(e.config.enabled);
    HU_ASSERT_EQ(e.prng_state, 42);
}

static void test_chaos_disabled_no_faults(void) {
    hu_chaos_engine_t e;
    hu_chaos_config_t cfg = HU_CHAOS_CONFIG_DEFAULT;
    cfg.enabled = false;
    hu_chaos_engine_init(&e, &cfg);

    for (int i = 0; i < 100; i++) {
        hu_chaos_fault_t f = hu_chaos_maybe_inject(&e);
        HU_ASSERT_EQ(f, HU_CHAOS_NONE);
    }
    HU_ASSERT_EQ(e.stats.faults_injected, 0);
}

static void test_chaos_forced_fault(void) {
    hu_chaos_engine_t e;
    hu_chaos_config_t cfg = {
        .enabled = true,
        .fault_probability = 1.0,
        .forced_fault = HU_CHAOS_TIMEOUT,
        .seed = 42,
    };
    hu_chaos_engine_init(&e, &cfg);

    hu_chaos_fault_t f = hu_chaos_maybe_inject(&e);
    HU_ASSERT_EQ(f, HU_CHAOS_TIMEOUT);
    HU_ASSERT_EQ(e.stats.faults_injected, 1);
    HU_ASSERT_EQ(e.stats.total_calls, 1);
}

static void test_chaos_random_faults(void) {
    hu_chaos_engine_t e;
    hu_chaos_config_t cfg = {
        .enabled = true,
        .fault_probability = 1.0,
        .forced_fault = HU_CHAOS_NONE,
        .seed = 123,
    };
    hu_chaos_engine_init(&e, &cfg);

    bool seen[HU_CHAOS_FAULT_COUNT];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < 100; i++) {
        hu_chaos_fault_t f = hu_chaos_maybe_inject(&e);
        HU_ASSERT_TRUE(f >= 1 && f < HU_CHAOS_FAULT_COUNT);
        seen[f] = true;
    }
    HU_ASSERT_EQ(e.stats.faults_injected, 100);
}

static void test_chaos_apply_timeout(void) {
    hu_chaos_engine_t e;
    hu_chaos_engine_init(&e, NULL);

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err = hu_chaos_apply_to_response(&e, NULL, HU_CHAOS_TIMEOUT, &resp);
    HU_ASSERT_EQ(err, HU_ERR_TIMEOUT);
}

static void test_chaos_apply_empty(void) {
    hu_chaos_engine_t e;
    hu_chaos_engine_init(&e, NULL);

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.content = "hello";
    resp.content_len = 5;
    hu_error_t err = hu_chaos_apply_to_response(&e, NULL, HU_CHAOS_EMPTY_RESPONSE, &resp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NULL(resp.content);
    HU_ASSERT_EQ(resp.content_len, 0);
}

static void test_chaos_apply_partial(void) {
    hu_chaos_engine_t e;
    hu_chaos_engine_init(&e, NULL);

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.content = "hello world";
    resp.content_len = 11;
    hu_error_t err = hu_chaos_apply_to_response(&e, NULL, HU_CHAOS_PARTIAL_RESPONSE, &resp);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(resp.content_len, 5);
}

static void test_chaos_apply_error_code(void) {
    hu_chaos_engine_t e;
    hu_chaos_engine_init(&e, NULL);

    hu_chat_response_t resp;
    memset(&resp, 0, sizeof(resp));
    hu_error_t err = hu_chaos_apply_to_response(&e, NULL, HU_CHAOS_ERROR_CODE, &resp);
    HU_ASSERT_EQ(err, HU_ERR_PROVIDER_RESPONSE);
}

static void test_chaos_tool_result_timeout(void) {
    hu_chaos_engine_t e;
    hu_chaos_engine_init(&e, NULL);

    hu_tool_result_t result = {.success = true, .output = "ok", .output_len = 2};
    hu_error_t err = hu_chaos_apply_to_tool_result(&e, NULL, HU_CHAOS_TIMEOUT, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT_STR_CONTAINS(result.output, "timeout");
}

static void test_chaos_tool_result_error(void) {
    hu_chaos_engine_t e;
    hu_chaos_engine_init(&e, NULL);

    hu_tool_result_t result = {.success = true, .output = "ok", .output_len = 2};
    hu_error_t err = hu_chaos_apply_to_tool_result(&e, NULL, HU_CHAOS_ERROR_CODE, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
}

static void test_chaos_report(void) {
    hu_chaos_engine_t e;
    hu_chaos_config_t cfg = {
        .enabled = true,
        .fault_probability = 1.0,
        .forced_fault = HU_CHAOS_TIMEOUT,
        .seed = 42,
    };
    hu_chaos_engine_init(&e, &cfg);

    hu_chaos_maybe_inject(&e);
    hu_chaos_maybe_inject(&e);

    char buf[512];
    size_t len = hu_chaos_report(&e, buf, sizeof(buf));
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_STR_CONTAINS(buf, "2 calls");
    HU_ASSERT_STR_CONTAINS(buf, "2 faults");
}

static void test_chaos_fault_names(void) {
    HU_ASSERT_STR_EQ(hu_chaos_fault_name(HU_CHAOS_NONE), "none");
    HU_ASSERT_STR_EQ(hu_chaos_fault_name(HU_CHAOS_TIMEOUT), "timeout");
    HU_ASSERT_STR_EQ(hu_chaos_fault_name(HU_CHAOS_EMPTY_RESPONSE), "empty_response");
    HU_ASSERT_STR_EQ(hu_chaos_fault_name(HU_CHAOS_GARBAGE_RESPONSE), "garbage_response");
    HU_ASSERT_STR_EQ(hu_chaos_fault_name(HU_CHAOS_ERROR_CODE), "error_code");
    HU_ASSERT_STR_EQ(hu_chaos_fault_name(HU_CHAOS_CORRUPT_JSON), "corrupt_json");
}

static void test_chaos_null_args(void) {
    hu_chaos_engine_init(NULL, NULL);
    HU_ASSERT_EQ(hu_chaos_maybe_inject(NULL), HU_CHAOS_NONE);
    HU_ASSERT_EQ(hu_chaos_apply_to_response(NULL, NULL, HU_CHAOS_NONE, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_chaos_apply_to_tool_result(NULL, NULL, HU_CHAOS_NONE, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_chaos_report(NULL, NULL, 0), 0);
}

void run_chaos_tests(void) {
    HU_TEST_SUITE("Chaos Testing");
    HU_RUN_TEST(test_chaos_engine_init);
    HU_RUN_TEST(test_chaos_disabled_no_faults);
    HU_RUN_TEST(test_chaos_forced_fault);
    HU_RUN_TEST(test_chaos_random_faults);
    HU_RUN_TEST(test_chaos_apply_timeout);
    HU_RUN_TEST(test_chaos_apply_empty);
    HU_RUN_TEST(test_chaos_apply_partial);
    HU_RUN_TEST(test_chaos_apply_error_code);
    HU_RUN_TEST(test_chaos_tool_result_timeout);
    HU_RUN_TEST(test_chaos_tool_result_error);
    HU_RUN_TEST(test_chaos_report);
    HU_RUN_TEST(test_chaos_fault_names);
    HU_RUN_TEST(test_chaos_null_args);
}
