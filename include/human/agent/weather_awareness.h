#ifndef HU_WEATHER_AWARENESS_H
#define HU_WEATHER_AWARENESS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_weather_context {
    char condition[64]; /* "sunny", "rainy", "snowy", "cloudy", "stormy", etc. */
    int temp_celsius;
    int feels_like_celsius;
    uint8_t humidity_pct;
    bool has_alert; /* severe weather alert */
    char alert_text[128];
} hu_weather_context_t;

/* Build a weather-aware proactive directive for prompt injection.
 * Returns HU_OK and writes directive into out and out_len.
 * Caller must free *out via alloc. */
hu_error_t hu_weather_awareness_build_directive(hu_allocator_t *alloc,
                                               const hu_weather_context_t *weather,
                                               uint8_t hour_local, char **out, size_t *out_len);

/* Check if weather conditions warrant a proactive mention.
 * Returns true for: severe alerts, extreme temp (<0C or >35C),
 * rain/snow when outdoor plans are likely (morning hours). */
bool hu_weather_awareness_should_mention(const hu_weather_context_t *weather,
                                        uint8_t hour_local);

/* Parse a JSON weather response (from OpenWeatherMap or similar) into hu_weather_context_t.
 * Tolerant of missing fields. Returns HU_OK on success. */
hu_error_t hu_weather_awareness_parse_json(const char *json, size_t json_len,
                                           hu_weather_context_t *out);

#endif /* HU_WEATHER_AWARENESS_H */
