#ifndef HU_AGENT_GIT_H
#define HU_AGENT_GIT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

hu_error_t hu_agent_git_init(hu_allocator_t *alloc, const char *workspace_dir);
hu_error_t hu_agent_git_snapshot(hu_allocator_t *alloc, const char *workspace_dir, const char *message);
hu_error_t hu_agent_git_rollback(hu_allocator_t *alloc, const char *workspace_dir, const char *ref);
hu_error_t hu_agent_git_diff(hu_allocator_t *alloc, const char *workspace_dir, const char *ref1,
                             const char *ref2, char **out, size_t *out_len);
hu_error_t hu_agent_git_branch(hu_allocator_t *alloc, const char *workspace_dir, const char *name);
hu_error_t hu_agent_git_log(hu_allocator_t *alloc, const char *workspace_dir, size_t limit, char **out,
                            size_t *out_len);

#endif
