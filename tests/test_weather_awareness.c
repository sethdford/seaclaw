#include "human/agent/weather_awareness.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

static void weather_build_directive_rainy_morning(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "rainy", 6);
    weather.temp_celsius = 5;
    weather.feels_like_celsius = 3;
    weather.humidity_pct = 85;

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_weather_awareness_build_directive(&alloc, &weather, 8, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT_TRUE(strstr(out, "5") != NULL);
    HU_ASSERT_TRUE(strstr(out, "rainy") != NULL);
    HU_ASSERT_TRUE(strstr(out, "WEATHER") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
}

static void weather_build_directive_extreme_cold(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "snowy", 6);
    weather.temp_celsius = -12;
    weather.feels_like_celsius = -15;

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_weather_awareness_build_directive(&alloc, &weather, 14, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "-12") != NULL || strstr(out, "12") != NULL);
    HU_ASSERT_TRUE(strstr(out, "snowy") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
}

static void weather_build_directive_sunny_calm(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "sunny", 6);
    weather.temp_celsius = 22;
    weather.feels_like_celsius = 21;

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_weather_awareness_build_directive(&alloc, &weather, 15, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "22") != NULL);
    HU_ASSERT_TRUE(strstr(out, "sunny") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
}

static void weather_should_mention_severe_alert(void) {
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "sunny", 6);
    weather.temp_celsius = 20;
    weather.has_alert = true;
    memcpy(weather.alert_text, "Severe thunderstorm warning", 27);

    HU_ASSERT_TRUE(hu_weather_awareness_should_mention(&weather, 14));
}

static void weather_should_mention_normal_conditions(void) {
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "sunny", 6);
    weather.temp_celsius = 22;
    weather.feels_like_celsius = 21;

    /* Sunny afternoon — no reason to mention */
    HU_ASSERT_FALSE(hu_weather_awareness_should_mention(&weather, 14));
}

static void weather_should_mention_extreme_cold(void) {
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "cold", 5);
    weather.temp_celsius = -5;

    HU_ASSERT_TRUE(hu_weather_awareness_should_mention(&weather, 14));
}

static void weather_should_mention_extreme_heat(void) {
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "sunny", 6);
    weather.temp_celsius = 38;

    HU_ASSERT_TRUE(hu_weather_awareness_should_mention(&weather, 14));
}

static void weather_should_mention_rain_morning(void) {
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "rainy", 6);
    weather.temp_celsius = 15;

    /* Rain at 8 AM — outdoor plans likely */
    HU_ASSERT_TRUE(hu_weather_awareness_should_mention(&weather, 8));
}

static void weather_should_mention_rain_afternoon(void) {
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "rainy", 6);
    weather.temp_celsius = 15;

    /* Rain at 2 PM — not in morning window */
    HU_ASSERT_FALSE(hu_weather_awareness_should_mention(&weather, 14));
}

static void weather_parse_json_complete(void) {
    const char *json =
        "{\"condition\":\"rainy\",\"temp\":5,\"feels_like\":3,\"humidity\":80,\"alert\":"
        "\"Severe storm\"}";
    hu_weather_context_t out;
    memset(&out, 0, sizeof(out));

    HU_ASSERT_EQ(hu_weather_awareness_parse_json(json, strlen(json), &out), HU_OK);
    HU_ASSERT_STR_EQ(out.condition, "rainy");
    HU_ASSERT_EQ(out.temp_celsius, 5);
    HU_ASSERT_EQ(out.feels_like_celsius, 3);
    HU_ASSERT_EQ(out.humidity_pct, 80);
    HU_ASSERT_TRUE(out.has_alert);
    HU_ASSERT_STR_EQ(out.alert_text, "Severe storm");
}

static void weather_parse_json_partial(void) {
    const char *json = "{\"condition\":\"cloudy\",\"temp\":18}";
    hu_weather_context_t out;
    memset(&out, 0, sizeof(out));

    HU_ASSERT_EQ(hu_weather_awareness_parse_json(json, strlen(json), &out), HU_OK);
    HU_ASSERT_STR_EQ(out.condition, "cloudy");
    HU_ASSERT_EQ(out.temp_celsius, 18);
    /* feels_like defaults to temp when missing */
    HU_ASSERT_EQ(out.feels_like_celsius, 18);
    /* humidity and alert absent — tolerant */
}

static void weather_parse_json_openweathermap(void) {
    const char *json =
        "{\"main\":{\"temp\":278.15,\"feels_like\":276.0,\"humidity\":75},"
        "\"weather\":[{\"main\":\"Rain\",\"description\":\"light rain\"}]}";
    hu_weather_context_t out;
    memset(&out, 0, sizeof(out));

    HU_ASSERT_EQ(hu_weather_awareness_parse_json(json, strlen(json), &out), HU_OK);
    HU_ASSERT_STR_EQ(out.condition, "Rain");
    HU_ASSERT_EQ(out.temp_celsius, 5);   /* 278.15 - 273.15 ≈ 5 */
    HU_ASSERT_EQ(out.feels_like_celsius, 3); /* 276 - 273 ≈ 3 */
    HU_ASSERT_EQ(out.humidity_pct, 75);
}

static void weather_build_directive_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_weather_context_t weather;
    memset(&weather, 0, sizeof(weather));
    memcpy(weather.condition, "sunny", 6);
    weather.temp_celsius = 20;

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_weather_awareness_build_directive(NULL, &weather, 10, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_weather_awareness_build_directive(&alloc, NULL, 10, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_weather_awareness_build_directive(&alloc, &weather, 10, NULL, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_weather_awareness_build_directive(&alloc, &weather, 10, &out, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void weather_parse_json_null_args(void) {
    hu_weather_context_t out;
    memset(&out, 0, sizeof(out));

    HU_ASSERT_EQ(hu_weather_awareness_parse_json(NULL, 0, &out), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_weather_awareness_parse_json("{}", 2, NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_weather_awareness_tests(void) {
    HU_TEST_SUITE("weather_awareness");
    HU_RUN_TEST(weather_build_directive_rainy_morning);
    HU_RUN_TEST(weather_build_directive_extreme_cold);
    HU_RUN_TEST(weather_build_directive_sunny_calm);
    HU_RUN_TEST(weather_should_mention_severe_alert);
    HU_RUN_TEST(weather_should_mention_normal_conditions);
    HU_RUN_TEST(weather_should_mention_extreme_cold);
    HU_RUN_TEST(weather_should_mention_extreme_heat);
    HU_RUN_TEST(weather_should_mention_rain_morning);
    HU_RUN_TEST(weather_should_mention_rain_afternoon);
    HU_RUN_TEST(weather_parse_json_complete);
    HU_RUN_TEST(weather_parse_json_partial);
    HU_RUN_TEST(weather_parse_json_openweathermap);
    HU_RUN_TEST(weather_build_directive_null_args);
    HU_RUN_TEST(weather_parse_json_null_args);
}
