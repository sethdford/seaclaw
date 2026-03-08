#ifndef SC_CONFIG_PARSE_INTERNAL_H
#define SC_CONFIG_PARSE_INTERNAL_H

#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"

/* Shared helper: parse JSON array of strings into allocated array. */
sc_error_t parse_string_array(sc_allocator_t *a, char ***out, size_t *out_len,
                              const sc_json_value_t *arr);

/* Provider-related parsers */
sc_error_t parse_providers(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *arr);
sc_error_t parse_router(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj);

/* Channel-related parsers */
sc_error_t parse_channels(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj);

/* Agent-related parsers */
sc_error_t parse_agent(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj);

#endif /* SC_CONFIG_PARSE_INTERNAL_H */
