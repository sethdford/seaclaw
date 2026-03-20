/*
 * skill_run — invoke a SkillForge skill by name. Loads SKILL.md instructions
 * (level-2 progressive disclosure) when available; optionally runs the skill
 * command via the same policy/sandbox path as the shell tool (non-test POSIX).
 */
#include "human/tools/skill_run.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/skillforge.h"
#include <string.h>

#if defined(HU_GATEWAY_POSIX) && !defined(HU_IS_TEST) && !defined(_WIN32)
#include "human/core/process_util.h"
#include "human/security.h"
#endif

#define TOOL_NAME "skill_run"
#define TOOL_DESC "Run a named skill from the skill registry"
#define TOOL_PARAMS                                                                               \
    "{\"type\":\"object\",\"properties\":{\"skill\":{\"type\":\"string\","                        \
    "\"description\":\"Name of the skill to run\"},\"args\":{\"type\":\"object\","                \
    "\"description\":\"Arguments to pass to the skill\"}},\"required\":[\"skill\"]}"

#define SKILL_RUN_CMD_MAX 4096
#define SKILL_RUN_CAPTURE_MAX 4096

typedef struct {
    hu_skillforge_t *skillforge;
    char *workspace_dir;
    size_t workspace_dir_len;
    hu_security_policy_t *policy;
} skill_run_ctx_t;

static hu_error_t skill_run_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                    hu_tool_result_t *out) {
    skill_run_ctx_t *sctx = (skill_run_ctx_t *)ctx;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!args || !sctx || !sctx->skillforge) {
        *out = hu_tool_result_fail("invalid args or no skill registry", 33);
        return HU_OK;
    }

    const char *skill_name = hu_json_get_string(args, "skill");
    if (!skill_name || !skill_name[0]) {
        *out = hu_tool_result_fail("missing skill name", 18);
        return HU_OK;
    }

    hu_skill_t *skill = hu_skillforge_get_skill(sctx->skillforge, skill_name);
    if (!skill) {
        char *msg = hu_sprintf(alloc, "skill '%s' not found", skill_name);
        if (!msg) {
            *out = hu_tool_result_fail("skill not found", 15);
            return HU_OK;
        }
        size_t len = strlen(msg);
        *out = hu_tool_result_fail_owned(msg, len);
        return HU_OK;
    }

    if (!skill->enabled) {
        char *msg = hu_sprintf(alloc, "skill '%s' is disabled", skill_name);
        if (!msg) {
            *out = hu_tool_result_fail("skill disabled", 14);
            return HU_OK;
        }
        size_t len = strlen(msg);
        *out = hu_tool_result_fail_owned(msg, len);
        return HU_OK;
    }

    char *instr = NULL;
    hu_error_t ierr = hu_skillforge_load_instructions(alloc, skill, &instr, NULL);
    if (ierr != HU_OK || !instr) {
        *out = hu_tool_result_fail("failed to load skill instructions", 34);
        return ierr != HU_OK ? ierr : HU_ERR_OUT_OF_MEMORY;
    }

    const char *cmd = skill->command;
    char *result = NULL;

#if defined(HU_GATEWAY_POSIX) && !defined(HU_IS_TEST) && !defined(_WIN32)
    if (cmd && cmd[0]) {
        size_t cmd_len = strlen(cmd);
        if (cmd_len > SKILL_RUN_CMD_MAX) {
            alloc->free(alloc->ctx, instr, strlen(instr) + 1);
            *out = hu_tool_result_fail("skill command too long", 22);
            return HU_OK;
        }
        if (sctx->policy && !hu_security_shell_allowed(sctx->policy)) {
            result = hu_sprintf(alloc, "[Skill: %s]\nInstructions:\n%s\n\nCommand: %s\n\n(Output "
                                      "skipped: shell execution not allowed by policy)",
                                skill_name, instr, cmd);
        } else {
            bool blocked = false;
            bool need_appr = false;
            if (sctx->policy) {
                bool approved = sctx->policy->pre_approved;
                sctx->policy->pre_approved = false;
                hu_command_risk_level_t risk;
                hu_error_t perr = hu_policy_validate_command(sctx->policy, cmd, approved, &risk);
                if (perr == HU_ERR_SECURITY_APPROVAL_REQUIRED) {
                    blocked = true;
                    need_appr = true;
                } else if (perr != HU_OK) {
                    blocked = true;
                }
            }
            if (blocked) {
                result = hu_sprintf(alloc,
                                    "[Skill: %s]\nInstructions:\n%s\n\nCommand: %s\n\n(Output "
                                    "skipped: %s)",
                                    skill_name, instr, cmd,
                                    need_appr ? "approval required" : "command blocked by policy");
            } else {
                const char *argv[] = {"/bin/sh", "-c", cmd, NULL};
                const char *cwd = (sctx->workspace_dir && sctx->workspace_dir_len > 0)
                                      ? sctx->workspace_dir
                                      : NULL;
                hu_run_result_t rr = {0};
                hu_error_t rerr = hu_process_run_with_policy(alloc, argv, cwd, SKILL_RUN_CAPTURE_MAX,
                                                             sctx->policy, &rr);
                if (rerr != HU_OK) {
                    result = hu_sprintf(alloc,
                                        "[Skill: %s]\nInstructions:\n%s\n\nCommand: %s\n\n(Output: "
                                        "execution error)",
                                        skill_name, instr, cmd);
                } else {
                    const char *so = rr.stdout_buf ? rr.stdout_buf : "";
                    const char *se = rr.stderr_buf ? rr.stderr_buf : "";
                    result = hu_sprintf(alloc,
                                        "[Skill: %s]\nInstructions:\n%s\n\nCommand: %s\n\nOutput:\n"
                                        "%s%s",
                                        skill_name, instr, cmd, so, se);
                }
                hu_run_result_free(alloc, &rr);
            }
        }
    } else {
        result = hu_sprintf(alloc, "[Skill: %s]\nInstructions:\n%s", skill_name, instr);
    }
#else
    if (cmd && cmd[0])
        result = hu_sprintf(alloc, "[Skill: %s]\nInstructions:\n%s\n\nCommand: %s", skill_name,
                            instr, cmd);
    else
        result = hu_sprintf(alloc, "[Skill: %s]\nInstructions:\n%s", skill_name, instr);
#endif

    alloc->free(alloc->ctx, instr, strlen(instr) + 1);

    if (!result) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t rlen = strlen(result);
    *out = hu_tool_result_ok_owned(result, rlen);
    return HU_OK;
}

static const char *skill_run_name(void *ctx) {
    (void)ctx;
    return TOOL_NAME;
}
static const char *skill_run_description(void *ctx) {
    (void)ctx;
    return TOOL_DESC;
}
static const char *skill_run_parameters_json(void *ctx) {
    (void)ctx;
    return TOOL_PARAMS;
}
static void skill_run_deinit(void *ctx, hu_allocator_t *alloc) {
    if (!ctx)
        return;
    skill_run_ctx_t *s = (skill_run_ctx_t *)ctx;
    if (s->workspace_dir && alloc)
        alloc->free(alloc->ctx, s->workspace_dir, s->workspace_dir_len + 1);
    if (alloc)
        alloc->free(alloc->ctx, s, sizeof(skill_run_ctx_t));
}

static const hu_tool_vtable_t skill_run_vtable = {
    .execute = skill_run_execute,
    .name = skill_run_name,
    .description = skill_run_description,
    .parameters_json = skill_run_parameters_json,
    .deinit = skill_run_deinit,
};

hu_error_t hu_skill_run_create(hu_allocator_t *alloc, hu_tool_t *out,
                               hu_skillforge_t *skillforge, const char *workspace_dir,
                               size_t workspace_dir_len, hu_security_policy_t *policy) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    skill_run_ctx_t *ctx = (skill_run_ctx_t *)alloc->alloc(alloc->ctx, sizeof(skill_run_ctx_t));
    if (!ctx)
        return HU_ERR_OUT_OF_MEMORY;
    ctx->skillforge = skillforge;
    ctx->policy = policy;
    ctx->workspace_dir = NULL;
    ctx->workspace_dir_len = 0;
    if (workspace_dir && workspace_dir_len > 0) {
        ctx->workspace_dir = hu_strndup(alloc, workspace_dir, workspace_dir_len);
        if (!ctx->workspace_dir) {
            alloc->free(alloc->ctx, ctx, sizeof(skill_run_ctx_t));
            return HU_ERR_OUT_OF_MEMORY;
        }
        ctx->workspace_dir_len = strlen(ctx->workspace_dir);
    }
    out->ctx = ctx;
    out->vtable = &skill_run_vtable;
    return HU_OK;
}
