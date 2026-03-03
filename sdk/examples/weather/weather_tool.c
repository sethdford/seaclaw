/*
 * Example: Weather tool
 *
 * Full working tool with JSON params. Shows:
 * - parameters_json schema
 * - sc_json_get_string / sc_json_get_number for args
 * - sc_tool_result_ok_owned for allocated output
 * - SC_IS_TEST guard for deterministic tests
 */
#include "weather_tool.h"
#include "seaclaw/tool.h"
#include "seaclaw/core/json.h"
#include "seaclaw/core/string.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SC_WEATHER_NAME "weather"
#define SC_WEATHER_DESC "Get current weather for a city. Returns temperature, conditions, and humidity."
#define SC_WEATHER_PARAMS "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\",\"description\":\"City name\"},\"units\":{\"type\":\"string\",\"enum\":[\"celsius\",\"fahrenheit\"],\"default\":\"celsius\"}},\"required\":[\"city\"]}"

typedef struct sc_weather_ctx {
    int _reserved; /* placeholder; could hold API key, cache, etc. */
} sc_weather_ctx_t;

static sc_error_t weather_execute(void *ctx, sc_allocator_t *alloc,
    const sc_json_value_t *args,
    sc_tool_result_t *out)
{
    (void)ctx;
    if (!args || !out) {
        *out = sc_tool_result_fail("invalid args", 12);
        return SC_ERR_INVALID_ARGUMENT;
    }

    const char *city = sc_json_get_string(args, "city");
    if (!city || strlen(city) == 0) {
        *out = sc_tool_result_fail("Missing required 'city' parameter", 31);
        return SC_OK;
    }

    const char *units = sc_json_get_string(args, "units");
    if (!units) units = "celsius";
    int use_fahrenheit = (strcmp(units, "fahrenheit") == 0);

#if SC_IS_TEST
    char *msg = sc_sprintf(alloc, "Weather in %s: 20°C, partly cloudy (stub)", city);
    if (!msg) {
        *out = sc_tool_result_fail("out of memory", 12);
        return SC_ERR_OUT_OF_MEMORY;
    }
    *out = sc_tool_result_ok_owned(msg, strlen(msg));
    return SC_OK;
#else
    /* Production: call OpenWeatherMap or similar. For demo, return mock. */
    char buf[256];
    int n;
    if (use_fahrenheit) {
        n = snprintf(buf, sizeof(buf),
            "Weather in %s: 68°F, partly cloudy. Humidity: 55%%. (mock - add API)", city);
    } else {
        n = snprintf(buf, sizeof(buf),
            "Weather in %s: 20°C, partly cloudy. Humidity: 55%%. (mock - add API)", city);
    }
    if (n < 0 || (size_t)n >= sizeof(buf)) {
        *out = sc_tool_result_fail("format error", 12);
        return SC_OK;
    }
    char *msg = sc_strndup(alloc, buf, (size_t)n);
    if (!msg) {
        *out = sc_tool_result_fail("out of memory", 12);
        return SC_ERR_OUT_OF_MEMORY;
    }
    *out = sc_tool_result_ok_owned(msg, (size_t)n);
    return SC_OK;
#endif
}

static const char *weather_name(void *ctx) {
    (void)ctx;
    return SC_WEATHER_NAME;
}

static const char *weather_description(void *ctx) {
    (void)ctx;
    return SC_WEATHER_DESC;
}

static const char *weather_parameters_json(void *ctx) {
    (void)ctx;
    return SC_WEATHER_PARAMS;
}

static void weather_deinit(void *ctx, sc_allocator_t *alloc) {
    (void)alloc;
    if (ctx) free(ctx);
}

static const sc_tool_vtable_t weather_vtable = {
    .execute = weather_execute,
    .name = weather_name,
    .description = weather_description,
    .parameters_json = weather_parameters_json,
    .deinit = weather_deinit,
};

sc_error_t sc_weather_create(sc_allocator_t *alloc, sc_tool_t *out) {
    (void)alloc;
    if (!out) return SC_ERR_INVALID_ARGUMENT;

    sc_weather_ctx_t *ctx = (sc_weather_ctx_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return SC_ERR_OUT_OF_MEMORY;

    out->ctx = ctx;
    out->vtable = &weather_vtable;
    return SC_OK;
}
