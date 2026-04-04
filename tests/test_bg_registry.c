#include "human/background_observer.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void bg_registry_init_and_count(void) {
    hu_bg_registry_t r;
    hu_bg_registry_init(&r);
    HU_ASSERT_EQ(hu_bg_registry_count(&r), 0);
}

static void bg_registry_register_and_count(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_bg_registry_t r;
    hu_bg_registry_init(&r);
    hu_bg_observer_t obs;
    HU_ASSERT_EQ(hu_bg_health_monitor_create(&a, &obs), HU_OK);
    HU_ASSERT_EQ(hu_bg_registry_register(&r, obs), HU_OK);
    HU_ASSERT_EQ(hu_bg_registry_count(&r), 1);
    hu_bg_registry_deinit(&r, &a);
}

static void bg_registry_register_null(void) {
    hu_bg_observer_t obs = {0};
    HU_ASSERT_EQ(hu_bg_registry_register(NULL, obs), HU_ERR_INVALID_ARGUMENT);
}

static void bg_registry_tick_all_runs(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_bg_registry_t r;
    hu_bg_registry_init(&r);
    hu_bg_observer_t obs;
    HU_ASSERT_EQ(hu_bg_health_monitor_create(&a, &obs), HU_OK);
    HU_ASSERT_EQ(hu_bg_registry_register(&r, obs), HU_OK);
    hu_bg_registry_tick_all(&r, &a, NULL);
    hu_bg_registry_deinit(&r, &a);
}

void run_bg_registry_tests(void) {
    HU_TEST_SUITE("BackgroundRegistry");
    HU_RUN_TEST(bg_registry_init_and_count);
    HU_RUN_TEST(bg_registry_register_and_count);
    HU_RUN_TEST(bg_registry_register_null);
    HU_RUN_TEST(bg_registry_tick_all_runs);
}
