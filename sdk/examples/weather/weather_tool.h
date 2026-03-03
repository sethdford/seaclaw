#ifndef SC_WEATHER_TOOL_H
#define SC_WEATHER_TOOL_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/tool.h"
#include <stddef.h>

/**
 * Example: Weather tool — returns current conditions for a city.
 *
 * In SC_IS_TEST returns stub data. In production, could call
 * OpenWeatherMap or similar API (add API key handling).
 */
sc_error_t sc_weather_create(sc_allocator_t *alloc, sc_tool_t *out);

#endif /* SC_WEATHER_TOOL_H */
