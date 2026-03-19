#include "human/agent/plan_executor.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

void hu_plan_executor_init(hu_plan_executor_t *exec, hu_allocator_t *alloc,
                           hu_provider_t *provider, const char *model, size_t model_len,
                           hu_tool_t *tools, size_t tools_count) {
    if (!exec)
        return;
    memset(exec, 0, sizeof(*exec));
    exec->alloc = alloc;
    exec->provider = provider;
    exec->model = model;
    exec->model_len = model_len;
    exec->tools = tools;
    exec->tools_count = tools_count;
    exec->max_replans = HU_PLAN_EXEC_MAX_REPLANS;
}

static hu_tool_t *find_tool(hu_tool_t *tools, size_t count, const char *name) {
    if (!tools || !name)
        return NULL;
    size_t name_len = strlen(name);
    for (size_t i = 0; i < count; i++) {
        if (tools[i].vtable && tools[i].vtable->name) {
            const char *tn = tools[i].vtable->name(tools[i].ctx);
            if (tn && strlen(tn) == name_len && memcmp(tn, name, name_len) == 0)
                return &tools[i];
        }
    }
    return NULL;
}

static void build_progress_summary(hu_plan_t *plan, char *buf, size_t cap, size_t *out_len) {
    size_t pos = 0;
    for (size_t i = 0; i < plan->steps_count && pos < cap - 1; i++) {
        const char *status_str = "pending";
        if (plan->steps[i].status == HU_PLAN_STEP_DONE)
            status_str = "done";
        else if (plan->steps[i].status == HU_PLAN_STEP_FAILED)
            status_str = "failed";
        else if (plan->steps[i].status == HU_PLAN_STEP_RUNNING)
            status_str = "running";
        int n = snprintf(buf + pos, cap - pos, "Step %zu (%s): %s\n",
                         i + 1, plan->steps[i].tool_name ? plan->steps[i].tool_name : "?",
                         status_str);
        if (n > 0 && pos + (size_t)n < cap)
            pos += (size_t)n;
    }
    buf[pos] = '\0';
    *out_len = pos;
}

static void swap_plan_steps(hu_allocator_t *alloc, hu_plan_t *plan, hu_plan_t *revised) {
    if (plan->steps) {
        for (size_t si = 0; si < plan->steps_count; si++) {
            if (plan->steps[si].tool_name)
                alloc->free(alloc->ctx, plan->steps[si].tool_name,
                            strlen(plan->steps[si].tool_name) + 1);
            if (plan->steps[si].args_json)
                alloc->free(alloc->ctx, plan->steps[si].args_json,
                            strlen(plan->steps[si].args_json) + 1);
            if (plan->steps[si].description)
                alloc->free(alloc->ctx, plan->steps[si].description,
                            strlen(plan->steps[si].description) + 1);
        }
        alloc->free(alloc->ctx, plan->steps, plan->steps_cap * sizeof(hu_plan_step_t));
    }
    plan->steps = revised->steps;
    plan->steps_count = revised->steps_count;
    plan->steps_cap = revised->steps_cap;
    revised->steps = NULL;
    revised->steps_count = 0;
}

hu_error_t hu_plan_executor_run(hu_plan_executor_t *exec, hu_plan_t *plan,
                                const char *goal, size_t goal_len,
                                hu_plan_exec_result_t *result) {
    if (!exec || !plan || !result)
        return HU_ERR_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));
    uint32_t replans_done = 0;

    while (!hu_planner_is_complete(plan)) {
        hu_plan_step_t *step = hu_planner_next_step(plan);
        if (!step)
            break;

        size_t step_idx = (size_t)(step - plan->steps);
        hu_planner_mark_step(plan, step_idx, HU_PLAN_STEP_RUNNING);

        hu_tool_t *tool = find_tool(exec->tools, exec->tools_count, step->tool_name);
        if (!tool || !tool->vtable || !tool->vtable->execute) {
            hu_planner_mark_step(plan, step_idx, HU_PLAN_STEP_FAILED);
            result->steps_failed++;

            if (replans_done < exec->max_replans && goal && goal_len > 0) {
                char progress[2048];
                size_t plen = 0;
                build_progress_summary(plan, progress, sizeof(progress), &plen);

                char fail_detail[256];
                int fd_n = snprintf(fail_detail, sizeof(fail_detail),
                                    "Tool '%s' not found", step->tool_name ? step->tool_name : "?");
                size_t fd_len = (fd_n > 0) ? (size_t)fd_n : 0;

                hu_plan_t *revised = NULL;
                hu_error_t rerr = hu_planner_replan(
                    exec->alloc, exec->provider, exec->model, exec->model_len,
                    goal, goal_len, progress, plen, fail_detail, fd_len, NULL, 0, &revised);
                if (rerr == HU_OK && revised && revised->steps_count > 0) {
                    swap_plan_steps(exec->alloc, plan, revised);
                    hu_plan_free(exec->alloc, revised);
                    replans_done++;
                    result->replans++;
                    continue;
                }
                if (revised)
                    hu_plan_free(exec->alloc, revised);
            }
            continue;
        }

        hu_json_value_t *args = NULL;
        if (step->args_json && strlen(step->args_json) > 0)
            hu_json_parse(exec->alloc, step->args_json, strlen(step->args_json), &args);
        if (!args)
            hu_json_parse(exec->alloc, "{}", 2, &args);

        hu_tool_result_t tr = hu_tool_result_fail("not executed", 12);
        if (args) {
            tool->vtable->execute(tool->ctx, exec->alloc, args, &tr);
            hu_json_free(exec->alloc, args);
        }

        if (tr.success) {
            hu_planner_mark_step(plan, step_idx, HU_PLAN_STEP_DONE);
            result->steps_completed++;
        } else {
            hu_planner_mark_step(plan, step_idx, HU_PLAN_STEP_FAILED);
            result->steps_failed++;

            if (replans_done < exec->max_replans && goal && goal_len > 0) {
                char progress[2048];
                size_t plen = 0;
                build_progress_summary(plan, progress, sizeof(progress), &plen);

                const char *fail_msg = tr.error_msg ? tr.error_msg : "unknown error";
                size_t fail_len = tr.error_msg_len > 0 ? tr.error_msg_len : strlen(fail_msg);

                hu_plan_t *revised = NULL;
                hu_error_t rerr = hu_planner_replan(
                    exec->alloc, exec->provider, exec->model, exec->model_len,
                    goal, goal_len, progress, plen, fail_msg, fail_len, NULL, 0, &revised);
                if (rerr == HU_OK && revised && revised->steps_count > 0) {
                    hu_tool_result_free(exec->alloc, &tr);
                    swap_plan_steps(exec->alloc, plan, revised);
                    hu_plan_free(exec->alloc, revised);
                    replans_done++;
                    result->replans++;
                    continue;
                }
                if (revised)
                    hu_plan_free(exec->alloc, revised);
            }
        }
        hu_tool_result_free(exec->alloc, &tr);
    }

    result->goal_achieved = (result->steps_failed == 0 && result->steps_completed > 0);
    int sn = snprintf(result->summary, sizeof(result->summary),
                       "Plan execution: %zu completed, %zu failed, %zu replans. %s",
                       result->steps_completed, result->steps_failed, result->replans,
                       result->goal_achieved ? "Goal achieved." : "Goal not fully achieved.");
    result->summary_len = (sn > 0) ? (size_t)sn : 0;

    return HU_OK;
}

hu_error_t hu_plan_executor_run_goal(hu_plan_executor_t *exec,
                                     const char *goal, size_t goal_len,
                                     hu_plan_exec_result_t *result) {
    if (!exec || !goal || !result)
        return HU_ERR_INVALID_ARGUMENT;

    const char **tool_names = NULL;
    if (exec->tools_count > 0) {
        tool_names = (const char **)exec->alloc->alloc(
            exec->alloc->ctx, exec->tools_count * sizeof(const char *));
        if (tool_names) {
            for (size_t i = 0; i < exec->tools_count; i++) {
                tool_names[i] = (exec->tools[i].vtable && exec->tools[i].vtable->name)
                                    ? exec->tools[i].vtable->name(exec->tools[i].ctx)
                                    : "unknown";
            }
        }
    }

    hu_plan_t *plan = NULL;
    hu_error_t err = hu_planner_generate(
        exec->alloc, exec->provider, exec->model, exec->model_len,
        goal, goal_len, tool_names, exec->tools_count, &plan);

    if (tool_names)
        exec->alloc->free(exec->alloc->ctx, (void *)tool_names,
                          exec->tools_count * sizeof(const char *));

    if (err != HU_OK || !plan)
        return err != HU_OK ? err : HU_ERR_NOT_FOUND;

    err = hu_plan_executor_run(exec, plan, goal, goal_len, result);
    hu_plan_free(exec->alloc, plan);
    return err;
}
