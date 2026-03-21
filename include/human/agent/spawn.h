#ifndef HU_AGENT_SPAWN_H
#define HU_AGENT_SPAWN_H

#include "human/agent/mailbox.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/security.h"

typedef struct hu_worktree_manager hu_worktree_manager_t;
typedef struct hu_team_config hu_team_config_t;
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_spawn_mode {
    HU_SPAWN_ONE_SHOT,
    HU_SPAWN_PERSISTENT,
} hu_spawn_mode_t;

typedef struct hu_spawn_config {
    const char *provider;
    size_t provider_len;
    const char *api_key;
    size_t api_key_len;
    const char *base_url;
    size_t base_url_len;
    const char *model;
    size_t model_len;
    double temperature;
    const char *workspace_dir;
    size_t workspace_dir_len;
    const char *system_prompt;
    size_t system_prompt_len;
    const char *persona_name;
    size_t persona_name_len;
    const char *const *tool_names;
    size_t tool_names_count;
    hu_security_policy_t *policy;
    double budget_usd;
    uint32_t max_iterations;
    hu_spawn_mode_t mode;
    hu_mailbox_t *mailbox; /* optional; when set, child agents share this mailbox */
    /* Optional parity with parent agent (pointers alias parent; not copied). When set with
     * tools_count > 0, child gets same tool surface + memory + skill catalog as parent. */
    void *skillforge; /* hu_skillforge_t * when HU_HAS_SKILLS */
    const void *parent_tools; /* const hu_tool_t * */
    size_t parent_tools_count;
    void *memory;           /* hu_memory_t * */
    void *session_store;    /* hu_session_store_t * */
    void *observer;         /* hu_observer_t * */
    uint8_t autonomy_level; /* 0 = default 2 when no parent tools; else typical parent copy */
} hu_spawn_config_t;

typedef enum hu_agent_status {
    HU_AGENT_RUNNING,
    HU_AGENT_IDLE,
    HU_AGENT_COMPLETED,
    HU_AGENT_FAILED,
    HU_AGENT_CANCELLED,
} hu_agent_status_t;

typedef struct hu_agent_pool hu_agent_pool_t;

hu_agent_pool_t *hu_agent_pool_create(hu_allocator_t *alloc, uint32_t max_concurrent);
void hu_agent_pool_destroy(hu_agent_pool_t *pool);

void hu_agent_pool_set_worktree_manager(hu_agent_pool_t *pool, hu_worktree_manager_t *worktree_mgr);

void hu_agent_pool_set_team_config(hu_agent_pool_t *pool, hu_team_config_t *team_config);

hu_error_t hu_agent_pool_spawn(hu_agent_pool_t *pool, const hu_spawn_config_t *cfg,
                               const char *task, size_t task_len, const char *label,
                               uint64_t *out_id);

hu_error_t hu_agent_pool_query(hu_agent_pool_t *pool, uint64_t agent_id, const char *message,
                               size_t message_len, char **out_response, size_t *out_response_len);

hu_agent_status_t hu_agent_pool_status(hu_agent_pool_t *pool, uint64_t agent_id);
const char *hu_agent_pool_result(hu_agent_pool_t *pool, uint64_t agent_id);
hu_error_t hu_agent_pool_cancel(hu_agent_pool_t *pool, uint64_t agent_id);
size_t hu_agent_pool_running_count(hu_agent_pool_t *pool);

typedef struct hu_agent_pool_info {
    uint64_t agent_id;
    hu_agent_status_t status;
    const char *label;
    const char *model;
    hu_spawn_mode_t mode;
    int64_t started_at;
    double cost_usd;
} hu_agent_pool_info_t;

hu_error_t hu_agent_pool_list(hu_agent_pool_t *pool, hu_allocator_t *alloc,
                              hu_agent_pool_info_t **out, size_t *out_count);

/* Forward declaration for the agent registry */
typedef struct hu_agent_registry hu_agent_registry_t;

/* Spawn an agent from a named config in the registry. Looks up the agent
 * definition by name, builds a spawn config from it, and spawns. */
hu_error_t hu_agent_pool_spawn_named(hu_agent_pool_t *pool,
                                     const hu_agent_registry_t *registry,
                                     const char *agent_name, const char *task,
                                     size_t task_len, uint64_t *out_id);

/* Build a hu_spawn_config_t from a hu_named_agent_config_t. Caller provides
 * the spawn_config struct; string pointers reference the named config
 * (caller must keep the named config alive). */
struct hu_named_agent_config;
void hu_spawn_config_from_named(hu_spawn_config_t *out,
                                const struct hu_named_agent_config *cfg);

#endif /* HU_AGENT_SPAWN_H */
