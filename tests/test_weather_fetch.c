#include "human/agent/weather_fetch.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void weather_fetch_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_EQ(hu_weather_fetch(NULL, "NYC", 3, NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    hu_weather_context_t out = {0};
    HU_ASSERT_EQ(hu_weather_fetch(&alloc, NULL, 0, NULL, &out), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_weather_fetch(&alloc, "", 0, NULL, &out), HU_ERR_INVALID_ARGUMENT);
}

static void weather_fetch_test_mock_returns_sunny(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weather_context_t out = {0};
    hu_error_t err = hu_weather_fetch(&alloc, "New York", 8, NULL, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out.condition, "Sunny");
    HU_ASSERT_EQ(out.temp_celsius, 22);
    HU_ASSERT_EQ(out.humidity_pct, 45);
    HU_ASSERT_TRUE(!out.has_alert);
}

static void weather_cache_ttl_is_30_minutes(void) {
    HU_ASSERT_EQ(HU_WEATHER_CACHE_TTL_SEC, 1800);
}

int run_weather_fetch_tests(void) {
    HU_TEST_SUITE("weather_fetch");
    HU_RUN_TEST(weather_fetch_null_args_returns_error);
    HU_RUN_TEST(weather_fetch_test_mock_returns_sunny);
    HU_RUN_TEST(weather_cache_ttl_is_30_minutes);
    return hu__failed;
}
