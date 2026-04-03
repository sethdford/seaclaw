#ifndef HU_CONFIG_PARSE_INTERNAL_H
#define HU_CONFIG_PARSE_INTERNAL_H

#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include <stddef.h>

/* Field type enum for schema-driven parsing */
typedef enum {
    HU_CFG_STRING, /* char* field — freed and replaced with hu_strdup */
    HU_CFG_BOOL,   /* bool field */
    HU_CFG_UINT32, /* uint32_t field with optional min/max validation */
    HU_CFG_INT,    /* int field with optional min/max validation */
    HU_CFG_UINT16, /* uint16_t field with optional min/max validation */
    HU_CFG_UINT64, /* uint64_t field with optional min/max validation */
    HU_CFG_DOUBLE, /* double field with optional min/max validation */
} hu_config_field_type_t;

/* Schema entry for table-driven parsing */
typedef struct hu_config_field {
    const char *key;             /* JSON field name */
    hu_config_field_type_t type; /* Field type */
    size_t offset;               /* offsetof(target_struct, field) */
    double min_val, max_val;     /* For numeric validation (ignored for strings/bools) */
} hu_config_field_t;

/* Generic schema-driven parser: parses fields from JSON object into target struct */
hu_error_t hu_config_parse_fields(hu_allocator_t *alloc, void *target,
                                  const hu_config_field_t *schema, size_t schema_len,
                                  const hu_json_value_t *obj);

/* Shared helper: parse JSON array of strings into allocated array. */
hu_error_t parse_string_array(hu_allocator_t *a, char ***out, size_t *out_len,
                              const hu_json_value_t *arr);

/* Provider-related parsers */
hu_error_t parse_providers(hu_allocator_t *a, hu_config_t *cfg, const hu_json_value_t *arr);
hu_error_t parse_router(hu_allocator_t *a, hu_config_t *cfg, const hu_json_value_t *obj);

/* Channel-related parsers */
hu_error_t parse_channels(hu_allocator_t *a, hu_config_t *cfg, const hu_json_value_t *obj);

/* Agent-related parsers */
hu_error_t parse_agent(hu_allocator_t *a, hu_config_t *cfg, const hu_json_value_t *obj);

/* Behavior thresholds parser */
hu_error_t parse_behavior(hu_allocator_t *a, hu_config_t *cfg, const hu_json_value_t *obj);

#endif /* HU_CONFIG_PARSE_INTERNAL_H */
