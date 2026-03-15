#include "human/agent/orchestrator_llm.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

#define HU_ORCH_LLM_CAPABILITY_SIZE 64

__attribute__((unused))
static void extract_json_from_response(const char *s, size_t len, const char **out_ptr,
                                       size_t *out_len) {
    const char *p = s;
    const char *end = s + len;

    while (p + 3 <= end && memcmp(p, "```", 3) == 0) {
        p += 3;
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p + 4 <= end && (memcmp(p, "json", 4) == 0 || memcmp(p, "JSON", 4) == 0))
            p += 4;
        while (p < end && *p != '\n')
            p++;
        if (p < end)
            p++;
    }

    while (p < end && *p != '{')
        p++;
    if (p >= end) {
        *out_ptr = s;
        *out_len = len;
        return;
    }
    const char *start = p;
    int depth = 1;
    p++;
    while (p < end && depth > 0) {
        if (*p == '{')
            depth++;
        else if (*p == '}')
            depth--;
        p++;
    }
    *out_ptr = start;
    *out_len = (size_t)(p - start);
}

static bool contains_substr(const char *haystack, size_t hlen, const char *needle, size_t nlen) {
    if (nlen == 0 || nlen > hlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        if (memcmp(haystack + i, needle, nlen) == 0)
            return true;
    }
    return false;
}

static bool agent_matches_capability(const hu_agent_capability_t *agent,
                                      const char *capability, size_t cap_len) {
    if (!capability || cap_len == 0)
        return true;
    if (agent->skills_len > 0 &&
        contains_substr(agent->skills, agent->skills_len, capability, cap_len))
        return true;
    if (agent->role_len > 0 &&
        contains_substr(agent->role, agent->role_len, capability, cap_len))
        return true;
    return false;
}

static size_t safe_strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n])
        n++;
    return n;
}

hu_error_t hu_orchestrator_decompose_goal(hu_allocator_t *alloc, hu_provider_t *provider,
                                           const char *model, size_t model_len,
                                           const char *goal, size_t goal_len,
                                           const hu_agent_capability_t *capabilities,
                                           size_t capability_count,
                                           hu_decomposition_t *result) {
    if (!alloc || !result)
        return HU_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)provider;
    (void)model;
    (void)model_len;
    (void)goal;
    (void)goal_len;
    (void)capabilities;
    (void)capability_count;

    result->task_count = 2;
    result->tasks[0].status = HU_TASK_UNASSIGNED;
    result->tasks[0].depends_on = 0;
    result->tasks[0].priority = 1.0;
    strncpy(result->tasks[0].description, "research the topic",
            sizeof(result->tasks[0].description) - 1);
    result->tasks[0].description[sizeof(result->tasks[0].description) - 1] = '\0';
    result->tasks[0].description_len = 18;

    strncpy(result->capabilities[0], "research", sizeof(result->capabilities[0]) - 1);
    result->capabilities[0][sizeof(result->capabilities[0]) - 1] = '\0';

    result->tasks[1].status = HU_TASK_UNASSIGNED;
    result->tasks[1].depends_on = 1; /* 1 = index of dependency (task 0) */
    result->tasks[1].priority = 1.0;
    strncpy(result->tasks[1].description, "synthesize findings",
            sizeof(result->tasks[1].description) - 1);
    result->tasks[1].description[sizeof(result->tasks[1].description) - 1] = '\0';
    result->tasks[1].description_len = 19;

    strncpy(result->capabilities[1], "synthesis", sizeof(result->capabilities[1]) - 1);
    result->capabilities[1][sizeof(result->capabilities[1]) - 1] = '\0';

    result->reasoning = hu_strndup(alloc, "Split into research and synthesis phases", 40);
    result->reasoning_len = result->reasoning ? 40 : 0;
    return HU_OK;
#else
    if (!provider || !provider->vtable || !provider->vtable->chat_with_system)
        return HU_ERR_NOT_SUPPORTED;
    if (goal_len > 0 && !goal)
        return HU_ERR_INVALID_ARGUMENT;

    size_t cap_buf = 256;
    for (size_t i = 0; i < capability_count && capabilities; i++) {
        cap_buf += capabilities[i].agent_id_len + capabilities[i].role_len +
                   capabilities[i].skills_len + 32;
    }
    size_t prompt_cap = 512 + (goal_len > 0 ? goal_len : 0) + cap_buf;
    char *prompt = (char *)alloc->alloc(alloc->ctx, prompt_cap);
    if (!prompt)
        return HU_ERR_OUT_OF_MEMORY;

    int pos = snprintf(prompt, prompt_cap,
                       "Decompose this goal into subtasks. Reply with JSON only: "
                       "{\"reasoning\":\"...\",\"tasks\":[{\"description\":\"...\","
                       "\"capability\":\"...\",\"depends_on\":[]}]}. "
                       "depends_on: array of task indices (0-based) this task depends on. "
                       "Available agents: ");
    if (pos < 0 || (size_t)pos >= prompt_cap) {
        alloc->free(alloc->ctx, prompt, prompt_cap);
        return HU_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < capability_count && capabilities; i++) {
        const hu_agent_capability_t *c = &capabilities[i];
        int n = snprintf(prompt + pos, prompt_cap - (size_t)pos, "[%.*s: role=%.*s skills=%.*s] ",
                         (int)c->agent_id_len, c->agent_id,
                         (int)c->role_len, c->role,
                         (int)c->skills_len, c->skills);
        if (n > 0 && pos + n < (int)prompt_cap)
            pos += n;
    }

    int n = snprintf(prompt + pos, prompt_cap - (size_t)pos, " Goal: %.*s",
                     (int)goal_len, goal && goal_len > 0 ? goal : "");
    if (n > 0 && pos + n < (int)prompt_cap)
        pos += n;

    const char *sys = "You are a task decomposition assistant. Output valid JSON only.";
    char *llm_out = NULL;
    size_t llm_out_len = 0;

    hu_error_t err = provider->vtable->chat_with_system(
        provider->ctx, alloc, sys, 55, prompt, (size_t)pos,
        model && model_len > 0 ? model : "gpt-4o-mini",
        model && model_len > 0 ? model_len : 11,
        0.2, &llm_out, &llm_out_len);

    alloc->free(alloc->ctx, prompt, prompt_cap);

    if (err != HU_OK)
        return err;
    if (!llm_out || llm_out_len == 0) {
        if (llm_out)
            alloc->free(alloc->ctx, llm_out, llm_out_len + 1);
        return HU_ERR_PROVIDER_RESPONSE;
    }

    const char *json_ptr = NULL;
    size_t json_len = 0;
    extract_json_from_response(llm_out, llm_out_len, &json_ptr, &json_len);

    hu_json_value_t *root = NULL;
    err = hu_json_parse(alloc, json_ptr, json_len, &root);
    alloc->free(alloc->ctx, llm_out, llm_out_len + 1);

    if (err != HU_OK)
        return err;

    const char *reasoning_str = root && root->type == HU_JSON_OBJECT
                                   ? hu_json_get_string(root, "reasoning")
                                   : NULL;
    if (reasoning_str) {
        result->reasoning = hu_strndup(alloc, reasoning_str, strlen(reasoning_str));
        result->reasoning_len = result->reasoning ? strlen(reasoning_str) : 0;
    }

    hu_json_value_t *tasks_arr =
        root && root->type == HU_JSON_OBJECT ? hu_json_object_get(root, "tasks") : NULL;
    if (!tasks_arr || tasks_arr->type != HU_JSON_ARRAY) {
        hu_json_free(alloc, root);
        return HU_ERR_PARSE;
    }

    size_t n_tasks = tasks_arr->data.array.len;
    if (n_tasks > HU_ORCH_LLM_MAX_SUBTASKS)
        n_tasks = HU_ORCH_LLM_MAX_SUBTASKS;

    for (size_t i = 0; i < n_tasks; i++) {
        hu_json_value_t *task_val = tasks_arr->data.array.items[i];
        if (!task_val || task_val->type != HU_JSON_OBJECT)
            continue;

        hu_orchestrator_task_t *t = &result->tasks[result->task_count];
        memset(t, 0, sizeof(*t));
        t->status = HU_TASK_UNASSIGNED;
        t->depends_on = 0;
        t->priority = 1.0;

        const char *desc = hu_json_get_string(task_val, "description");
        if (desc) {
            size_t dlen = strlen(desc);
            size_t copy = dlen < sizeof(t->description) - 1 ? dlen : sizeof(t->description) - 1;
            strncpy(t->description, desc, copy);
            t->description[copy] = '\0';
            t->description_len = copy;
        }

        const char *cap = hu_json_get_string(task_val, "capability");
        if (cap) {
            size_t clen = strlen(cap);
            size_t copy = clen < HU_ORCH_LLM_CAPABILITY_SIZE - 1 ? clen : HU_ORCH_LLM_CAPABILITY_SIZE - 1;
            strncpy(result->capabilities[result->task_count], cap, copy);
            result->capabilities[result->task_count][copy] = '\0';
        }

        hu_json_value_t *deps = hu_json_object_get(task_val, "depends_on");
        if (deps && deps->type == HU_JSON_ARRAY && deps->data.array.len > 0) {
            hu_json_value_t *first = deps->data.array.items[0];
            if (first && first->type == HU_JSON_NUMBER)
                t->depends_on = (uint32_t)(int)first->data.number + 1; /* 1-based for resolution */
        }

        result->task_count++;
    }

    hu_json_free(alloc, root);
    return HU_OK;
#endif
}

void hu_decomposition_free(hu_allocator_t *alloc, hu_decomposition_t *result) {
    if (!alloc || !result)
        return;
    if (result->reasoning) {
        alloc->free(alloc->ctx, result->reasoning, result->reasoning_len + 1);
        result->reasoning = NULL;
        result->reasoning_len = 0;
    }
}

hu_error_t hu_orchestrator_auto_assign(hu_orchestrator_t *orch,
                                        const hu_decomposition_t *decomposition) {
    if (!orch || !decomposition || decomposition->task_count == 0)
        return HU_ERR_INVALID_ARGUMENT;

    const char *subtasks[HU_ORCH_LLM_MAX_SUBTASKS];
    size_t subtask_lens[HU_ORCH_LLM_MAX_SUBTASKS];

    for (size_t i = 0; i < decomposition->task_count; i++) {
        subtasks[i] = decomposition->tasks[i].description;
        subtask_lens[i] = decomposition->tasks[i].description_len;
    }

    hu_error_t err = hu_orchestrator_propose_split(orch, "", 0, subtasks, subtask_lens,
                                                    decomposition->task_count);
    if (err != HU_OK)
        return err;

    size_t base = orch->task_count - decomposition->task_count;

    for (size_t i = 0; i < decomposition->task_count; i++) {
        hu_orchestrator_task_t *t = &orch->tasks[base + i];
        uint32_t dep_idx = decomposition->tasks[i].depends_on;
        if (dep_idx > 0 && dep_idx <= decomposition->task_count) {
            t->depends_on = orch->tasks[base + dep_idx - 1].id;
        }
    }

    for (size_t i = 0; i < decomposition->task_count; i++) {
        const char *cap = decomposition->capabilities[i];
        size_t cap_len = safe_strnlen(cap, HU_ORCH_LLM_CAPABILITY_SIZE - 1);

        size_t agent_idx = orch->agent_count;
        for (size_t a = 0; a < orch->agent_count; a++) {
            if (agent_matches_capability(&orch->agents[a], cap, cap_len)) {
                agent_idx = a;
                break;
            }
        }

        if (agent_idx >= orch->agent_count)
            return HU_ERR_NOT_FOUND;

        err = hu_orchestrator_assign_task(orch, orch->tasks[base + i].id,
                                          orch->agents[agent_idx].agent_id,
                                          orch->agents[agent_idx].agent_id_len);
        if (err != HU_OK)
            return err;
    }

    return HU_OK;
}
