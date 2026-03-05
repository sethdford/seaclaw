#ifndef SC_TEAM_H
#define SC_TEAM_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/security.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sc_team_member {
    char *name;                      /* "backend", "frontend", "reviewer" */
    char *role;                      /* "builder", "reviewer", "tester" */
    sc_autonomy_level_t autonomy;    /* per-member autonomy level */
    char **allowed_tools;            /* NULL = all tools */
    size_t allowed_tools_count;
    char *model;                     /* provider/model override, NULL = default */
    uint64_t agent_id;               /* assigned at runtime */
    bool active;
} sc_team_member_t;

typedef struct sc_team_config {
    char *name;                      /* "checkout-feature" */
    sc_team_member_t *members;
    size_t members_count;
    char *base_branch;                /* branch to create worktrees from */
} sc_team_config_t;

/* Parse team config from JSON string */
sc_error_t sc_team_config_parse(sc_allocator_t *alloc, const char *json, size_t json_len,
    sc_team_config_t *out);

/* Free team config */
void sc_team_config_free(sc_allocator_t *alloc, sc_team_config_t *cfg);

/* Get member by name */
const sc_team_member_t *sc_team_config_get_member(const sc_team_config_t *cfg,
    const char *name);

/* Get member by role (first match) */
const sc_team_member_t *sc_team_config_get_by_role(const sc_team_config_t *cfg,
    const char *role);

#endif /* SC_TEAM_H */
