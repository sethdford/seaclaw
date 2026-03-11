/*
 * F51: Weather proactive awareness — weather context for proactive messages.
 */
#include "human/agent/weather_awareness.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

#define HU_WEATHER_CONDITION_CAP 63
#define HU_WEATHER_ALERT_CAP     127

static bool is_precipitation(const char *condition) {
    if (!condition || condition[0] == '\0')
        return false;
    char lower[64];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && condition[i] != '\0'; i++) {
        char c = condition[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    lower[i] = '\0';
    return strstr(lower, "rain") != NULL || strstr(lower, "snow") != NULL ||
           strstr(lower, "drizzle") != NULL;
}

hu_error_t hu_weather_awareness_build_directive(hu_allocator_t *alloc,
                                      const hu_weather_context_t *weather,
                                      uint8_t hour_local, char **out, size_t *out_len) {
    (void)hour_local;
    if (!alloc || !weather || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    char buf[384];
    const char *cond = weather->condition[0] != '\0' ? weather->condition : "unknown";
    int n;
    if (weather->has_alert && weather->alert_text[0] != '\0') {
        n = snprintf(buf, sizeof(buf),
                     "[WEATHER: %s. Currently %d°C (feels like %d°C). Consider mentioning "
                     "weather if relevant to plans.]",
                     weather->alert_text, weather->temp_celsius, weather->feels_like_celsius);
    } else {
        n = snprintf(buf, sizeof(buf),
                     "[WEATHER: Currently %d°C and %s. Consider mentioning weather if relevant "
                     "to plans.]",
                     weather->temp_celsius, cond);
    }
    if (n <= 0 || (size_t)n >= sizeof(buf))
        return HU_ERR_INVALID_ARGUMENT;
    size_t len = (size_t)n;

    *out = hu_strndup(alloc, buf, len);
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    *out_len = len;
    return HU_OK;
}

bool hu_weather_awareness_should_mention(const hu_weather_context_t *weather,
                                         uint8_t hour_local) {
    if (!weather)
        return false;

    /* Severe weather alert always warrants mention */
    if (weather->has_alert && weather->alert_text[0] != '\0')
        return true;

    /* Extreme temperatures */
    if (weather->temp_celsius < 0 || weather->temp_celsius > 35)
        return true;

    /* Rain/snow during morning hours (6–10 AM) when outdoor plans are likely */
    if (hour_local >= 6 && hour_local <= 10 && is_precipitation(weather->condition))
        return true;

    return false;
}

hu_error_t hu_weather_awareness_parse_json(const char *json, size_t json_len,
                                            hu_weather_context_t *out) {
    if (!json || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *root = NULL;
    hu_error_t err = hu_json_parse(&alloc, json, json_len, &root);
    if (err != HU_OK || !root)
        return err == HU_OK ? HU_ERR_PARSE : err;

    /* Prefer flat keys, then OpenWeatherMap nested structure */
    const char *cond = hu_json_get_string(root, "condition");
    if (!cond) {
        hu_json_value_t *weather_arr = hu_json_object_get(root, "weather");
        if (weather_arr && weather_arr->type == HU_JSON_ARRAY &&
            weather_arr->data.array.len > 0) {
            hu_json_value_t *w0 = weather_arr->data.array.items[0];
            cond = hu_json_get_string(w0, "main");
            if (!cond)
                cond = hu_json_get_string(w0, "description");
        }
    }
    if (cond) {
        size_t clen = strlen(cond);
        if (clen > HU_WEATHER_CONDITION_CAP)
            clen = HU_WEATHER_CONDITION_CAP;
        memcpy(out->condition, cond, clen);
        out->condition[clen] = '\0';
    }

    double temp = hu_json_get_number(root, "temp", -999.0);
    if (temp == -999.0) {
        hu_json_value_t *main_obj = hu_json_object_get(root, "main");
        if (main_obj)
            temp = hu_json_get_number(main_obj, "temp", -999.0);
    }
    if (temp != -999.0) {
        /* OpenWeatherMap uses Kelvin; if > 100 assume Kelvin */
        if (temp > 100.0)
            temp = temp - 273.15;
        out->temp_celsius = (int)(temp + (temp >= 0 ? 0.5 : -0.5));
    }

    double feels = hu_json_get_number(root, "feels_like", -999.0);
    if (feels == -999.0) {
        hu_json_value_t *main_obj = hu_json_object_get(root, "main");
        if (main_obj)
            feels = hu_json_get_number(main_obj, "feels_like", -999.0);
    }
    if (feels != -999.0) {
        if (feels > 100.0)
            feels = feels - 273.15;
        out->feels_like_celsius = (int)(feels + (feels >= 0 ? 0.5 : -0.5));
    } else {
        out->feels_like_celsius = out->temp_celsius;
    }

    double hum = hu_json_get_number(root, "humidity", -1.0);
    if (hum < 0.0) {
        hu_json_value_t *main_obj = hu_json_object_get(root, "main");
        if (main_obj)
            hum = hu_json_get_number(main_obj, "humidity", -1.0);
    }
    if (hum >= 0.0 && hum <= 100.0)
        out->humidity_pct = (uint8_t)(hum + 0.5);

    const char *alert = hu_json_get_string(root, "alert");
    if (!alert) {
        hu_json_value_t *alerts_arr = hu_json_object_get(root, "alerts");
        if (alerts_arr && alerts_arr->type == HU_JSON_ARRAY &&
            alerts_arr->data.array.len > 0) {
            hu_json_value_t *a0 = alerts_arr->data.array.items[0];
            alert = hu_json_get_string(a0, "event");
            if (!alert)
                alert = hu_json_get_string(a0, "description");
        }
    }
    if (alert && alert[0] != '\0') {
        out->has_alert = true;
        size_t alen = strlen(alert);
        if (alen > HU_WEATHER_ALERT_CAP)
            alen = HU_WEATHER_ALERT_CAP;
        memcpy(out->alert_text, alert, alen);
        out->alert_text[alen] = '\0';
    }

    hu_json_free(&alloc, root);
    return HU_OK;
}
