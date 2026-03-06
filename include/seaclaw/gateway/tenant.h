#ifndef SC_GATEWAY_TENANT_H
#define SC_GATEWAY_TENANT_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum sc_tenant_role {
    SC_TENANT_ROLE_ADMIN = 0,
    SC_TENANT_ROLE_USER = 1,
    SC_TENANT_ROLE_READONLY = 2
} sc_tenant_role_t;

typedef struct sc_tenant {
    char user_id[256];
    char display_name[256];
    sc_tenant_role_t role;
    uint32_t rate_limit_rpm;
    uint64_t usage_quota_tokens;
    uint64_t usage_current_tokens;
} sc_tenant_t;

typedef struct sc_tenant_store sc_tenant_store_t;

sc_error_t sc_tenant_store_init(sc_allocator_t *alloc, sc_tenant_store_t **out);
void sc_tenant_store_destroy(sc_tenant_store_t *store);

sc_error_t sc_tenant_create(sc_tenant_store_t *store, const sc_tenant_t *tenant);
sc_error_t sc_tenant_get(sc_tenant_store_t *store, const char *user_id, sc_tenant_t *out);
sc_error_t sc_tenant_update(sc_tenant_store_t *store, const sc_tenant_t *tenant);
sc_error_t sc_tenant_delete(sc_tenant_store_t *store, const char *user_id);
sc_error_t sc_tenant_list(sc_tenant_store_t *store, sc_tenant_t *out, size_t max, size_t *count);

sc_error_t sc_tenant_increment_usage(sc_tenant_store_t *store, const char *user_id,
                                     uint64_t tokens);
bool sc_tenant_check_quota(sc_tenant_store_t *store, const char *user_id);
bool sc_tenant_check_rate_limit(sc_tenant_store_t *store, const char *user_id);

#endif
