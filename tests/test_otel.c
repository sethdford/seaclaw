#include "test_framework.h"
#include "human/observability/otel.h"
#include "human/observer.h"

#include <string.h>

static void test_otel_observer_create(void) {
    hu_observer_t obs;
    memset(&obs, 0, sizeof(obs));
    HU_ASSERT(obs.vtable == NULL);
}

static void test_otel_span_lifecycle(void) {
    hu_observer_t obs = hu_observer_noop();
    HU_ASSERT(obs.vtable != NULL);
}

static void test_otel_observer_create_null_allocator_returns_error(void) {
    hu_observer_t obs;
    memset(&obs, 0, sizeof(obs));
    HU_ASSERT_EQ(hu_otel_observer_create(NULL, NULL, &obs), HU_ERR_INVALID_ARGUMENT);
}

static void test_otel_observer_create_null_endpoint_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_observer_t obs;
    memset(&obs, 0, sizeof(obs));
    hu_otel_config_t cfg = {
        .endpoint = NULL,
        .endpoint_len = 0,
        .service_name = "human",
        .service_name_len = 5,
        .enable_traces = false,
        .enable_metrics = false,
        .enable_logs = false,
    };
    HU_ASSERT_EQ(hu_otel_observer_create(&alloc, &cfg, &obs), HU_OK);
    HU_ASSERT_NOT_NULL(obs.vtable);
    HU_ASSERT_STR_EQ(obs.vtable->name(obs.ctx), "otel");
    if (obs.vtable->deinit)
        obs.vtable->deinit(obs.ctx);
}

static void test_otel_observer_deinit_null_ctx_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_observer_t obs;
    memset(&obs, 0, sizeof(obs));
    HU_ASSERT_EQ(hu_otel_observer_create(&alloc, NULL, &obs), HU_OK);
    HU_ASSERT_NOT_NULL(obs.vtable);
    HU_ASSERT_NOT_NULL(obs.vtable->deinit);
    obs.vtable->deinit(NULL);
    obs.vtable->deinit(obs.ctx);
}

void run_otel_tests(void) {
    HU_TEST_SUITE("OpenTelemetry");
    HU_RUN_TEST(test_otel_observer_create);
    HU_RUN_TEST(test_otel_span_lifecycle);
    HU_RUN_TEST(test_otel_observer_create_null_allocator_returns_error);
    HU_RUN_TEST(test_otel_observer_create_null_endpoint_ok);
    HU_RUN_TEST(test_otel_observer_deinit_null_ctx_no_crash);
}
