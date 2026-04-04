#include "human/background_observer.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void background_registry_init_zero(void) {
    hu_bg_registry_t r;
    hu_bg_registry_init(&r);
    HU_ASSERT_EQ(hu_bg_registry_count(&r), 0);
    HU_ASSERT_NULL(hu_bg_registry_get(&r, 0));
}

static void background_registry_register_overflow(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_bg_registry_t r;
    hu_bg_registry_init(&r);
    for (size_t i = 0; i < HU_BG_OBSERVER_MAX; i++) {
        hu_bg_observer_t obs;
        HU_ASSERT_EQ(hu_bg_health_monitor_create(&a, &obs), HU_OK);
        HU_ASSERT_EQ(hu_bg_registry_register(&r, obs), HU_OK);
    }
    hu_bg_observer_t extra;
    HU_ASSERT_EQ(hu_bg_health_monitor_create(&a, &extra), HU_OK);
    HU_ASSERT_EQ(hu_bg_registry_register(&r, extra), HU_ERR_INVALID_ARGUMENT);
    extra.vtable->deinit(extra.ctx, &a);
    hu_bg_registry_deinit(&r, &a);
}

static void background_registry_deinit_null(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_bg_registry_deinit(NULL, &a);
}

void run_background_registry_tests(void) {
    HU_TEST_SUITE("BackgroundRegistry2");
    HU_RUN_TEST(background_registry_init_zero);
    HU_RUN_TEST(background_registry_register_overflow);
    HU_RUN_TEST(background_registry_deinit_null);
}
