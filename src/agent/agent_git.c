#include "human/agent/agent_git.h"
#include "human/core/process_util.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(HU_IS_TEST) && HU_IS_TEST
static void agent_git__clear_out(char **out, size_t *out_len) {
    if (out)
        *out = NULL;
    if (out_len)
        *out_len = 0;
}
#endif

static bool agent_git__ref_token_safe(const char *s) {
    if (!s || !s[0])
        return false;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
            c == '_' || c == '/' || c == '-' || c == '~' || c == '^')
            continue;
        return false;
    }
    return true;
}

#if defined(HU_GATEWAY_POSIX) && !(defined(HU_IS_TEST) && HU_IS_TEST)

static hu_error_t agent_git__run(hu_allocator_t *alloc, const char *cwd, const char *const *argv,
                                 bool allow_empty_commit) {
    hu_run_result_t rr = {0};
    hu_error_t e = hu_process_run(alloc, argv, cwd, 65536u, &rr);
    if (e != HU_OK) {
        hu_run_result_free(alloc, &rr);
        return e;
    }
    if (!rr.success) {
        if (allow_empty_commit && rr.stderr_buf && strstr(rr.stderr_buf, "nothing to commit") != NULL) {
            hu_run_result_free(alloc, &rr);
            return HU_OK;
        }
        hu_run_result_free(alloc, &rr);
        return HU_ERR_IO;
    }
    hu_run_result_free(alloc, &rr);
    return HU_OK;
}

static hu_error_t agent_git__capture(hu_allocator_t *alloc, const char *cwd, const char *const *argv,
                                     char **out, size_t *out_len) {
    hu_run_result_t rr = {0};
    hu_error_t e = hu_process_run(alloc, argv, cwd, 4u * 1024u * 1024u, &rr);
    if (e != HU_OK) {
        hu_run_result_free(alloc, &rr);
        return e;
    }
    if (!rr.success) {
        hu_run_result_free(alloc, &rr);
        return HU_ERR_IO;
    }
    size_t n = rr.stdout_len;
    char *buf = (char *)alloc->alloc(alloc->ctx, n + 1u);
    if (!buf) {
        hu_run_result_free(alloc, &rr);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(buf, rr.stdout_buf, n);
    buf[n] = '\0';
    hu_run_result_free(alloc, &rr);
    *out = buf;
    *out_len = n;
    return HU_OK;
}

#endif

hu_error_t hu_agent_git_init(hu_allocator_t *alloc, const char *workspace_dir) {
    if (!alloc || !workspace_dir)
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)workspace_dir;
    return HU_OK;
#elif defined(HU_GATEWAY_POSIX)
    {
        const char *argv_init[] = {"git", "init", NULL};
        hu_error_t e = agent_git__run(alloc, workspace_dir, argv_init, false);
        if (e != HU_OK)
            return e;
        const char *argv_add[] = {"git", "add", "-A", NULL};
        e = agent_git__run(alloc, workspace_dir, argv_add, false);
        if (e != HU_OK)
            return e;
        const char *argv_commit[] = {"git", "commit", "-m", "initial", NULL};
        return agent_git__run(alloc, workspace_dir, argv_commit, true);
    }
#else
    (void)workspace_dir;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_agent_git_snapshot(hu_allocator_t *alloc, const char *workspace_dir, const char *message) {
    if (!alloc || !workspace_dir || !message)
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)message;
    return HU_OK;
#elif defined(HU_GATEWAY_POSIX)
    {
        const char *argv_add[] = {"git", "add", "-A", NULL};
        hu_error_t e = agent_git__run(alloc, workspace_dir, argv_add, false);
        if (e != HU_OK)
            return e;
        const char *argv_commit[] = {"git", "commit", "-m", message, NULL};
        return agent_git__run(alloc, workspace_dir, argv_commit, true);
    }
#else
    (void)message;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_agent_git_rollback(hu_allocator_t *alloc, const char *workspace_dir, const char *ref) {
    if (!alloc || !workspace_dir || !ref)
        return HU_ERR_INVALID_ARGUMENT;
    if (!agent_git__ref_token_safe(ref))
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)workspace_dir;
    (void)ref;
    return HU_OK;
#elif defined(HU_GATEWAY_POSIX)
    {
        const char *argv[] = {"git", "checkout", ref, "--", ".", NULL};
        return agent_git__run(alloc, workspace_dir, argv, false);
    }
#else
    (void)workspace_dir;
    (void)ref;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_agent_git_diff(hu_allocator_t *alloc, const char *workspace_dir, const char *ref1,
                             const char *ref2, char **out, size_t *out_len) {
    if (!alloc || !workspace_dir || !ref1 || !ref2 || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (!agent_git__ref_token_safe(ref1) || !agent_git__ref_token_safe(ref2))
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    agent_git__clear_out(out, out_len);
    (void)workspace_dir;
    (void)ref1;
    (void)ref2;
    return HU_OK;
#elif defined(HU_GATEWAY_POSIX)
    {
        const char *argv[] = {"git", "diff", ref1, ref2, NULL};
        return agent_git__capture(alloc, workspace_dir, argv, out, out_len);
    }
#else
    (void)workspace_dir;
    (void)ref1;
    (void)ref2;
    *out = NULL;
    *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_agent_git_branch(hu_allocator_t *alloc, const char *workspace_dir, const char *name) {
    if (!alloc || !workspace_dir || !name)
        return HU_ERR_INVALID_ARGUMENT;
    if (!agent_git__ref_token_safe(name))
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)workspace_dir;
    (void)name;
    return HU_OK;
#elif defined(HU_GATEWAY_POSIX)
    {
        const char *argv[] = {"git", "checkout", "-b", name, NULL};
        return agent_git__run(alloc, workspace_dir, argv, false);
    }
#else
    (void)workspace_dir;
    (void)name;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_agent_git_log(hu_allocator_t *alloc, const char *workspace_dir, size_t limit, char **out,
                            size_t *out_len) {
    if (!alloc || !workspace_dir || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (limit == 0u)
        limit = 1u;
    if (limit > 10000u)
        limit = 10000u;

#if defined(HU_IS_TEST) && HU_IS_TEST
    agent_git__clear_out(out, out_len);
    (void)workspace_dir;
    (void)limit;
    return HU_OK;
#elif defined(HU_GATEWAY_POSIX)
    {
        char limbuf[32];
        (void)snprintf(limbuf, sizeof(limbuf), "-%zu", limit);
        const char *argv[] = {"git", "log", "--oneline", limbuf, NULL};
        return agent_git__capture(alloc, workspace_dir, argv, out, out_len);
    }
#else
    (void)workspace_dir;
    (void)limit;
    *out = NULL;
    *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
#endif
}
