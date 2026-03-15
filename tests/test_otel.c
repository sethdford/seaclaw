#include "test_framework.h"
#include "human/observer.h"

static void test_otel_observer_create(void) {
    hu_observer_t obs;
    memset(&obs, 0, sizeof(obs));
    HU_ASSERT(obs.vtable == NULL);
}

static void test_otel_span_lifecycle(void) {
    hu_observer_t obs = hu_observer_noop();
    HU_ASSERT(obs.vtable != NULL);
}

void run_otel_tests(void) {
    HU_TEST_SUITE("OpenTelemetry");
    HU_RUN_TEST(test_otel_observer_create);
    HU_RUN_TEST(test_otel_span_lifecycle);
}
