#include "seaclaw/agent/worktree.h"
#include "seaclaw/core/string.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SC_WORKTREE_PATH_FMT "%s-agent-%llu"
#define SC_WORKTREE_BRANCH_FMT "agent/%llu"
#define SC_WORKTREE_CMD_ADD "git worktree add %s -b %s"
#define SC_WORKTREE_CMD_REMOVE "git worktree remove %s --force"
#define SC_WORKTREE_CMD_MERGE "git merge %s"

struct sc_worktree_manager {
    sc_allocator_t *alloc;
    char *repo_root;
    size_t max_worktrees;
    sc_worktree_t *worktrees;
    size_t worktrees_count;
};

static sc_worktree_t *find_by_agent_id(sc_worktree_manager_t *mgr, uint64_t agent_id) {
    for (size_t i = 0; i < mgr->worktrees_count; i++) {
        if (mgr->worktrees[i].agent_id == agent_id)
            return &mgr->worktrees[i];
    }
    return NULL;
}

static sc_error_t run_git_cmd(const char *repo_root, const char *fmt, ...) {
    char cmd[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof(cmd))
        return SC_ERR_INVALID_ARGUMENT;

    char full_cmd[1280];
    n = snprintf(full_cmd, sizeof(full_cmd), "cd %s && %s", repo_root, cmd);
    if (n < 0 || (size_t)n >= sizeof(full_cmd))
        return SC_ERR_INVALID_ARGUMENT;

    int r = system(full_cmd);
    return (r == 0) ? SC_OK : SC_ERR_IO;
}

sc_worktree_manager_t *sc_worktree_manager_create(sc_allocator_t *alloc,
    const char *repo_root, size_t max_worktrees) {
    if (!alloc || !repo_root || max_worktrees == 0)
        return NULL;
    sc_worktree_manager_t *mgr =
        (sc_worktree_manager_t *)alloc->alloc(alloc->ctx, sizeof(*mgr));
    if (!mgr)
        return NULL;
    memset(mgr, 0, sizeof(*mgr));
    mgr->alloc = alloc;
    mgr->repo_root = sc_strdup(alloc, repo_root);
    if (!mgr->repo_root) {
        alloc->free(alloc->ctx, mgr, sizeof(*mgr));
        return NULL;
    }
    mgr->max_worktrees = max_worktrees;
    mgr->worktrees =
        (sc_worktree_t *)alloc->alloc(alloc->ctx, max_worktrees * sizeof(sc_worktree_t));
    if (!mgr->worktrees) {
        alloc->free(alloc->ctx, mgr->repo_root, strlen(mgr->repo_root) + 1);
        alloc->free(alloc->ctx, mgr, sizeof(*mgr));
        return NULL;
    }
    memset(mgr->worktrees, 0, max_worktrees * sizeof(sc_worktree_t));
    return mgr;
}

void sc_worktree_manager_destroy(sc_worktree_manager_t *mgr) {
    if (!mgr)
        return;
    sc_allocator_t *a = mgr->alloc;
    for (size_t i = 0; i < mgr->worktrees_count; i++) {
        sc_worktree_free(a, &mgr->worktrees[i]);
    }
    if (mgr->repo_root)
        a->free(a->ctx, mgr->repo_root, strlen(mgr->repo_root) + 1);
    if (mgr->worktrees)
        a->free(a->ctx, mgr->worktrees, mgr->max_worktrees * sizeof(sc_worktree_t));
    a->free(a->ctx, mgr, sizeof(*mgr));
}

sc_error_t sc_worktree_create(sc_worktree_manager_t *mgr, uint64_t agent_id,
    sc_worktree_t *out) {
    if (!mgr || !out)
        return SC_ERR_INVALID_ARGUMENT;
    if (find_by_agent_id(mgr, agent_id))
        return SC_ERR_ALREADY_EXISTS;
    if (mgr->worktrees_count >= mgr->max_worktrees)
        return SC_ERR_OUT_OF_MEMORY;

    char path[1024];
    char branch[128];
    int n = snprintf(path, sizeof(path), SC_WORKTREE_PATH_FMT, mgr->repo_root, (unsigned long long)agent_id);
    if (n < 0 || (size_t)n >= sizeof(path))
        return SC_ERR_INVALID_ARGUMENT;
    n = snprintf(branch, sizeof(branch), SC_WORKTREE_BRANCH_FMT, (unsigned long long)agent_id);
    if (n < 0 || (size_t)n >= sizeof(branch))
        return SC_ERR_INVALID_ARGUMENT;

#if defined(SC_IS_TEST) && SC_IS_TEST == 1
    (void)path;
    (void)branch;
#else
    if (run_git_cmd(mgr->repo_root, SC_WORKTREE_CMD_ADD, path, branch) != SC_OK)
        return SC_ERR_IO;
#endif

    sc_worktree_t *wt = &mgr->worktrees[mgr->worktrees_count++];
    wt->path = sc_strdup(mgr->alloc, path);
    wt->branch = sc_strdup(mgr->alloc, branch);
    wt->agent_id = agent_id;
    wt->active = true;

    out->path = sc_strdup(mgr->alloc, path);
    out->branch = sc_strdup(mgr->alloc, branch);
    out->agent_id = agent_id;
    out->active = true;
    return SC_OK;
}

sc_error_t sc_worktree_remove(sc_worktree_manager_t *mgr, uint64_t agent_id) {
    if (!mgr)
        return SC_ERR_INVALID_ARGUMENT;
    sc_worktree_t *wt = find_by_agent_id(mgr, agent_id);
    if (!wt)
        return SC_ERR_NOT_FOUND;

#if defined(SC_IS_TEST) && SC_IS_TEST == 1
    (void)wt;
#else
    if (run_git_cmd(mgr->repo_root, SC_WORKTREE_CMD_REMOVE, wt->path) != SC_OK)
        return SC_ERR_IO;
#endif

    wt->active = false;
    return SC_OK;
}

sc_error_t sc_worktree_get(sc_worktree_manager_t *mgr, uint64_t agent_id,
    sc_worktree_t *out) {
    if (!mgr || !out)
        return SC_ERR_INVALID_ARGUMENT;
    sc_worktree_t *wt = find_by_agent_id(mgr, agent_id);
    if (!wt || !wt->active)
        return SC_ERR_NOT_FOUND;
    out->path = wt->path ? sc_strdup(mgr->alloc, wt->path) : NULL;
    out->branch = wt->branch ? sc_strdup(mgr->alloc, wt->branch) : NULL;
    out->agent_id = wt->agent_id;
    out->active = wt->active;
    return SC_OK;
}

sc_error_t sc_worktree_list(sc_worktree_manager_t *mgr,
    sc_worktree_t **out, size_t *out_count) {
    if (!mgr || !out || !out_count)
        return SC_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_count = 0;

    size_t n = 0;
    for (size_t i = 0; i < mgr->worktrees_count; i++)
        if (mgr->worktrees[i].active)
            n++;
    if (n == 0)
        return SC_OK;

    sc_worktree_t *arr =
        (sc_worktree_t *)mgr->alloc->alloc(mgr->alloc->ctx, n * sizeof(sc_worktree_t));
    if (!arr)
        return SC_ERR_OUT_OF_MEMORY;

    size_t j = 0;
    for (size_t i = 0; i < mgr->worktrees_count && j < n; i++) {
        if (!mgr->worktrees[i].active)
            continue;
        sc_worktree_t *src = &mgr->worktrees[i];
        arr[j].path = src->path ? sc_strdup(mgr->alloc, src->path) : NULL;
        arr[j].branch = src->branch ? sc_strdup(mgr->alloc, src->branch) : NULL;
        arr[j].agent_id = src->agent_id;
        arr[j].active = src->active;
        j++;
    }
    *out = arr;
    *out_count = j;
    return SC_OK;
}

sc_error_t sc_worktree_merge(sc_worktree_manager_t *mgr, uint64_t agent_id) {
    if (!mgr)
        return SC_ERR_INVALID_ARGUMENT;
    sc_worktree_t *wt = find_by_agent_id(mgr, agent_id);
    if (!wt || !wt->active)
        return SC_ERR_NOT_FOUND;

#if defined(SC_IS_TEST) && SC_IS_TEST == 1
    (void)wt;
    return SC_OK;
#else
    return run_git_cmd(mgr->repo_root, SC_WORKTREE_CMD_MERGE, wt->branch);
#endif
}

void sc_worktree_free(sc_allocator_t *alloc, sc_worktree_t *wt) {
    if (!alloc || !wt)
        return;
    if (wt->path) {
        alloc->free(alloc->ctx, wt->path, strlen(wt->path) + 1);
        wt->path = NULL;
    }
    if (wt->branch) {
        alloc->free(alloc->ctx, wt->branch, strlen(wt->branch) + 1);
        wt->branch = NULL;
    }
}
