/*
 * skill_run — invoke a SkillForge skill by name. Looks up the skill's command
 * and description, returning instructions for the agent to follow.
 * Under HU_IS_TEST, returns the skill description without execution.
 */
#include "human/tools/skill_run.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <string.h>

#define TOOL_NAME "skill_run"
#define TOOL_DESC "Run a named skill from the skill registry"
#define TOOL_PARAMS                                                                               \
    "{\"type\":\"object\",\"properties\":{\"skill\":{\"type\":\"string\","                        \
    "\"description\":\"Name of the skill to run\"},\"args\":{\"type\":\"object\","                \
    "\"description\":\"Arguments to pass to the skill\"}},\"required\":[\"skill\"]}"

typedef struct {
    hu_skillforge_t *skillforge;
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

    /* Build output: skill description + command (if available) as instructions */
    const char *desc = skill->description ? skill->description : "";
    const char *cmd = skill->command;

    char *result = NULL;
    if (cmd && cmd[0]) {
        result = hu_sprintf(alloc, "[Skill: %s]\nDescription: %s\nCommand: %s", skill_name, desc,
                            cmd);
    } else {
        result = hu_sprintf(alloc, "[Skill: %s]\nDescription: %s", skill_name, desc);
    }

    if (!result) {
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t len = strlen(result);
    *out = hu_tool_result_ok_owned(result, len);
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
    if (ctx)
        alloc->free(alloc->ctx, ctx, sizeof(skill_run_ctx_t));
}

static const hu_tool_vtable_t skill_run_vtable = {
    .execute = skill_run_execute,
    .name = skill_run_name,
    .description = skill_run_description,
    .parameters_json = skill_run_parameters_json,
    .deinit = skill_run_deinit,
};

hu_error_t hu_skill_run_create(hu_allocator_t *alloc, hu_tool_t *out,
                               hu_skillforge_t *skillforge) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    skill_run_ctx_t *ctx =
        (skill_run_ctx_t *)alloc->alloc(alloc->ctx, sizeof(skill_run_ctx_t));
    if (!ctx)
        return HU_ERR_OUT_OF_MEMORY;
    ctx->skillforge = skillforge;
    out->ctx = ctx;
    out->vtable = &skill_run_vtable;
    return HU_OK;
}
