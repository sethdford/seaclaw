#ifndef SC_WORKTREE_H
#define SC_WORKTREE_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sc_worktree {
    char *path;           /* e.g., "/path/to/repo-agent-1" */
    char *branch;        /* e.g., "agent/task-42" */
    uint64_t agent_id;   /* owning agent */
    bool active;
} sc_worktree_t;

typedef struct sc_worktree_manager sc_worktree_manager_t;

/* Create manager anchored to a git repo root */
sc_worktree_manager_t *sc_worktree_manager_create(sc_allocator_t *alloc,
    const char *repo_root, size_t max_worktrees);
void sc_worktree_manager_destroy(sc_worktree_manager_t *mgr);

/* Create a worktree for an agent: git worktree add <path> -b <branch>
 * path is auto-generated: <repo_root>-agent-<agent_id>
 * branch is auto-generated: agent/<agent_id>
 */
sc_error_t sc_worktree_create(sc_worktree_manager_t *mgr, uint64_t agent_id,
    sc_worktree_t *out);

/* Remove a worktree: git worktree remove <path> */
sc_error_t sc_worktree_remove(sc_worktree_manager_t *mgr, uint64_t agent_id);

/* Get worktree info for an agent */
sc_error_t sc_worktree_get(sc_worktree_manager_t *mgr, uint64_t agent_id,
    sc_worktree_t *out);

/* List all active worktrees. Caller must sc_worktree_free each element and free the array. */
sc_error_t sc_worktree_list(sc_worktree_manager_t *mgr,
    sc_worktree_t **out, size_t *out_count);

/* Merge agent's branch back to base: git merge <branch> */
sc_error_t sc_worktree_merge(sc_worktree_manager_t *mgr, uint64_t agent_id);

void sc_worktree_free(sc_allocator_t *alloc, sc_worktree_t *wt);

#endif /* SC_WORKTREE_H */
