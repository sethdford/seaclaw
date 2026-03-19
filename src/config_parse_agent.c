#include "config_internal.h"
#include "config_parse_internal.h"
#include "human/config.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

hu_error_t parse_agent(hu_allocator_t *a, hu_config_t *cfg, const hu_json_value_t *obj) {
    if (!obj || obj->type != HU_JSON_OBJECT)
        return HU_OK;
    cfg->agent.llm_compiler_enabled =
        hu_json_get_bool(obj, "llm_compiler", cfg->agent.llm_compiler_enabled);
    cfg->agent.tree_of_thought =
        hu_json_get_bool(obj, "tree_of_thought", cfg->agent.tree_of_thought);
    cfg->agent.constitutional_ai =
        hu_json_get_bool(obj, "constitutional_ai", cfg->agent.constitutional_ai);
    cfg->agent.speculative_cache =
        hu_json_get_bool(obj, "speculative_cache", cfg->agent.speculative_cache);
    cfg->agent.tool_routing_enabled =
        hu_json_get_bool(obj, "tool_routing", cfg->agent.tool_routing_enabled);
    cfg->agent.multi_agent =
        hu_json_get_bool(obj, "multi_agent", cfg->agent.multi_agent);
    cfg->agent.compact_context =
        hu_json_get_bool(obj, "compact_context", cfg->agent.compact_context);
    double mti = hu_json_get_number(obj, "max_tool_iterations", cfg->agent.max_tool_iterations);
    if (mti >= 0 && mti <= 100000)
        cfg->agent.max_tool_iterations = (uint32_t)mti;
    double mhm = hu_json_get_number(obj, "max_history_messages", cfg->agent.max_history_messages);
    if (mhm >= 0 && mhm <= 10000)
        cfg->agent.max_history_messages = (uint32_t)mhm;
    cfg->agent.parallel_tools = hu_json_get_bool(obj, "parallel_tools", cfg->agent.parallel_tools);
    const char *td = hu_json_get_string(obj, "tool_dispatcher");
    if (td) {
        if (cfg->agent.tool_dispatcher)
            a->free(a->ctx, cfg->agent.tool_dispatcher, strlen(cfg->agent.tool_dispatcher) + 1);
        cfg->agent.tool_dispatcher = hu_strdup(a, td);
    }
    double tl = hu_json_get_number(obj, "token_limit", cfg->agent.token_limit);
    if (tl >= 0 && tl <= 2000000)
        cfg->agent.token_limit = (uint64_t)tl;
    double sids =
        hu_json_get_number(obj, "session_idle_timeout_secs", cfg->agent.session_idle_timeout_secs);
    if (sids >= 0)
        cfg->agent.session_idle_timeout_secs = (uint64_t)sids;
    double ckr =
        hu_json_get_number(obj, "compaction_keep_recent", cfg->agent.compaction_keep_recent);
    if (ckr >= 0 && ckr <= 200)
        cfg->agent.compaction_keep_recent = (uint32_t)ckr;
    double cms = hu_json_get_number(obj, "compaction_max_summary_chars",
                                    cfg->agent.compaction_max_summary_chars);
    if (cms >= 0 && cms <= 50000)
        cfg->agent.compaction_max_summary_chars = (uint32_t)cms;
    double cmx = hu_json_get_number(obj, "compaction_max_source_chars",
                                    cfg->agent.compaction_max_source_chars);
    if (cmx >= 0 && cmx <= 100000)
        cfg->agent.compaction_max_source_chars = (uint32_t)cmx;
    double mts = hu_json_get_number(obj, "message_timeout_secs", cfg->agent.message_timeout_secs);
    if (mts >= 0)
        cfg->agent.message_timeout_secs = (uint64_t)mts;
    double pmc = hu_json_get_number(obj, "pool_max_concurrent", cfg->agent.pool_max_concurrent);
    if (pmc >= 1 && pmc <= 64)
        cfg->agent.pool_max_concurrent = (uint32_t)pmc;
    const char *dp = hu_json_get_string(obj, "default_profile");
    if (dp) {
        if (cfg->agent.default_profile)
            a->free(a->ctx, cfg->agent.default_profile, strlen(cfg->agent.default_profile) + 1);
        cfg->agent.default_profile = hu_strdup(a, dp);
    }
    double cpw = hu_json_get_number(obj, "context_pressure_warn", cfg->agent.context_pressure_warn);
    if (cpw > 0.0 && cpw <= 1.0)
        cfg->agent.context_pressure_warn = (float)cpw;
    double cpc =
        hu_json_get_number(obj, "context_pressure_compact", cfg->agent.context_pressure_compact);
    if (cpc > 0.0 && cpc <= 1.0)
        cfg->agent.context_pressure_compact = (float)cpc;
    double cct =
        hu_json_get_number(obj, "context_compact_target", cfg->agent.context_compact_target);
    if (cct > 0.0 && cct <= 1.0)
        cfg->agent.context_compact_target = (float)cct;
    const char *persona = hu_json_get_string(obj, "persona");
    if (persona) {
        if (cfg->agent.persona)
            a->free(a->ctx, cfg->agent.persona, strlen(cfg->agent.persona) + 1);
        cfg->agent.persona = hu_strdup(a, persona);
    }

    hu_json_value_t *pc_obj = hu_json_object_get(obj, "persona_channels");
    if (pc_obj && pc_obj->type == HU_JSON_OBJECT && pc_obj->data.object.pairs) {
        if (cfg->agent.persona_channels) {
            for (size_t i = 0; i < cfg->agent.persona_channels_count; i++) {
                if (cfg->agent.persona_channels[i].channel)
                    a->free(a->ctx, cfg->agent.persona_channels[i].channel,
                            strlen(cfg->agent.persona_channels[i].channel) + 1);
                if (cfg->agent.persona_channels[i].persona)
                    a->free(a->ctx, cfg->agent.persona_channels[i].persona,
                            strlen(cfg->agent.persona_channels[i].persona) + 1);
            }
            a->free(a->ctx, cfg->agent.persona_channels,
                    cfg->agent.persona_channels_count * sizeof(hu_persona_channel_entry_t));
        }
        size_t n = pc_obj->data.object.len;
        if (n > 0) {
            hu_persona_channel_entry_t *arr = (hu_persona_channel_entry_t *)a->alloc(
                a->ctx, n * sizeof(hu_persona_channel_entry_t));
            if (arr) {
                memset(arr, 0, n * sizeof(hu_persona_channel_entry_t));
                size_t count = 0;
                for (size_t i = 0; i < n && count < n; i++) {
                    hu_json_pair_t *p = &pc_obj->data.object.pairs[i];
                    if (!p->key || !p->value || p->value->type != HU_JSON_STRING)
                        continue;
                    const char *ch_name = p->key;
                    const char *per_name = p->value->data.string.ptr;
                    if (!ch_name || !per_name)
                        continue;
                    arr[count].channel = hu_strdup(a, ch_name);
                    arr[count].persona = hu_strndup(a, per_name, p->value->data.string.len);
                    if (arr[count].channel && arr[count].persona)
                        count++;
                    else {
                        if (arr[count].channel)
                            a->free(a->ctx, arr[count].channel, strlen(arr[count].channel) + 1);
                        if (arr[count].persona)
                            a->free(a->ctx, arr[count].persona, strlen(arr[count].persona) + 1);
                    }
                }
                cfg->agent.persona_channels = arr;
                cfg->agent.persona_channels_count = count;
            }
        }
    }

    /* Constitutional AI configurable principles */
    hu_json_value_t *cp_arr = hu_json_object_get(obj, "constitutional_principles");
    if (cp_arr && cp_arr->type == HU_JSON_ARRAY && cp_arr->data.array.len > 0) {
        if (cfg->agent.constitutional_principles)
            a->free(a->ctx, cfg->agent.constitutional_principles,
                    strlen(cfg->agent.constitutional_principles) + 1);
        size_t buf_cap = 512;
        char *buf = (char *)a->alloc(a->ctx, buf_cap);
        if (buf) {
            size_t pos = 0;
            buf[0] = '\0';
            for (size_t i = 0; i < cp_arr->data.array.len && i < 16; i++) {
                hu_json_value_t *item = cp_arr->data.array.items[i];
                if (!item || item->type != HU_JSON_STRING)
                    continue;
                const char *principle = item->data.string.ptr;
                size_t plen = item->data.string.len;
                size_t needed = pos + plen + 8;
                if (needed >= buf_cap) {
                    size_t nc = buf_cap * 2;
                    while (nc < needed) nc *= 2;
                    char *nb = (char *)a->realloc(a->ctx, buf, buf_cap, nc);
                    if (!nb) break;
                    buf = nb;
                    buf_cap = nc;
                }
                int n = snprintf(buf + pos, buf_cap - pos, "- %.*s\n", (int)plen, principle);
                if (n > 0 && (size_t)n < buf_cap - pos)
                    pos += (size_t)n;
            }
            buf[pos] = '\0';
            cfg->agent.constitutional_principles = buf;
        }
    }

    hu_json_value_t *ct_obj = hu_json_object_get(obj, "persona_contacts");
    if (ct_obj && ct_obj->type == HU_JSON_OBJECT && ct_obj->data.object.pairs) {
        size_t n = ct_obj->data.object.len;
        if (n > 0) {
            hu_persona_channel_entry_t *arr = (hu_persona_channel_entry_t *)a->alloc(
                a->ctx, n * sizeof(hu_persona_channel_entry_t));
            if (arr) {
                memset(arr, 0, n * sizeof(hu_persona_channel_entry_t));
                size_t count = 0;
                for (size_t i = 0; i < n && count < n; i++) {
                    hu_json_pair_t *p = &ct_obj->data.object.pairs[i];
                    if (!p->key || !p->value || p->value->type != HU_JSON_STRING)
                        continue;
                    arr[count].channel = hu_strdup(a, p->key);
                    arr[count].persona =
                        hu_strndup(a, p->value->data.string.ptr, p->value->data.string.len);
                    if (arr[count].channel && arr[count].persona)
                        count++;
                    else {
                        if (arr[count].channel)
                            a->free(a->ctx, arr[count].channel, strlen(arr[count].channel) + 1);
                        if (arr[count].persona)
                            a->free(a->ctx, arr[count].persona, strlen(arr[count].persona) + 1);
                    }
                }
                cfg->agent.persona_contacts = arr;
                cfg->agent.persona_contacts_count = count;
            }
        }
    }

    hu_json_value_t *mr_obj = hu_json_object_get(obj, "model_router");
    if (mr_obj && mr_obj->type == HU_JSON_OBJECT) {
        const char *mr_ref = hu_json_get_string(mr_obj, "reflexive_model");
        if (mr_ref) {
            if (cfg->agent.mr_reflexive_model)
                a->free(a->ctx, cfg->agent.mr_reflexive_model,
                        strlen(cfg->agent.mr_reflexive_model) + 1);
            cfg->agent.mr_reflexive_model = hu_strdup(a, mr_ref);
        }
        const char *mr_conv = hu_json_get_string(mr_obj, "conversational_model");
        if (mr_conv) {
            if (cfg->agent.mr_conversational_model)
                a->free(a->ctx, cfg->agent.mr_conversational_model,
                        strlen(cfg->agent.mr_conversational_model) + 1);
            cfg->agent.mr_conversational_model = hu_strdup(a, mr_conv);
        }
        const char *mr_ana = hu_json_get_string(mr_obj, "analytical_model");
        if (mr_ana) {
            if (cfg->agent.mr_analytical_model)
                a->free(a->ctx, cfg->agent.mr_analytical_model,
                        strlen(cfg->agent.mr_analytical_model) + 1);
            cfg->agent.mr_analytical_model = hu_strdup(a, mr_ana);
        }
        const char *mr_deep = hu_json_get_string(mr_obj, "deep_model");
        if (mr_deep) {
            if (cfg->agent.mr_deep_model)
                a->free(a->ctx, cfg->agent.mr_deep_model,
                        strlen(cfg->agent.mr_deep_model) + 1);
            cfg->agent.mr_deep_model = hu_strdup(a, mr_deep);
        }
    }

    return HU_OK;
}
