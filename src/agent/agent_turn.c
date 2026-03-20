/* Core turn execution: hu_agent_turn and turn-local helpers */
#include "agent_internal.h"
#include "human/agent/ab_response.h"
#include "human/agent/awareness.h"
#include "human/agent/commands.h"
#include "human/agent/commitment.h"
#include "human/agent/commitment_store.h"
#include "human/agent/compaction.h"
#include "human/agent/dag.h"
#include "human/agent/dag_executor.h"
#include "human/agent/dispatcher.h"
#include "human/agent/input_guard.h"
#include "human/agent/llm_compiler.h"
#include "human/agent/mailbox.h"
#include "human/agent/memory_loader.h"
#include "human/agent/outcomes.h"
#include "human/agent/pattern_radar.h"
#include "human/agent/plan_executor.h"
#include "human/agent/planner.h"
#include "human/agent/preferences.h"
#include "human/agent/proactive.h"
#include "human/agent/prompt.h"
#include "human/agent/superhuman.h"
#include "human/agent/tool_call_parser.h"
#include "human/agent/tool_router.h"
#include "human/observability/bth_metrics.h"
#ifdef HU_HAS_PERSONA
#include "human/persona/circadian.h"
#include "human/persona/relationship.h"
#endif
#include "human/agent/orchestrator.h"
#include "human/agent/orchestrator_llm.h"
#include "human/agent/reflection.h"
#include "human/agent/swarm.h"
#include "human/skillforge.h"
#include "human/context.h"
#include "human/context/conversation.h"
#include "human/context_tokens.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/memory/deep_extract.h"
#include "human/memory/emotional_moments.h"
#include "human/memory/fast_capture.h"
#include "human/memory/stm.h"
#include "human/memory/tiers.h"
#include "human/memory/superhuman.h"
#ifdef HU_HAS_PERSONA
#include "human/persona.h"
#endif
#include "human/cost.h"
#include "human/provider.h"
#include "human/voice.h"
#ifdef HU_ENABLE_SQLITE
#include "human/intelligence/self_improve.h"
#include "human/intelligence/online_learning.h"
#include "human/intelligence/skills_context.h"
#include "human/intelligence/value_learning.h"
#include "human/intelligence/world_model.h"
#include "human/experience.h"
#include "human/agent/goals.h"
#include "human/memory.h"
#include "human/memory/contact_graph.h"
#include <sqlite3.h>
#endif
#include "human/agent/constitutional.h"
#include "human/agent/speculative.h"
#include "human/agent/tree_of_thought.h"
#include "human/agent/uncertainty.h"
#if HU_HAS_PWA
#include "human/pwa_context.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if (defined(__unix__) || defined(__APPLE__)) && !defined(HU_IS_TEST)
#include <pthread.h>

typedef struct dag_parallel_work {
    hu_agent_t *agent;
    hu_dag_node_t *node;
    hu_dag_t *dag;
} dag_parallel_work_t;

static void *dag_parallel_worker(void *arg) {
    dag_parallel_work_t *w = (dag_parallel_work_t *)arg;
    hu_dag_node_t *node = w->node;
    node->status = HU_DAG_RUNNING;

    char *resolved_args = NULL;
    size_t resolved_len = 0;
    const char *use_args = node->args_json;
    if (node->args_json) {
        if (hu_dag_resolve_vars(w->agent->alloc, w->dag, node->args_json,
                strlen(node->args_json), &resolved_args, &resolved_len) == HU_OK && resolved_args)
            use_args = resolved_args;
    }

    hu_tool_t *dag_tool = hu_agent_internal_find_tool(
        w->agent, node->tool_name, node->tool_name ? strlen(node->tool_name) : 0);
    if (!dag_tool) {
        node->status = HU_DAG_FAILED;
        if (resolved_args)
            w->agent->alloc->free(w->agent->alloc->ctx, resolved_args, resolved_len + 1);
        return NULL;
    }

    hu_tool_result_t dag_result = hu_tool_result_fail("invalid", 7);
    hu_json_value_t *dag_args = NULL;
    if (use_args) {
        hu_error_t jerr = hu_json_parse(w->agent->alloc, use_args, strlen(use_args), &dag_args);
        if (jerr != HU_OK)
            fprintf(stderr, "[agent_turn] DAG tool args parse failed\n");
    }
    if (dag_args && dag_tool->vtable->execute)
        dag_tool->vtable->execute(dag_tool->ctx, w->agent->alloc, dag_args, &dag_result);
    if (dag_args)
        hu_json_free(w->agent->alloc, dag_args);
    if (resolved_args)
        w->agent->alloc->free(w->agent->alloc->ctx, resolved_args, resolved_len + 1);

    if (dag_result.success) {
        node->status = HU_DAG_DONE;
        if (dag_result.output && dag_result.output_len > 0) {
            node->result = hu_strndup(w->agent->alloc, dag_result.output, dag_result.output_len);
            node->result_len = dag_result.output_len;
        }
    } else {
        node->status = HU_DAG_FAILED;
    }
    hu_tool_result_free(w->agent->alloc, &dag_result);
    return NULL;
}
#endif

static bool message_looks_multistep_for_orchestrator(const char *m, size_t mlen) {
    if (!m || mlen < 48)
        return false;
    static const char *const needles[] = {
        " first ", " then ", " finally", " step ", " steps ", "step 1", "step 2",
        "\n1.", "\n2.", "1) ", "2) ",
    };
    for (size_t ni = 0; ni < sizeof(needles) / sizeof(needles[0]); ni++) {
        const char *n = needles[ni];
        size_t nl = strlen(n);
        if (nl > mlen)
            continue;
        for (size_t j = 0; j + nl <= mlen; j++) {
            if (memcmp(m + j, n, nl) == 0)
                return true;
        }
    }
    return false;
}

hu_error_t hu_agent_turn(hu_agent_t *agent, const char *msg, size_t msg_len, char **response_out,
                         size_t *response_len_out) {
    if (!agent || !msg || !response_out)
        return HU_ERR_INVALID_ARGUMENT;
    *response_out = NULL;
    if (response_len_out)
        *response_len_out = 0;

    hu_agent_set_current_for_tools(agent);

    /* Speculative cache: check for pre-computed response */
    if (agent->speculative_cache) {
        hu_speculative_config_t spec_cfg = hu_speculative_config_default();
        hu_prediction_t *hit = NULL;
        int64_t now = (int64_t)time(NULL);
        if (hu_speculative_cache_lookup(agent->speculative_cache, msg, msg_len,
                                         now, &spec_cfg, &hit) == HU_OK && hit) {
            *response_out = hu_strndup(agent->alloc, hit->response, hit->response_len);
            if (*response_out) {
                if (response_len_out)
                    *response_len_out = hit->response_len;
                hu_agent_clear_current_for_tools();
                return HU_OK;
            }
        }
    }

    hu_agent_internal_process_mailbox_messages(agent);

    char *slash_resp = hu_agent_handle_slash_command(agent, msg, msg_len);
    if (slash_resp) {
        hu_agent_clear_current_for_tools();
        *response_out = slash_resp;
        if (response_len_out)
            *response_len_out = strlen(slash_resp);
        return HU_OK;
    }

    /* Prompt injection defense-in-depth */
    {
        hu_injection_risk_t risk = HU_INJECTION_SAFE;
        hu_error_t guard_err = hu_input_guard_check(msg, msg_len, &risk);
        if (guard_err != HU_OK) {
            hu_agent_clear_current_for_tools();
            return guard_err;
        }
        if (risk == HU_INJECTION_HIGH_RISK) {
            if (agent->observer) {
                hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_ERR};
                ev.data.err.component = "input_guard";
                ev.data.err.message = "high-risk injection pattern detected";
                hu_observer_record_event(*agent->observer, &ev);
            }
            *response_out = hu_strndup(agent->alloc,
                "I can't process that request due to safety concerns.", 52);
            if (response_len_out)
                *response_len_out = 52;
            hu_agent_clear_current_for_tools();
            return HU_OK;
        }
    }

    /* Automatic planning + execution for complex tasks */
    char *plan_ctx = NULL;
    size_t plan_ctx_len = 0;
#ifndef HU_IS_TEST
    if (agent->history_count > 0) {
        size_t scan_n = agent->history_count < 10 ? agent->history_count : 10;
        for (size_t k = 0; k < scan_n; k++) {
            size_t hi = agent->history_count - 1 - k;
            if (agent->history[hi].role != HU_ROLE_SYSTEM || !agent->history[hi].content)
                continue;
            const char *hc = agent->history[hi].content;
            if (strncmp(hc, "[ACTIVE_PLAN]", 13) != 0)
                continue;
            if (plan_ctx == NULL) {
                size_t clen = strlen(hc);
                plan_ctx = hu_strndup(agent->alloc, hc, clen);
                if (plan_ctx)
                    plan_ctx_len = clen;
            }
            break;
        }
    }
    if (msg_len > 200 && agent->tools_count >= 5 && agent->provider.vtable &&
        agent->provider.vtable->chat) {
        const char *tool_names[32];
        size_t tn_count = 0;
        for (size_t ti = 0; ti < agent->tools_count && tn_count < 32; ti++) {
            if (agent->tools[ti].vtable && agent->tools[ti].vtable->name)
                tool_names[tn_count++] = agent->tools[ti].vtable->name(agent->tools[ti].ctx);
        }
        hu_plan_t *plan = NULL;
        if (hu_planner_generate(agent->alloc, &agent->provider, agent->model_name,
                                 agent->model_name_len, msg, msg_len,
                                 tool_names, tn_count, &plan) == HU_OK && plan) {
            /* MCTS refinement for complex plans */
            if (agent->mcts_planner_enabled && plan &&
                hu_planner_plan_needs_mcts(plan)) {
                hu_plan_t *mcts_plan = NULL;
                if (hu_planner_plan_mcts(agent->alloc, &agent->provider,
                                         agent->model_name, agent->model_name_len,
                                         msg, msg_len, tool_names, tn_count,
                                         &mcts_plan) == HU_OK && mcts_plan) {
                    hu_plan_free(agent->alloc, plan);
                    plan = mcts_plan;
                }
            }
            hu_plan_executor_t exec;
            hu_plan_executor_init(&exec, agent->alloc, &agent->provider,
                                  agent->model_name, agent->model_name_len,
                                  agent->tools, agent->tools_count);
            hu_plan_exec_result_t exec_result;
            memset(&exec_result, 0, sizeof(exec_result));
            hu_error_t pe = hu_plan_executor_run(&exec, plan, msg, msg_len, &exec_result);
            if (pe == HU_OK && exec_result.summary_len > 0) {
                plan_ctx = hu_strndup(agent->alloc, exec_result.summary, exec_result.summary_len);
                plan_ctx_len = exec_result.summary_len;
            } else {
                char plan_buf[1024];
                int pn = snprintf(plan_buf, sizeof(plan_buf),
                                  "[PLAN]: %zu steps planned, %zu completed",
                                  plan->steps_count, exec_result.steps_completed);
                if (pn > 0 && (size_t)pn < sizeof(plan_buf)) {
                    plan_ctx = hu_strndup(agent->alloc, plan_buf, (size_t)pn);
                    plan_ctx_len = (size_t)pn;
                }
            }
            if (plan && exec_result.steps_completed < plan->steps_count) {
                char plan_mem[2048];
                int pm = snprintf(plan_mem, sizeof(plan_mem),
                                  "[ACTIVE_PLAN] %zu/%zu steps completed. Remaining: ",
                                  exec_result.steps_completed, plan->steps_count);
                for (size_t si = exec_result.steps_completed;
                     si < plan->steps_count && pm > 0 && (size_t)pm < sizeof(plan_mem) - 128;
                     si++) {
                    const char *desc = plan->steps[si].description;
                    size_t dlen = desc ? strlen(desc) : 0;
                    if (dlen > 100)
                        dlen = 100;
                    int add = snprintf(plan_mem + (size_t)pm, sizeof(plan_mem) - (size_t)pm,
                                       "\n  Step %zu: %.*s", si + 1, (int)dlen, desc ? desc : "");
                    if (add > 0)
                        pm += add;
                }
                if (pm > 0 && (size_t)pm < sizeof(plan_mem)) {
                    (void)hu_agent_internal_append_history(agent, HU_ROLE_SYSTEM, plan_mem,
                                                            (size_t)pm, NULL, 0, NULL, 0);
                }
            }
            hu_plan_free(agent->alloc, plan);
        }
    }
#endif

    hu_error_t err =
        hu_agent_internal_append_history(agent, HU_ROLE_USER, msg, msg_len, NULL, 0, NULL, 0);
    if (err != HU_OK) {
        if (plan_ctx)
            agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
        hu_agent_clear_current_for_tools();
        return err;
    }

    /* Superhuman: observe user message (emotional, silence services) */
    (void)hu_superhuman_observe_all(&agent->superhuman, agent->alloc, msg, msg_len, "user", 4);

    /* Fast-capture and STM: extract entities/emotions, record turn, populate last turn */
    {
        hu_fc_result_t fc_result;
        memset(&fc_result, 0, sizeof(fc_result));
        (void)hu_fast_capture(agent->alloc, msg, msg_len, &fc_result);

        uint64_t ts_ms = (uint64_t)time(NULL) * 1000;
        err = hu_stm_record_turn(&agent->stm, "user", 4, msg, msg_len, ts_ms);
        if (err == HU_OK) {
            size_t last_idx = hu_stm_count(&agent->stm) - 1;
            if (fc_result.primary_topic && fc_result.primary_topic[0]) {
                (void)hu_stm_turn_set_primary_topic(&agent->stm, last_idx, fc_result.primary_topic,
                                                    strlen(fc_result.primary_topic));
            }
            for (size_t i = 0; i < fc_result.entity_count; i++) {
                const hu_fc_entity_match_t *e = &fc_result.entities[i];
                uint32_t mention = 1;
                (void)hu_stm_turn_add_entity(&agent->stm, last_idx, e->name, e->name_len,
                                             e->type ? e->type : "entity",
                                             e->type ? e->type_len : 6, mention);
            }
            for (size_t i = 0; i < fc_result.emotion_count; i++) {
                (void)hu_stm_turn_add_emotion(&agent->stm, last_idx, fc_result.emotions[i].tag,
                                              fc_result.emotions[i].intensity);
            }
        }

        /* Pattern radar: observe entities as topic recurrence, emotions as emotional trend */
        {
            char ts_buf[32];
            int ts_n = snprintf(ts_buf, sizeof(ts_buf), "%llu", (unsigned long long)(ts_ms / 1000));
            const char *ts = ts_n > 0 ? ts_buf : NULL;
            size_t ts_len = (ts_n > 0 && ts_n < (int)sizeof(ts_buf)) ? (size_t)ts_n : 0;

            for (size_t i = 0; i < fc_result.entity_count; i++) {
                const hu_fc_entity_match_t *e = &fc_result.entities[i];
                if (e->name && e->name_len > 0) {
                    (void)hu_pattern_radar_observe(
                        &agent->radar, e->name, e->name_len, HU_PATTERN_TOPIC_RECURRENCE,
                        e->type ? e->type : NULL, e->type ? e->type_len : 0, ts, ts_len);
                }
            }
            static const char *emotion_names[] = {"neutral",     "joy",        "sadness",
                                                  "anger",       "fear",       "surprise",
                                                  "frustration", "excitement", "anxiety"};
            for (size_t i = 0; i < fc_result.emotion_count; i++) {
                hu_emotion_tag_t tag = fc_result.emotions[i].tag;
                if (tag >= 0 && (size_t)tag < sizeof(emotion_names) / sizeof(emotion_names[0])) {
                    const char *name = emotion_names[tag];
                    (void)hu_pattern_radar_observe(&agent->radar, name, strlen(name),
                                                   HU_PATTERN_EMOTIONAL_TREND, NULL, 0, ts, ts_len);
                }
            }
        }
        hu_fc_result_deinit(&fc_result, agent->alloc);
    }

    /* Commitment detection: extract promises, intentions, reminders, goals from user message */
    if (agent->commitment_store) {
        hu_commitment_detect_result_t commit_result;
        memset(&commit_result, 0, sizeof(commit_result));
        hu_error_t cerr =
            hu_commitment_detect(agent->alloc, msg, msg_len, "user", 4, &commit_result);
        if (cerr == HU_OK && commit_result.count > 0) {
            const char *sess = agent->memory_session_id;
            size_t sess_len = agent->memory_session_id ? agent->memory_session_id_len : 0;
            for (size_t i = 0; i < commit_result.count; i++) {
                hu_error_t cs_err = hu_commitment_store_save(agent->commitment_store,
                                                              &commit_result.commitments[i],
                                                              sess, sess_len);
                if (cs_err != HU_OK)
                    fprintf(stderr, "[agent] commitment save failed: %d\n", (int)cs_err);
            }
        }
        hu_commitment_detect_result_deinit(&commit_result, agent->alloc);
    }

    /* Detect preferences from user corrections and store them */
    bool is_correction = hu_preferences_is_correction(msg, msg_len);
    if (agent->memory && is_correction) {
        size_t pref_len = 0;
        char *pref = hu_preferences_extract(agent->alloc, msg, msg_len, &pref_len);
        if (pref) {
            hu_preferences_store(agent->memory, agent->alloc, pref, pref_len);
            agent->alloc->free(agent->alloc->ctx, pref, pref_len + 1);
        }
    }

    /* Outcome tracking: record corrections and positive feedback */
    if (agent->outcomes) {
        if (is_correction) {
            const char *prev_response = NULL;
            if (agent->history_count >= 2 &&
                agent->history[agent->history_count - 2].role == HU_ROLE_ASSISTANT)
                prev_response = agent->history[agent->history_count - 2].content;
            hu_outcome_record_correction(agent->outcomes, prev_response, msg);

#ifdef HU_HAS_PERSONA
            if (agent->outcomes->auto_apply_feedback && agent->persona && agent->persona_name &&
                prev_response) {
                hu_persona_feedback_t fb = {
                    .channel = agent->active_channel,
                    .channel_len = agent->active_channel_len,
                    .original_response = prev_response,
                    .original_response_len = strlen(prev_response),
                    .corrected_response = msg,
                    .corrected_response_len = msg_len,
                };
                (void)hu_persona_feedback_record(agent->alloc, agent->persona_name,
                                                 strlen(agent->persona_name), &fb);
            }
#endif
        } else if (msg_len >= 5 && msg_len <= 80) {
            /* Detect simple positive feedback */
            bool positive = false;
            for (size_t k = 0; k + 5 <= msg_len && !positive; k++) {
                char c0 = msg[k] | 0x20, c1 = msg[k + 1] | 0x20, c2 = msg[k + 2] | 0x20;
                char c3 = msg[k + 3] | 0x20, c4 = msg[k + 4] | 0x20;
                if (c0 == 't' && c1 == 'h' && c2 == 'a' && c3 == 'n' && c4 == 'k')
                    positive = true;
                if (c0 == 'g' && c1 == 'r' && c2 == 'e' && c3 == 'a' && c4 == 't')
                    positive = true;
                if (k + 6 <= msg_len && c0 == 'p' && c1 == 'e' && c2 == 'r' && c3 == 'f' &&
                    c4 == 'e' && (msg[k + 5] | 0x20) == 'c')
                    positive = true;
            }
            if (positive)
                hu_outcome_record_positive(agent->outcomes, msg);
        }
    }

    /* Detect tone from recent user messages */
    const char *tone_hint = NULL;
    size_t tone_hint_len = 0;
    {
        const char *recent_msgs[3];
        size_t recent_lens[3];
        size_t rm_count = 0;
        for (size_t i = agent->history_count; i > 0 && rm_count < 3; i--) {
            if (agent->history[i - 1].role == HU_ROLE_USER && agent->history[i - 1].content) {
                recent_msgs[rm_count] = agent->history[i - 1].content;
                recent_lens[rm_count] = agent->history[i - 1].content_len;
                rm_count++;
            }
        }
        if (rm_count > 0) {
            hu_tone_t tone = hu_detect_tone(recent_msgs, recent_lens, rm_count);
            tone_hint = hu_tone_hint_string(tone, &tone_hint_len);
        }
    }

    /* Load user preferences for prompt injection */
    char *pref_ctx = NULL;
    size_t pref_ctx_len = 0;
    if (agent->memory) {
        hu_error_t pref_err = hu_preferences_load(agent->memory, agent->alloc, &pref_ctx, &pref_ctx_len);
        if (pref_err != HU_OK)
            fprintf(stderr, "[agent_turn] preferences load failed: %d\n", pref_err);
    }

    /* Self-RAG gate: decide whether retrieval is needed before loading memory */
    hu_srag_assessment_t srag_assessment;
    memset(&srag_assessment, 0, sizeof(srag_assessment));
    bool srag_skip_retrieval = false;
    if (agent->sota_initialized && agent->srag_config.enabled) {
        hu_srag_should_retrieve(agent->alloc, &agent->srag_config, msg, msg_len,
                                NULL, 0, &srag_assessment);
        if (srag_assessment.decision == HU_SRAG_NO_RETRIEVAL)
            srag_skip_retrieval = true;
    }

    /* Load memory context for this turn (gated by Self-RAG) */
    char *memory_ctx = NULL;
    size_t memory_ctx_len = 0;
    if (agent->memory && agent->memory->vtable && !srag_skip_retrieval) {
        hu_memory_loader_t loader;
        hu_memory_loader_init(&loader, agent->alloc, agent->memory, agent->retrieval_engine, 10,
                              4000);
        hu_error_t load_err = hu_memory_loader_load(&loader, msg, msg_len,
                                    agent->memory_session_id ? agent->memory_session_id : "",
                                    agent->memory_session_id ? agent->memory_session_id_len : 0,
                                    &memory_ctx, &memory_ctx_len);
        if (load_err != HU_OK)
            fprintf(stderr, "[agent_turn] memory loader failed: %d\n", load_err);

        /* Self-RAG: verify relevance of retrieved content */
        if (srag_assessment.decision == HU_SRAG_RETRIEVE_AND_VERIFY && memory_ctx && memory_ctx_len > 0) {
            double relevance = 0.0;
            bool should_use = false;
            hu_srag_verify_relevance(agent->alloc, &agent->srag_config, msg, msg_len,
                                     memory_ctx, memory_ctx_len, &relevance, &should_use);
            if (!should_use) {
                agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
                memory_ctx = NULL;
                memory_ctx_len = 0;
            }
        }
    }

    /* Adaptive RAG: select strategy and record for learning */
    hu_rag_strategy_t rag_strategy_used = HU_RAG_NONE;
    if (agent->sota_initialized && !srag_skip_retrieval) {
        rag_strategy_used = hu_adaptive_rag_select(&agent->adaptive_rag, msg, msg_len);
    }

#ifdef HU_ENABLE_SQLITE
    /* Load contact-scoped memories */
    if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0) {
        hu_memory_entry_t *contact_entries = NULL;
        size_t contact_count = 0;
        if (hu_memory_recall_for_contact(agent->memory, agent->alloc,
                agent->memory_session_id, agent->memory_session_id_len,
                msg, msg_len, 5, "", 0, &contact_entries, &contact_count) == HU_OK &&
            contact_entries && contact_count > 0) {
            size_t extra_len = 0;
            for (size_t i = 0; i < contact_count; i++)
                extra_len += contact_entries[i].content_len + 1;
            if (extra_len > 0) {
                size_t old_len = memory_ctx ? memory_ctx_len : 0;
                size_t new_total = old_len + (old_len > 0 ? 2 : 0) + extra_len + 32;
                char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, new_total);
                if (merged) {
                    size_t pos = 0;
                    if (memory_ctx && memory_ctx_len > 0) {
                        memcpy(merged, memory_ctx, memory_ctx_len);
                        pos = memory_ctx_len;
                        merged[pos++] = '\n';
                        merged[pos++] = '\n';
                    }
                    int n = snprintf(merged + pos, new_total - pos, "[About this contact]\n");
                    if (n > 0)
                        pos += (size_t)n;
                    for (size_t i = 0; i < contact_count && pos < new_total - 1; i++) {
                        size_t to_copy = contact_entries[i].content_len;
                        if (pos + to_copy + 1 > new_total)
                            to_copy = new_total - pos - 1;
                        memcpy(merged + pos, contact_entries[i].content, to_copy);
                        pos += to_copy;
                        merged[pos++] = '\n';
                    }
                    merged[pos] = '\0';
                    if (memory_ctx)
                        agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
                    memory_ctx = merged;
                    memory_ctx_len = pos;
                }
            }
            for (size_t i = 0; i < contact_count; i++)
                hu_memory_entry_free_fields(agent->alloc, &contact_entries[i]);
            agent->alloc->free(agent->alloc->ctx, contact_entries,
                               contact_count * sizeof(hu_memory_entry_t));
        }
    }
#endif

    /* Build STM context for this turn */
    char *stm_ctx = NULL;
    size_t stm_ctx_len = 0;
    hu_error_t stm_err = hu_stm_build_context(&agent->stm, agent->alloc, &stm_ctx, &stm_ctx_len);
    if (stm_err != HU_OK)
        fprintf(stderr, "[agent_turn] STM context build failed: %d\n", stm_err);
    if (stm_ctx_len > 0 && agent->bth_metrics)
        agent->bth_metrics->emotions_surfaced++;

    /* Build commitment context for this turn */
    char *commitment_ctx = NULL;
    size_t commitment_ctx_len = 0;
    if (agent->commitment_store) {
        const char *sess = agent->memory_session_id;
        size_t sess_len = agent->memory_session_id ? agent->memory_session_id_len : 0;
        (void)hu_commitment_store_build_context(agent->commitment_store, agent->alloc, sess,
                                                sess_len, &commitment_ctx, &commitment_ctx_len);
        if (commitment_ctx_len > 0 && agent->bth_metrics)
            agent->bth_metrics->commitment_followups++;
    }

    /* Build pattern radar context for this turn */
    char *pattern_ctx = NULL;
    size_t pattern_ctx_len = 0;
    (void)hu_pattern_radar_build_context(&agent->radar, agent->alloc, &pattern_ctx,
                                         &pattern_ctx_len);
    if (pattern_ctx_len > 0 && agent->bth_metrics)
        agent->bth_metrics->pattern_insights++;

    /* Build proactive context (milestones, morning briefing, check-in) */
    char *proactive_ctx = NULL;
    size_t proactive_ctx_len = 0;
    {
        uint32_t session_count = 0;
        uint8_t hour = 10;
#ifdef HU_HAS_PERSONA
        session_count = agent->relationship.session_count;
#endif
#ifndef HU_IS_TEST
        {
            time_t now = time(NULL);
            struct tm lt_buf;
            struct tm *lt = localtime_r(&now, &lt_buf);
            if (lt)
                hour = (uint8_t)(lt->tm_hour & 0xFF);
        }
#endif
        hu_proactive_result_t proactive_result;
        memset(&proactive_result, 0, sizeof(proactive_result));
        hu_commitment_t *commitments = NULL;
        size_t commitment_count = 0;
        if (agent->commitment_store && agent->memory_session_id &&
            agent->memory_session_id_len > 0) {
            hu_error_t commit_err = hu_commitment_store_list_active(
                agent->commitment_store, agent->alloc, agent->memory_session_id,
                agent->memory_session_id_len, &commitments, &commitment_count);
            if (commit_err != HU_OK)
                fprintf(stderr, "[agent_turn] commitment list failed: %d\n", commit_err);
        }
        hu_error_t proactive_err =
            hu_proactive_check_extended(agent->alloc, session_count, hour, commitments,
                                        commitment_count, NULL, NULL, 0, &proactive_result);
        if (commitments) {
            for (size_t ci = 0; ci < commitment_count; ci++)
                hu_commitment_deinit(&commitments[ci], agent->alloc);
            agent->alloc->free(agent->alloc->ctx, commitments,
                               commitment_count * sizeof(hu_commitment_t));
        }
        if (proactive_err == HU_OK && proactive_result.count > 0) {
            (void)hu_proactive_build_context(&proactive_result, agent->alloc, 8, &proactive_ctx,
                                             &proactive_ctx_len);
            hu_proactive_result_deinit(&proactive_result, agent->alloc);
        }
        /* Merge contextual conversation starter from memory when we have a contact */
        if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0) {
            char *starter = NULL;
            size_t starter_len = 0;
            if (hu_proactive_build_starter(agent->alloc, agent->memory, agent->memory_session_id,
                                           agent->memory_session_id_len, &starter,
                                           &starter_len) == HU_OK &&
                starter && starter_len > 0) {
                if (proactive_ctx && proactive_ctx_len > 0) {
                    size_t merged_len = proactive_ctx_len + 2 + starter_len;
                    char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, merged_len + 1);
                    if (merged) {
                        memcpy(merged, proactive_ctx, proactive_ctx_len);
                        merged[proactive_ctx_len] = '\n';
                        merged[proactive_ctx_len + 1] = '\n';
                        memcpy(merged + proactive_ctx_len + 2, starter, starter_len);
                        merged[merged_len] = '\0';
                        agent->alloc->free(agent->alloc->ctx, proactive_ctx, proactive_ctx_len + 1);
                        agent->alloc->free(agent->alloc->ctx, starter, starter_len + 1);
                        proactive_ctx = merged;
                        proactive_ctx_len = merged_len;
                    } else {
                        agent->alloc->free(agent->alloc->ctx, starter, starter_len + 1);
                    }
                } else {
                    proactive_ctx = starter;
                    proactive_ctx_len = starter_len;
                }
                if (agent->bth_metrics)
                    agent->bth_metrics->starters_built++;
            }
        }
    }

    /* Build superhuman context (commitment, predictive, emotional, silence) */
    char *superhuman_ctx = NULL;
    size_t superhuman_ctx_len = 0;
    {
        agent->superhuman_commitment_ctx.session_id = agent->memory_session_id;
        agent->superhuman_commitment_ctx.session_id_len = agent->memory_session_id_len;
        (void)hu_superhuman_build_context(&agent->superhuman, agent->alloc, &superhuman_ctx,
                                          &superhuman_ctx_len);
#ifdef HU_ENABLE_SQLITE
        /* Superhuman memory: micro-moments, inside jokes, avoidance, topic absences, growth, patterns */
        if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0) {
            bool include_avoidance = false;
#ifdef HU_HAS_PERSONA
            include_avoidance =
                (agent->relationship.stage == HU_REL_TRUSTED ||
                 agent->relationship.stage == HU_REL_DEEP);
#endif
            char *memory_sh_ctx = NULL;
            size_t memory_sh_len = 0;
            if (hu_superhuman_memory_build_context(agent->memory, agent->alloc,
                    agent->memory_session_id, agent->memory_session_id_len, include_avoidance,
                    &memory_sh_ctx, &memory_sh_len) == HU_OK &&
                memory_sh_ctx && memory_sh_len > 0) {
                if (superhuman_ctx && superhuman_ctx_len > 0) {
                    size_t merged_len = superhuman_ctx_len + 2 + strlen(memory_sh_ctx) + 1;
                    char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, merged_len);
                    if (merged) {
                        memcpy(merged, superhuman_ctx, superhuman_ctx_len);
                        merged[superhuman_ctx_len] = '\n';
                        merged[superhuman_ctx_len + 1] = '\n';
                        memcpy(merged + superhuman_ctx_len + 2, memory_sh_ctx,
                            strlen(memory_sh_ctx) + 1);
                        agent->alloc->free(agent->alloc->ctx, superhuman_ctx,
                            superhuman_ctx_len);
                        agent->alloc->free(agent->alloc->ctx, memory_sh_ctx, memory_sh_len);
                        superhuman_ctx = merged;
                        superhuman_ctx_len = merged_len;
                    } else {
                        agent->alloc->free(agent->alloc->ctx, memory_sh_ctx, memory_sh_len);
                    }
                } else {
                    superhuman_ctx = memory_sh_ctx;
                    superhuman_ctx_len = memory_sh_len;
                }
            } else if (memory_sh_ctx) {
                agent->alloc->free(agent->alloc->ctx, memory_sh_ctx, memory_sh_len);
            }
        }
        /* Per-contact style evolution guidance */
        if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0) {
            char *style_guidance = NULL;
            size_t style_guidance_len = 0;
            if (hu_superhuman_style_build_guidance(agent->memory, agent->alloc,
                    agent->memory_session_id, agent->memory_session_id_len,
                    &style_guidance, &style_guidance_len) == HU_OK &&
                style_guidance && style_guidance_len > 0) {
                if (superhuman_ctx && superhuman_ctx_len > 0) {
                    size_t merged_len = superhuman_ctx_len + 1 + style_guidance_len + 1;
                    char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, merged_len);
                    if (merged) {
                        memcpy(merged, superhuman_ctx, superhuman_ctx_len);
                        merged[superhuman_ctx_len] = '\n';
                        memcpy(merged + superhuman_ctx_len + 1, style_guidance, style_guidance_len);
                        merged[merged_len - 1] = '\0';
                        agent->alloc->free(agent->alloc->ctx, superhuman_ctx, superhuman_ctx_len);
                        agent->alloc->free(agent->alloc->ctx, style_guidance, style_guidance_len + 1);
                        superhuman_ctx = merged;
                        superhuman_ctx_len = merged_len;
                    } else {
                        agent->alloc->free(agent->alloc->ctx, style_guidance, style_guidance_len + 1);
                    }
                } else {
                    superhuman_ctx = style_guidance;
                    superhuman_ctx_len = style_guidance_len;
                }
            } else if (style_guidance) {
                agent->alloc->free(agent->alloc->ctx, style_guidance, style_guidance_len + 1);
            }
        }
        /* Cross-channel identity: merge canonical contact id from contact graph into superhuman ctx */
        if (agent->memory && agent->active_channel && agent->active_channel_len > 0 &&
            agent->memory_session_id && agent->memory_session_id_len > 0) {
            sqlite3 *cg_db = hu_sqlite_memory_get_db(agent->memory);
            if (cg_db) {
                char plat[64];
                char handle[256];
                size_t pl = agent->active_channel_len < sizeof(plat) - 1 ? agent->active_channel_len
                                                                       : sizeof(plat) - 1;
                memcpy(plat, agent->active_channel, pl);
                plat[pl] = '\0';
                size_t hl = agent->memory_session_id_len < sizeof(handle) - 1
                                ? agent->memory_session_id_len
                                : sizeof(handle) - 1;
                memcpy(handle, agent->memory_session_id, hl);
                handle[hl] = '\0';
                char canon[128];
                if (hu_contact_graph_resolve(cg_db, plat, handle, canon, sizeof(canon)) == HU_OK) {
                    char line[320];
                    int nw = snprintf(line, sizeof(line), "Cross-channel identity (canonical): %s", canon);
                    if (nw > 0 && (size_t)nw < sizeof(line)) {
                        size_t line_len = (size_t)nw;
                        if (superhuman_ctx && superhuman_ctx_len > 0) {
                            size_t merged_len = superhuman_ctx_len + 1 + line_len + 1;
                            char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, merged_len);
                            if (merged) {
                                memcpy(merged, superhuman_ctx, superhuman_ctx_len);
                                merged[superhuman_ctx_len] = '\n';
                                memcpy(merged + superhuman_ctx_len + 1, line, line_len);
                                merged[merged_len - 1] = '\0';
                                agent->alloc->free(agent->alloc->ctx, superhuman_ctx, superhuman_ctx_len);
                                superhuman_ctx = merged;
                                superhuman_ctx_len = merged_len;
                            }
                        } else {
                            char *dup = (char *)agent->alloc->alloc(agent->alloc->ctx, line_len + 1);
                            if (dup) {
                                memcpy(dup, line, line_len);
                                dup[line_len] = '\0';
                                superhuman_ctx = dup;
                                superhuman_ctx_len = line_len + 1;
                            }
                        }
                    }
                }
            }
        }
        /* Emotional moments due for check-in (contact-scoped) */
        if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0) {
            hu_emotional_moment_t *due = NULL;
            size_t due_count = 0;
            int64_t now_ts = (int64_t)time(NULL);
            if (hu_emotional_moment_get_due(agent->alloc, agent->memory, now_ts, &due,
                                            &due_count) == HU_OK &&
                due && due_count > 0) {
                size_t contact_due = 0;
                for (size_t d = 0; d < due_count; d++) {
                    bool match = (strcmp(due[d].contact_id, agent->memory_session_id) == 0);
                    if (!match) {
                        const char *colon = strchr(agent->memory_session_id, ':');
                        if (colon && strcmp(due[d].contact_id, colon + 1) == 0)
                            match = true;
                    }
                    if (match)
                        contact_due++;
                }
                if (contact_due > 0) {
                    size_t em_len = 64 + contact_due * 128;
                    char *em_ctx = (char *)agent->alloc->alloc(agent->alloc->ctx, em_len);
                    if (em_ctx) {
                        size_t pos = 0;
                        int n = snprintf(em_ctx + pos, em_len - pos,
                                        "[Emotional check-in due] They shared something difficult "
                                        "1–3 days ago. Consider a natural check-in:\n");
                        if (n > 0)
                            pos += (size_t)n;
                        for (size_t d = 0; d < due_count && pos < em_len - 1; d++) {
                            bool match =
                                (strcmp(due[d].contact_id, agent->memory_session_id) == 0);
                            if (!match) {
                                const char *colon = strchr(agent->memory_session_id, ':');
                                if (colon && strcmp(due[d].contact_id, colon + 1) == 0)
                                    match = true;
                            }
                            if (match) {
                                n = snprintf(em_ctx + pos, em_len - pos,
                                             "- Topic: %s, emotion: %s\n", due[d].topic,
                                             due[d].emotion);
                                if (n > 0)
                                    pos += (size_t)n;
                            }
                        }
                        em_ctx[pos] = '\0';
                        if (pos > 0 && superhuman_ctx) {
                            size_t merged_len = superhuman_ctx_len + 2 + pos + 1;
                            char *merged =
                                (char *)agent->alloc->alloc(agent->alloc->ctx, merged_len);
                            if (merged) {
                                memcpy(merged, superhuman_ctx, superhuman_ctx_len);
                                merged[superhuman_ctx_len] = '\n';
                                merged[superhuman_ctx_len + 1] = '\n';
                                memcpy(merged + superhuman_ctx_len + 2, em_ctx, pos + 1);
                                agent->alloc->free(agent->alloc->ctx, superhuman_ctx,
                                                   superhuman_ctx_len);
                                agent->alloc->free(agent->alloc->ctx, em_ctx, em_len);
                                superhuman_ctx = merged;
                                superhuman_ctx_len = merged_len;
                            } else {
                                agent->alloc->free(agent->alloc->ctx, em_ctx, em_len);
                            }
                        } else if (pos > 0) {
                            superhuman_ctx = em_ctx;
                            superhuman_ctx_len = pos;
                        } else {
                            agent->alloc->free(agent->alloc->ctx, em_ctx, em_len);
                        }
                    }
                }
                agent->alloc->free(agent->alloc->ctx, due,
                                   due_count * sizeof(hu_emotional_moment_t));
            }
        }
        /* F26: If they're quiet and it's during their usual quiet hours, inject hint */
        if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0 &&
            superhuman_ctx) {
            int qday = 0, qstart = 0, qend = 1;
            if (hu_superhuman_temporal_get_quiet_hours(agent->memory, agent->alloc,
                    agent->memory_session_id, agent->memory_session_id_len,
                    &qday, &qstart, &qend) == HU_OK) {
                time_t now_t = time(NULL);
                struct tm lt_buf;
                struct tm *lt = localtime_r(&now_t, &lt_buf);
                if (lt && lt->tm_wday == qday && lt->tm_hour >= qstart && lt->tm_hour < qend) {
                    static const char hint[] =
                        "\nThey're often quiet at this time. Don't worry if no reply.";
                    size_t hint_len = sizeof(hint) - 1;
                    size_t ctx_str_len = strlen(superhuman_ctx);
                    size_t new_len = ctx_str_len + hint_len + 1;
                    char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, new_len);
                    if (merged) {
                        memcpy(merged, superhuman_ctx, ctx_str_len);
                        memcpy(merged + ctx_str_len, hint, hint_len + 1);
                        agent->alloc->free(agent->alloc->ctx, superhuman_ctx,
                            superhuman_ctx_len);
                        superhuman_ctx = merged;
                        superhuman_ctx_len = new_len;
                    }
                }
            }
        }
#endif
    }

    /* Build adaptive persona context (circadian + relationship) */
    char *adaptive_ctx = NULL;
    size_t adaptive_ctx_len = 0;
#ifdef HU_HAS_PERSONA
    {
        uint8_t hour = 10;
#ifndef HU_IS_TEST
        {
            time_t now = time(NULL);
            struct tm lt_buf;
            struct tm *lt = localtime_r(&now, &lt_buf);
            if (lt)
                hour = (uint8_t)(lt->tm_hour & 0xFF);
        }
#endif
        char *circadian_str = NULL;
        size_t circadian_len = 0;
        char *rel_str = NULL;
        size_t rel_len = 0;
        if (hu_circadian_build_prompt(agent->alloc, hour, &circadian_str, &circadian_len) ==
                HU_OK &&
            circadian_str) {
            if (hu_relationship_build_prompt(agent->alloc, &agent->relationship, &rel_str,
                                             &rel_len) == HU_OK &&
                rel_str) {
                size_t total = circadian_len + rel_len + 1;
                adaptive_ctx = (char *)agent->alloc->alloc(agent->alloc->ctx, total);
                if (adaptive_ctx) {
                    memcpy(adaptive_ctx, circadian_str, circadian_len);
                    memcpy(adaptive_ctx + circadian_len, rel_str, rel_len);
                    adaptive_ctx[circadian_len + rel_len] = '\0';
                    adaptive_ctx_len = circadian_len + rel_len;
                }
                agent->alloc->free(agent->alloc->ctx, rel_str, rel_len + 1);
            }
            if (adaptive_ctx) {
                agent->alloc->free(agent->alloc->ctx, circadian_str, circadian_len + 1);
            } else {
                adaptive_ctx = circadian_str;
                adaptive_ctx_len = circadian_len;
            }
        }
    }
#endif

    /* Build situational awareness context */
    char *awareness_ctx = NULL;
    size_t awareness_ctx_len = 0;
    if (agent->awareness)
        awareness_ctx = hu_awareness_context(agent->awareness, agent->alloc, &awareness_ctx_len);

    /* Build cross-app PWA context */
#if HU_HAS_PWA
    {
        char *pwa_ctx = NULL;
        size_t pwa_ctx_len = 0;
        hu_error_t pwa_err = hu_pwa_context_build(agent->alloc, &pwa_ctx, &pwa_ctx_len);
        if (pwa_err == HU_OK && pwa_ctx && pwa_ctx_len > 0) {
            if (awareness_ctx) {
                /* Append PWA context to existing awareness */
                size_t total = awareness_ctx_len + 1 + pwa_ctx_len;
                char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, total + 1);
                if (merged) {
                    memcpy(merged, awareness_ctx, awareness_ctx_len);
                    merged[awareness_ctx_len] = '\n';
                    memcpy(merged + awareness_ctx_len + 1, pwa_ctx, pwa_ctx_len);
                    merged[total] = '\0';
                    agent->alloc->free(agent->alloc->ctx, awareness_ctx, awareness_ctx_len + 1);
                    awareness_ctx = merged;
                    awareness_ctx_len = total;
                }
            } else {
                /* Use PWA context as the awareness context */
                awareness_ctx = pwa_ctx;
                awareness_ctx_len = pwa_ctx_len;
                pwa_ctx = NULL; /* ownership transferred */
            }
        }
        if (pwa_ctx)
            agent->alloc->free(agent->alloc->ctx, pwa_ctx, pwa_ctx_len + 1);
    }
#endif

    /* Build outcome tracking summary */
    char *outcome_ctx = NULL;
    size_t outcome_ctx_len = 0;
    if (agent->outcomes)
        outcome_ctx = hu_outcome_build_summary(agent->outcomes, agent->alloc, &outcome_ctx_len);

    /* Build AGI frontier intelligence context */
    char *intelligence_ctx = NULL;
    size_t intelligence_ctx_len = 0;
#ifdef HU_ENABLE_SQLITE
    if (agent->memory) {
        sqlite3 *intel_db = hu_sqlite_memory_get_db(agent->memory);
        if (intel_db) {
            char parts[4096];
            size_t pos = 0;

            /* Self-improvement: active prompt patches */
            {
                hu_self_improve_t si;
                if (hu_self_improve_create(agent->alloc, intel_db, &si) == HU_OK) {
                    char *patches = NULL;
                    size_t patches_len = 0;
                    if (hu_self_improve_get_prompt_patches(&si, &patches, &patches_len) == HU_OK &&
                        patches && patches_len > 0) {
                        int n = snprintf(parts + pos, sizeof(parts) - pos,
                                         "### Learned Behaviors\n%.*s\n",
                                         (int)patches_len, patches);
                        if (n > 0 && pos + (size_t)n < sizeof(parts))
                            pos += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, patches, patches_len + 1);
                    }
                    char *tool_prefs = NULL;
                    size_t tool_prefs_len = 0;
                    if (hu_self_improve_get_tool_prefs_prompt(&si, &tool_prefs, &tool_prefs_len) == HU_OK &&
                        tool_prefs && tool_prefs_len > 0) {
                        int n = snprintf(parts + pos, sizeof(parts) - pos,
                                         "\n%s\n", tool_prefs);
                        if (n > 0 && pos + (size_t)n < sizeof(parts))
                            pos += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, tool_prefs, tool_prefs_len + 1);
                    }
                    hu_self_improve_deinit(&si);
                }
            }

            /* Goals: active goals context */
            {
                hu_goal_engine_t ge;
                if (hu_goal_engine_create(agent->alloc, intel_db, &ge) == HU_OK) {
                    char *gctx = NULL;
                    size_t gctx_len = 0;
                    if (hu_goal_build_context(&ge, &gctx, &gctx_len) == HU_OK &&
                        gctx && gctx_len > 0) {
                        int n = snprintf(parts + pos, sizeof(parts) - pos,
                                         "### %.*s\n", (int)gctx_len, gctx);
                        if (n > 0 && pos + (size_t)n < sizeof(parts))
                            pos += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, gctx, gctx_len + 1);
                    }
                    hu_goal_engine_deinit(&ge);
                }
            }

            /* Online learning: strategy preferences */
            {
                hu_online_learning_t ol;
                if (hu_online_learning_create(agent->alloc, intel_db, 0.1, &ol) == HU_OK) {
                    char *lctx = NULL;
                    size_t lctx_len = 0;
                    if (hu_online_learning_build_context(&ol, &lctx, &lctx_len) == HU_OK &&
                        lctx && lctx_len > 0) {
                        int n = snprintf(parts + pos, sizeof(parts) - pos,
                                         "### %.*s\n", (int)lctx_len, lctx);
                        if (n > 0 && pos + (size_t)n < sizeof(parts))
                            pos += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, lctx, lctx_len + 1);
                    }
                    hu_online_learning_deinit(&ol);
                }
            }

            /* Value learning: user values */
            {
                hu_value_engine_t ve;
                if (hu_value_engine_create(agent->alloc, intel_db, &ve) == HU_OK) {
                    char *vctx = NULL;
                    size_t vctx_len = 0;
                    if (hu_value_build_prompt(&ve, &vctx, &vctx_len) == HU_OK &&
                        vctx && vctx_len > 0) {
                        int n = snprintf(parts + pos, sizeof(parts) - pos,
                                         "### %.*s\n", (int)vctx_len, vctx);
                        if (n > 0 && pos + (size_t)n < sizeof(parts))
                            pos += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, vctx, vctx_len + 1);
                    }
                    hu_value_engine_deinit(&ve);
                }
            }

            /* World model: predict likely outcome of this request */
            {
                hu_world_model_t wm;
                if (hu_world_model_create(agent->alloc, intel_db, &wm) == HU_OK) {
                    hu_wm_prediction_t pred = {0};
                    double ctx_threshold = 0.3;
#ifdef HU_ENABLE_SQLITE
                    ctx_threshold = agent->meta_params.default_confidence_threshold * 0.6;
                    if (ctx_threshold < 0.1) ctx_threshold = 0.1;
#endif
                    if (hu_world_simulate(&wm, msg, msg_len, NULL, 0, &pred) == HU_OK &&
                        pred.confidence > ctx_threshold) {
                        int n = snprintf(parts + pos, sizeof(parts) - pos,
                                         "### Predicted Outcome\n"
                                         "Based on past patterns, this request likely leads to: %.*s "
                                         "(confidence: %.0f%%)\n",
                                         (int)(pred.outcome_len < 200 ? pred.outcome_len : 200),
                                         pred.outcome,
                                         pred.confidence * 100.0);
                        if (n > 0 && pos + (size_t)n < sizeof(parts))
                            pos += (size_t)n;
                    }
                    hu_world_model_deinit(&wm);
                }
            }

            /* Experience: recall similar past experiences (semantic when available) */
            {
                hu_experience_store_t exp_store;
                if (hu_experience_store_init(agent->alloc, agent->memory, &exp_store) == HU_OK) {
#ifdef HU_ENABLE_SQLITE
                    sqlite3 *exp_db = hu_sqlite_memory_get_db(agent->memory);
                    if (exp_db)
                        exp_store.db = exp_db;
#endif
                    char *exp_prompt = NULL;
                    size_t exp_prompt_len = 0;
                    if (hu_experience_build_prompt(&exp_store, msg, msg_len, &exp_prompt, &exp_prompt_len) == HU_OK &&
                        exp_prompt && exp_prompt_len > 0) {
                        int n = snprintf(parts + pos, sizeof(parts) - pos,
                                         "### %.*s\n", (int)exp_prompt_len, exp_prompt);
                        if (n > 0 && pos + (size_t)n < sizeof(parts))
                            pos += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, exp_prompt, exp_prompt_len + 1);
                    } else if (exp_prompt) {
                        agent->alloc->free(agent->alloc->ctx, exp_prompt, 1);
                    }
                    hu_experience_store_deinit(&exp_store);
                }
            }

            if (plan_ctx && plan_ctx_len > 0) {
                int n = snprintf(parts + pos, sizeof(parts) - pos, "### %.*s\n",
                                 (int)plan_ctx_len, plan_ctx);
                if (n > 0 && pos + (size_t)n < sizeof(parts))
                    pos += (size_t)n;
            }

            if (pos > 0) {
                intelligence_ctx = hu_strndup(agent->alloc, parts, pos);
                intelligence_ctx_len = pos;
            }
        }
    }
#endif

    /* Build persona prompt fresh each turn (channel-dependent; no caching) */
    char *persona_prompt = NULL;
    size_t persona_prompt_len = 0;
#ifdef HU_HAS_PERSONA
    if (agent->persona) {
        const char *ch = agent->active_channel;
        size_t ch_len = agent->active_channel_len;
        hu_error_t perr = hu_persona_build_prompt(agent->alloc, agent->persona, ch, ch_len, msg,
                                                  msg_len, &persona_prompt, &persona_prompt_len);
        if (perr != HU_OK) {
            if (pref_ctx)
                agent->alloc->free(agent->alloc->ctx, pref_ctx, pref_ctx_len + 1);
            if (memory_ctx)
                agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
            if (stm_ctx)
                agent->alloc->free(agent->alloc->ctx, stm_ctx, stm_ctx_len + 1);
            if (commitment_ctx)
                agent->alloc->free(agent->alloc->ctx, commitment_ctx, commitment_ctx_len + 1);
            if (pattern_ctx)
                agent->alloc->free(agent->alloc->ctx, pattern_ctx, pattern_ctx_len + 1);
            if (proactive_ctx)
                agent->alloc->free(agent->alloc->ctx, proactive_ctx, proactive_ctx_len + 1);
            if (superhuman_ctx)
                agent->alloc->free(agent->alloc->ctx, superhuman_ctx, superhuman_ctx_len);
            if (adaptive_ctx)
                agent->alloc->free(agent->alloc->ctx, adaptive_ctx, adaptive_ctx_len + 1);
            if (awareness_ctx)
                agent->alloc->free(agent->alloc->ctx, awareness_ctx, awareness_ctx_len + 1);
            if (outcome_ctx)
                agent->alloc->free(agent->alloc->ctx, outcome_ctx, outcome_ctx_len + 1);
            if (intelligence_ctx)
                agent->alloc->free(agent->alloc->ctx, intelligence_ctx, intelligence_ctx_len + 1);
            if (plan_ctx)
                agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
            hu_agent_clear_current_for_tools();
            return perr;
        }
    }
    /* Sentiment-aware persona: detect user's emotional tone and adjust */
    if (persona_prompt && msg && msg_len > 0) {
        static const char *neg_words[] = {"sad", "frustrated", "angry", "upset", "worried",
                                           "anxious", "stressed", "tired", "exhausted", "hurt",
                                           "disappointed", "confused", "lost", "struggling"};
        static const char *pos_words[] = {"happy", "excited", "great", "amazing", "wonderful",
                                           "grateful", "thankful", "love", "celebrating"};
        int neg_count = 0, pos_count = 0;
        for (int w = 0; w < (int)(sizeof(neg_words) / sizeof(neg_words[0])); w++) {
            size_t wlen = strlen(neg_words[w]);
            for (size_t p = 0; p + wlen <= msg_len; p++) {
                bool match = true;
                for (size_t j = 0; j < wlen && match; j++) {
                    char c = msg[p + j];
                    char n = neg_words[w][j];
                    if (c >= 'A' && c <= 'Z') c += 32;
                    if (c != n) match = false;
                }
                if (match) { neg_count++; break; }
            }
        }
        for (int w = 0; w < (int)(sizeof(pos_words) / sizeof(pos_words[0])); w++) {
            size_t wlen = strlen(pos_words[w]);
            for (size_t p = 0; p + wlen <= msg_len; p++) {
                bool match = true;
                for (size_t j = 0; j < wlen && match; j++) {
                    char c = msg[p + j];
                    char n = pos_words[w][j];
                    if (c >= 'A' && c <= 'Z') c += 32;
                    if (c != n) match = false;
                }
                if (match) { pos_count++; break; }
            }
        }
        const char *sentiment_note = NULL;
        if (neg_count >= 2)
            sentiment_note = "\n\n[Emotional context: User seems distressed. "
                             "Respond with extra warmth, empathy, and validation. "
                             "Prioritize emotional support before problem-solving.]";
        else if (neg_count == 1 && pos_count == 0)
            sentiment_note = "\n\n[Emotional context: User may be having a tough time. "
                             "Be warm and supportive in tone.]";
        else if (pos_count >= 2)
            sentiment_note = "\n\n[Emotional context: User is in a positive mood. "
                             "Match their energy — be enthusiastic and celebratory.]";
        if (sentiment_note) {
            size_t note_len = strlen(sentiment_note);
            size_t new_len = persona_prompt_len + note_len;
            char *augmented = (char *)agent->alloc->alloc(agent->alloc->ctx, new_len + 1);
            if (augmented) {
                memcpy(augmented, persona_prompt, persona_prompt_len);
                memcpy(augmented + persona_prompt_len, sentiment_note, note_len);
                augmented[new_len] = '\0';
                agent->alloc->free(agent->alloc->ctx, persona_prompt, persona_prompt_len + 1);
                persona_prompt = augmented;
                persona_prompt_len = new_len;
            }
        }
    }
#endif

    /* Build skills context from skillforge if available */
    char *skills_ctx = NULL;
    size_t skills_ctx_len = 0;
#ifdef HU_HAS_SKILLS
    if (agent->skillforge) {
        hu_skillforge_t *sf = (hu_skillforge_t *)agent->skillforge;
        hu_skill_t *all_skills = NULL;
        size_t all_count = 0;
        if (hu_skillforge_list_skills(sf, &all_skills, &all_count) == HU_OK && all_count > 0) {
            size_t total = 0;
            for (size_t i = 0; i < all_count; i++) {
                if (!all_skills[i].enabled)
                    continue;
                total += 4 + (all_skills[i].name ? strlen(all_skills[i].name) : 0) + 3 +
                         (all_skills[i].description ? strlen(all_skills[i].description) : 0) + 1;
            }
            if (total > 0) {
                skills_ctx = (char *)agent->alloc->alloc(agent->alloc->ctx, total + 1);
                if (skills_ctx) {
                    size_t pos = 0;
                    for (size_t i = 0; i < all_count; i++) {
                        if (!all_skills[i].enabled)
                            continue;
                        int n = snprintf(skills_ctx + pos, total + 1 - pos, "- %s: %s\n",
                                         all_skills[i].name ? all_skills[i].name : "",
                                         all_skills[i].description ? all_skills[i].description : "");
                        if (n > 0)
                            pos += (size_t)n;
                    }
                    skills_ctx[pos] = '\0';
                    skills_ctx_len = pos;
                }
            }
        }
    }
#if defined(HU_ENABLE_SQLITE) && defined(HU_HAS_SKILLS)
    /* Per-contact learned skills (DB-backed) */
    if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0) {
        sqlite3 *skill_db = hu_sqlite_memory_get_db(agent->memory);
        if (skill_db) {
            char *contact_skills = NULL;
            size_t contact_skills_len = 0;
            if (hu_skill_build_contact_context(agent->alloc, skill_db,
                    agent->memory_session_id, agent->memory_session_id_len,
                    &contact_skills, &contact_skills_len) == HU_OK &&
                contact_skills && contact_skills_len > 0) {
                if (skills_ctx && skills_ctx_len > 0) {
                    size_t merged_len = skills_ctx_len + 2 + contact_skills_len + 1;
                    char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, merged_len);
                    if (merged) {
                        memcpy(merged, skills_ctx, skills_ctx_len);
                        merged[skills_ctx_len] = '\n';
                        merged[skills_ctx_len + 1] = '\n';
                        memcpy(merged + skills_ctx_len + 2, contact_skills,
                               contact_skills_len + 1);
                        agent->alloc->free(agent->alloc->ctx, skills_ctx, skills_ctx_len + 1);
                        agent->alloc->free(agent->alloc->ctx, contact_skills,
                                           contact_skills_len + 1);
                        skills_ctx = merged;
                        skills_ctx_len = merged_len;
                    } else {
                        agent->alloc->free(agent->alloc->ctx, contact_skills,
                                           contact_skills_len + 1);
                    }
                } else {
                    skills_ctx = contact_skills;
                    skills_ctx_len = contact_skills_len;
                }
            } else if (contact_skills) {
                agent->alloc->free(agent->alloc->ctx, contact_skills,
                                   contact_skills_len + 1);
            }
        }
    }
#endif
#endif /* HU_HAS_SKILLS */

    /* Memory Tiers: inject core memory into prompt context */
    if (agent->sota_initialized) {
        char core_buf[2048];
        size_t core_len = 0;
        if (hu_tier_manager_build_core_prompt(&agent->tier_manager, core_buf, sizeof(core_buf),
                                              &core_len) == HU_OK && core_len > 0) {
            if (memory_ctx && memory_ctx_len > 0) {
                size_t merged_len = core_len + 1 + memory_ctx_len;
                char *merged = (char *)agent->alloc->alloc(agent->alloc->ctx, merged_len + 1);
                if (merged) {
                    memcpy(merged, core_buf, core_len);
                    merged[core_len] = '\n';
                    memcpy(merged + core_len + 1, memory_ctx, memory_ctx_len);
                    merged[merged_len] = '\0';
                    agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
                    memory_ctx = merged;
                    memory_ctx_len = merged_len;
                }
            } else if (!memory_ctx) {
                memory_ctx = hu_strndup(agent->alloc, core_buf, core_len);
                memory_ctx_len = memory_ctx ? core_len : 0;
            }
        }
    }

    /* Build system prompt using cached static portion when available */
    char *system_prompt = NULL;
    size_t system_prompt_len = 0;
    if (agent->cached_static_prompt && !pref_ctx && !tone_hint && !persona_prompt &&
        !awareness_ctx && !stm_ctx && !commitment_ctx && !pattern_ctx && !adaptive_ctx &&
        !proactive_ctx && !superhuman_ctx && !skills_ctx) {
        err = hu_prompt_build_with_cache(agent->alloc, agent->cached_static_prompt,
                                         agent->cached_static_prompt_len, memory_ctx,
                                         memory_ctx_len, &system_prompt, &system_prompt_len);
        if (memory_ctx)
            agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
        if (stm_ctx) {
            agent->alloc->free(agent->alloc->ctx, stm_ctx, stm_ctx_len + 1);
            stm_ctx = NULL;
        }
        if (err != HU_OK) {
            if (pref_ctx)
                agent->alloc->free(agent->alloc->ctx, pref_ctx, pref_ctx_len + 1);
            if (commitment_ctx)
                agent->alloc->free(agent->alloc->ctx, commitment_ctx, commitment_ctx_len + 1);
            if (pattern_ctx)
                agent->alloc->free(agent->alloc->ctx, pattern_ctx, pattern_ctx_len + 1);
            if (adaptive_ctx)
                agent->alloc->free(agent->alloc->ctx, adaptive_ctx, adaptive_ctx_len + 1);
            if (proactive_ctx)
                agent->alloc->free(agent->alloc->ctx, proactive_ctx, proactive_ctx_len + 1);
            if (superhuman_ctx)
                agent->alloc->free(agent->alloc->ctx, superhuman_ctx, superhuman_ctx_len);
            if (outcome_ctx)
                agent->alloc->free(agent->alloc->ctx, outcome_ctx, outcome_ctx_len + 1);
            if (intelligence_ctx)
                agent->alloc->free(agent->alloc->ctx, intelligence_ctx, intelligence_ctx_len + 1);
            if (plan_ctx)
                agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
            hu_agent_clear_current_for_tools();
            return err;
        }
    } else {
        hu_prompt_config_t cfg = {
            .provider_name = agent->provider.vtable->get_name(agent->provider.ctx),
            .provider_name_len = 0,
            .model_name = agent->model_name,
            .model_name_len = agent->model_name_len,
            .workspace_dir = agent->workspace_dir,
            .workspace_dir_len = agent->workspace_dir_len,
            .tools = agent->tools,
            .tools_count = agent->tools_count,
            .memory_context = memory_ctx,
            .memory_context_len = memory_ctx_len,
            .stm_context = stm_ctx,
            .stm_context_len = stm_ctx_len,
            .commitment_context = commitment_ctx,
            .commitment_context_len = commitment_ctx_len,
            .pattern_context = pattern_ctx,
            .pattern_context_len = pattern_ctx_len,
            .adaptive_persona_context = adaptive_ctx,
            .adaptive_persona_context_len = adaptive_ctx_len,
            .proactive_context = proactive_ctx,
            .proactive_context_len = proactive_ctx_len,
            .superhuman_context = superhuman_ctx,
            .superhuman_context_len = superhuman_ctx_len,
            .autonomy_level = agent->autonomy_level,
            .custom_instructions = agent->custom_instructions,
            .custom_instructions_len = agent->custom_instructions_len,
            .persona_prompt = persona_prompt,
            .persona_prompt_len = persona_prompt_len,
            .preferences = pref_ctx,
            .preferences_len = pref_ctx_len,
            .chain_of_thought = agent->chain_of_thought,
            .tone_hint = tone_hint,
            .tone_hint_len = tone_hint_len,
            .awareness_context = awareness_ctx,
            .awareness_context_len = awareness_ctx_len,
            .outcome_context = outcome_ctx,
            .outcome_context_len = outcome_ctx_len,
            .persona_immersive = (persona_prompt && persona_prompt_len > 0),
            .persona =
#ifdef HU_HAS_PERSONA
                agent->persona
#else
                NULL
#endif
            ,
            .contact_context = agent->contact_context,
            .contact_context_len = agent->contact_context_len,
            .conversation_context = agent->conversation_context,
            .conversation_context_len = agent->conversation_context_len,
            .max_response_chars = agent->max_response_chars,
            .intelligence_context = intelligence_ctx,
            .intelligence_context_len = intelligence_ctx_len,
            .skills_context = skills_ctx,
            .skills_context_len = skills_ctx_len,
            .native_tools = (agent->provider.vtable->supports_native_tools &&
                             agent->provider.vtable->supports_native_tools(agent->provider.ctx)),
        };
        err = hu_prompt_build_system(agent->alloc, &cfg, &system_prompt, &system_prompt_len);
        if (persona_prompt)
            agent->alloc->free(agent->alloc->ctx, persona_prompt, persona_prompt_len + 1);
        persona_prompt = NULL;
        if (memory_ctx)
            agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
        if (stm_ctx) {
            agent->alloc->free(agent->alloc->ctx, stm_ctx, stm_ctx_len + 1);
            stm_ctx = NULL;
        }
        if (awareness_ctx)
            agent->alloc->free(agent->alloc->ctx, awareness_ctx, awareness_ctx_len + 1);
        if (outcome_ctx)
            agent->alloc->free(agent->alloc->ctx, outcome_ctx, outcome_ctx_len + 1);
        if (intelligence_ctx)
            agent->alloc->free(agent->alloc->ctx, intelligence_ctx, intelligence_ctx_len + 1);
        if (skills_ctx)
            agent->alloc->free(agent->alloc->ctx, skills_ctx, skills_ctx_len + 1);
        skills_ctx = NULL;
        if (err != HU_OK) {
            if (pref_ctx)
                agent->alloc->free(agent->alloc->ctx, pref_ctx, pref_ctx_len + 1);
            if (commitment_ctx)
                agent->alloc->free(agent->alloc->ctx, commitment_ctx, commitment_ctx_len + 1);
            if (pattern_ctx)
                agent->alloc->free(agent->alloc->ctx, pattern_ctx, pattern_ctx_len + 1);
            if (adaptive_ctx)
                agent->alloc->free(agent->alloc->ctx, adaptive_ctx, adaptive_ctx_len + 1);
            if (proactive_ctx)
                agent->alloc->free(agent->alloc->ctx, proactive_ctx, proactive_ctx_len + 1);
            if (superhuman_ctx)
                agent->alloc->free(agent->alloc->ctx, superhuman_ctx, superhuman_ctx_len);
            if (plan_ctx)
                agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
            hu_agent_clear_current_for_tools();
            return err;
        }
    }
    if (stm_ctx)
        agent->alloc->free(agent->alloc->ctx, stm_ctx, stm_ctx_len + 1);
    if (commitment_ctx)
        agent->alloc->free(agent->alloc->ctx, commitment_ctx, commitment_ctx_len + 1);
    if (pattern_ctx)
        agent->alloc->free(agent->alloc->ctx, pattern_ctx, pattern_ctx_len + 1);
    if (adaptive_ctx)
        agent->alloc->free(agent->alloc->ctx, adaptive_ctx, adaptive_ctx_len + 1);
    if (proactive_ctx)
        agent->alloc->free(agent->alloc->ctx, proactive_ctx, proactive_ctx_len + 1);
    if (superhuman_ctx)
        agent->alloc->free(agent->alloc->ctx, superhuman_ctx, superhuman_ctx_len);
    if (pref_ctx)
        agent->alloc->free(agent->alloc->ctx, pref_ctx, pref_ctx_len + 1);

    /* Inject learned DPO preferences as few-shot examples */
    if (agent->sota_initialized && agent->dpo_collector.db && system_prompt) {
        char *dpo_examples = NULL;
        size_t dpo_len = 0;
        if (hu_dpo_get_best_examples(&agent->dpo_collector, agent->alloc, 5, &dpo_examples,
                                     &dpo_len) == HU_OK &&
            dpo_examples) {
            size_t new_len = system_prompt_len + dpo_len;
            char *new_sp = (char *)agent->alloc->realloc(agent->alloc->ctx, system_prompt,
                                                         system_prompt_len + 1, new_len + 1);
            if (new_sp) {
                memcpy(new_sp + system_prompt_len, dpo_examples, dpo_len);
                new_sp[new_len] = '\0';
                system_prompt = new_sp;
                system_prompt_len = new_len;
            }
            agent->alloc->free(agent->alloc->ctx, dpo_examples, dpo_len + 1);
        }
    }

    /* Tool routing: if enabled, select relevant subset of tools for this message */
    hu_tool_selection_t tool_selection;
    memset(&tool_selection, 0, sizeof(tool_selection));
    hu_tool_spec_t *routed_specs = NULL;
    size_t routed_specs_count = 0;
    if (agent->tool_routing_enabled && agent->tools_count > HU_TOOL_ROUTER_MAX_SELECTED) {
        hu_error_t rerr = hu_tool_router_select(agent->alloc, msg, msg_len, agent->tools,
                                                agent->tools_count, &tool_selection);
        if (rerr == HU_OK && tool_selection.count > 0) {
            routed_specs = (hu_tool_spec_t *)agent->alloc->alloc(
                agent->alloc->ctx, tool_selection.count * sizeof(hu_tool_spec_t));
            if (routed_specs) {
                for (size_t ri = 0; ri < tool_selection.count; ri++) {
                    size_t idx = tool_selection.indices[ri];
                    if (idx < agent->tool_specs_count)
                        routed_specs[routed_specs_count++] = agent->tool_specs[idx];
                }
            }
        }
    }

    /* Tool cache: create per-turn cache to avoid re-executing identical tool calls */
    hu_tool_cache_t *turn_cache = NULL;
    hu_tool_cache_create(agent->alloc, &turn_cache);

    hu_chat_message_t *msgs = NULL;
    size_t msgs_count = 0;

    hu_chat_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = agent->model_name;
    req.model_len = agent->model_name_len;
    req.temperature = agent->temperature;
    if (routed_specs_count > 0) {
        req.tools = routed_specs;
        req.tools_count = routed_specs_count;
    } else {
        req.tools = (agent->tool_specs_count > 0) ? agent->tool_specs : NULL;
        req.tools_count = agent->tool_specs_count;
    }

    uint32_t iter = 0;
    size_t turn_tool_results_count = 0;
    int reflection_retries_left = agent->reflection.max_retries;
    char *dpo_rejected_resp = NULL;
    size_t dpo_rejected_resp_len = 0;
    uint64_t max_tokens =
        agent->token_limit ? agent->token_limit
                           : hu_context_tokens_resolve(0, agent->model_name, agent->model_name_len);
    if (max_tokens == 0)
        max_tokens = 128000u;

    hu_compaction_config_t compact_cfg;
    hu_compaction_config_default(&compact_cfg);
    compact_cfg.max_history_messages = agent->max_history_messages;
    compact_cfg.token_limit = max_tokens;

    hu_agent_internal_generate_trace_id(agent->trace_id);
    clock_t turn_start = clock();
    uint64_t turn_tokens = 0;
    const char *prov_name = agent->provider.vtable->get_name
                                ? agent->provider.vtable->get_name(agent->provider.ctx)
                                : NULL;

    {
        hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_AGENT_START, .data = {{0}}};
        ev.data.agent_start.provider = prov_name ? prov_name : "";
        ev.data.agent_start.model = agent->model_name ? agent->model_name : "";
        HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
    }

    /* Per-turn arena: reset each iteration to reclaim ephemeral message arrays */
    hu_allocator_t turn_alloc =
        agent->turn_arena ? hu_arena_allocator(agent->turn_arena) : *agent->alloc;

    while (iter < agent->max_tool_iterations) {
        if (agent->cancel_requested) {
            if (dpo_rejected_resp)
                agent->alloc->free(agent->alloc->ctx, dpo_rejected_resp,
                                   dpo_rejected_resp_len + 1);
            if (system_prompt)
                agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
            if (plan_ctx)
                agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
            if (agent->turn_arena)
                hu_arena_reset(agent->turn_arena);
            return HU_ERR_CANCELLED;
        }
        if (agent->turn_arena)
            hu_arena_reset(agent->turn_arena);
        msgs = NULL;
        msgs_count = 0;
        iter++;
        /* Compact history if it exceeds limits (before each provider call).
         * Uses LLM summarization when the provider is available, with
         * rule-based fallback. */
        if (hu_should_compact(agent->history, agent->history_count, &compact_cfg)) {
            hu_compact_history_llm(agent->alloc, agent->history, &agent->history_count,
                                   &agent->history_cap, &compact_cfg, &agent->provider);

            /* Hierarchical summarization for deeper memory */
            if (agent->provider.vtable && agent->provider.vtable->chat) {
                char *session_sum = NULL, *chapter_sum = NULL, *overall_sum = NULL;
                size_t session_len = 0, chapter_len = 0, overall_len = 0;
                if (agent->history_count > 0) {
                    const char *last_content = agent->history[agent->history_count - 1].content;
                    size_t last_len = last_content ? strlen(last_content) : 0;
                    if (last_len > 0 &&
                        hu_compact_hierarchical(agent->alloc, &agent->provider,
                                                 agent->model_name, agent->model_name_len,
                                                 last_content, last_len, &session_sum, &session_len,
                                                 &chapter_sum, &chapter_len, &overall_sum,
                                                 &overall_len) == HU_OK) {
#if defined(HU_ENABLE_SQLITE)
                        if (agent->memory && agent->memory->vtable &&
                            agent->memory->vtable->store && agent->memory_session_id &&
                            agent->memory_session_id_len > 0) {
                            hu_memory_category_t hcat = {.tag = HU_MEMORY_CATEGORY_CONVERSATION};
                            const char *hsid = agent->memory_session_id;
                            size_t hsid_len = agent->memory_session_id_len;
                            static const char src[] = "compaction";
                            if (session_sum && session_len > 0)
                                (void)hu_memory_store_with_source(
                                    agent->memory, "hierarchical_session", 20, session_sum,
                                    session_len, &hcat, hsid, hsid_len, src, sizeof(src) - 1);
                            if (chapter_sum && chapter_len > 0)
                                (void)hu_memory_store_with_source(
                                    agent->memory, "hierarchical_chapter", 21, chapter_sum,
                                    chapter_len, &hcat, hsid, hsid_len, src, sizeof(src) - 1);
                            if (overall_sum && overall_len > 0)
                                (void)hu_memory_store_with_source(
                                    agent->memory, "hierarchical_overall", 21, overall_sum,
                                    overall_len, &hcat, hsid, hsid_len, src, sizeof(src) - 1);
                        }
#endif
                    }
                    if (session_sum)
                        agent->alloc->free(agent->alloc->ctx, session_sum, session_len + 1);
                    if (chapter_sum)
                        agent->alloc->free(agent->alloc->ctx, chapter_sum, chapter_len + 1);
                    if (overall_sum)
                        agent->alloc->free(agent->alloc->ctx, overall_sum, overall_len + 1);
                }
            }
        }

        /* Context pressure: estimate tokens, check thresholds, auto-compact if needed */
        {
            uint64_t current = hu_estimate_tokens(agent->history, agent->history_count) +
                               (uint64_t)((system_prompt_len + 3) / 4);
            hu_context_pressure_t pr = {
                .current_tokens = (size_t)current,
                .max_tokens = (size_t)max_tokens,
                .pressure = 0.0f,
                .warning_85_emitted = agent->context_pressure_warning_85_emitted,
                .warning_95_emitted = agent->context_pressure_warning_95_emitted,
            };
            if (hu_context_check_pressure(&pr, agent->context_pressure_warn,
                                          agent->context_pressure_compact)) {
                hu_context_compact_for_pressure(agent->alloc, agent->history, &agent->history_count,
                                                &agent->history_cap, (size_t)max_tokens,
                                                agent->context_compact_target);
                agent->context_pressure_warning_85_emitted = false;
                agent->context_pressure_warning_95_emitted = false;
            } else {
                agent->context_pressure_warning_85_emitted = pr.warning_85_emitted;
                agent->context_pressure_warning_95_emitted = pr.warning_95_emitted;
            }
        }

        /* Format messages for this iteration using arena allocator */
        {
            hu_chat_message_t *hist_msgs = NULL;
            size_t hist_count = 0;
            err = hu_context_format_messages(&turn_alloc, agent->history, agent->history_count,
                                             agent->max_history_messages, &hist_msgs, &hist_count);
            if (err != HU_OK) {
                hu_agent_clear_current_for_tools();
                if (system_prompt)
                    agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
                if (plan_ctx)
                    agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
                return err;
            }
            size_t total = (hist_msgs ? hist_count : 0) + 1;
            hu_chat_message_t *all = (hu_chat_message_t *)turn_alloc.alloc(
                turn_alloc.ctx, total * sizeof(hu_chat_message_t));
            if (!all) {
                hu_agent_clear_current_for_tools();
                if (system_prompt)
                    agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
                if (plan_ctx)
                    agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
                return HU_ERR_OUT_OF_MEMORY;
            }
            all[0].role = HU_ROLE_SYSTEM;
            all[0].content = system_prompt;
            all[0].content_len = system_prompt_len;
            all[0].name = NULL;
            all[0].name_len = 0;
            all[0].tool_call_id = NULL;
            all[0].tool_call_id_len = 0;
            all[0].content_parts = NULL;
            all[0].content_parts_count = 0;
            for (size_t i = 0; i < (hist_msgs ? hist_count : 0); i++)
                all[i + 1] = hist_msgs[i];
            msgs = all;
            msgs_count = total;
            req.messages = msgs;
            req.messages_count = msgs_count;
        }

        {
            hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_LLM_REQUEST, .data = {{0}}};
            ev.data.llm_request.provider = prov_name ? prov_name : "";
            ev.data.llm_request.model = agent->model_name ? agent->model_name : "";
            ev.data.llm_request.messages_count = msgs_count;
            HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
        }

        /* Extended reasoning: for complex queries on first iteration, run a
         * structured reasoning phase — ToT exploration + scratchpad synthesis.
         * The scratchpad captures hypotheses, evidence, and approach before
         * the main LLM call, giving it a richer starting context. */
#ifndef HU_IS_TEST
        if (agent->tree_of_thought_enabled && iter == 1 && msg_len > 200) {
            hu_tot_config_t tot_cfg = hu_tot_config_default();
            hu_tot_result_t tot_result;
            memset(&tot_result, 0, sizeof(tot_result));
            if (hu_tot_explore(agent->alloc, &agent->provider, agent->model_name,
                               agent->model_name_len, msg, msg_len, &tot_cfg, &tot_result) ==
                    HU_OK &&
                tot_result.best_thought && tot_result.best_thought_len > 0) {

                /* Build structured scratchpad from ToT exploration results */
                char scratchpad[4096];
                int sp_len = snprintf(scratchpad, sizeof(scratchpad),
                    "\n\n[REASONING SCRATCHPAD]\n"
                    "Branches explored: %zu | Best confidence: %.0f%%\n"
                    "Approach: %.*s\n"
                    "[/REASONING SCRATCHPAD]\n",
                    tot_result.branches_explored,
                    tot_result.best_score * 100.0,
                    (int)(tot_result.best_thought_len < 2048 ? tot_result.best_thought_len : 2048),
                    tot_result.best_thought);

                if (sp_len > 0 && (size_t)sp_len < sizeof(scratchpad) && system_prompt) {
                    size_t new_sys_len = system_prompt_len + (size_t)sp_len;
                    char *new_sys =
                        (char *)agent->alloc->alloc(agent->alloc->ctx, new_sys_len + 1);
                    if (new_sys) {
                        memcpy(new_sys, system_prompt, system_prompt_len);
                        memcpy(new_sys + system_prompt_len, scratchpad, (size_t)sp_len);
                        new_sys[new_sys_len] = '\0';
                        agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
                        system_prompt = new_sys;
                        system_prompt_len = new_sys_len;
                        if (msgs && msgs_count > 0) {
                            msgs[0].content = system_prompt;
                            msgs[0].content_len = system_prompt_len;
                        }
                    }
                }
            }
            hu_tot_result_free(agent->alloc, &tot_result);
        }
#endif

        /* PRM: score reasoning chain from ToT scratchpad if available */
        double prm_turn_score = 0.0;
        (void)prm_turn_score;

        /* Adaptive model selection: use per-turn override when set by the daemon's
         * model router, falling back to the agent's default model. */
        const char *turn_model = agent->model_name;
        size_t turn_model_len = agent->model_name_len;
        double turn_temp = agent->temperature;
        if (agent->turn_model && agent->turn_model_len > 0) {
            turn_model = agent->turn_model;
            turn_model_len = agent->turn_model_len;
        }
        if (agent->turn_temperature > 0.0)
            turn_temp = agent->turn_temperature;
        if (agent->turn_thinking_budget > 0)
            req.thinking_budget = agent->turn_thinking_budget;

        clock_t llm_start = clock();
        hu_chat_response_t resp;
        memset(&resp, 0, sizeof(resp));
        err =
            agent->provider.vtable->chat(agent->provider.ctx, agent->alloc, &req, turn_model,
                                         turn_model_len, turn_temp, &resp);
        uint64_t llm_duration_ms = hu_agent_internal_clock_diff_ms(llm_start, clock());

        {
            hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_LLM_RESPONSE, .data = {{0}}};
            ev.data.llm_response.provider = prov_name ? prov_name : "";
            ev.data.llm_response.model = agent->model_name ? agent->model_name : "";
            ev.data.llm_response.duration_ms = llm_duration_ms;
            ev.data.llm_response.success = (err == HU_OK);
            ev.data.llm_response.error_message = (err != HU_OK) ? "chat failed" : NULL;
            HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
        }

        if (err != HU_OK) {
            {
                hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_ERR, .data = {{0}}};
                ev.data.err.component = "agent";
                ev.data.err.message = "provider chat failed";
                HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
            }
            hu_agent_clear_current_for_tools();
            if (system_prompt)
                agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
            if (plan_ctx)
                agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
            if (agent->turn_arena)
                hu_arena_reset(agent->turn_arena);
            return err;
        }

        agent->total_tokens += resp.usage.total_tokens;
        hu_agent_internal_record_cost(agent, &resp.usage);
        turn_tokens += resp.usage.total_tokens;

        /* Text-based tool call parsing for providers without native tool support */
        if (resp.tool_calls_count == 0 && resp.content && resp.content_len > 0 &&
            agent->provider.vtable->supports_native_tools &&
            !agent->provider.vtable->supports_native_tools(agent->provider.ctx)) {
            hu_tool_call_t *text_calls = NULL;
            size_t text_calls_count = 0;
            if (hu_text_tool_calls_parse(agent->alloc, resp.content, resp.content_len, &text_calls,
                                         &text_calls_count) == HU_OK &&
                text_calls_count > 0) {
                /* Replace resp tool calls with parsed text tool calls */
                resp.tool_calls = text_calls;
                resp.tool_calls_count = text_calls_count;
            }
        }

        if (resp.tool_calls_count == 0) {
            uint64_t turn_duration_ms = hu_agent_internal_clock_diff_ms(turn_start, clock());
            {
                hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_AGENT_END, .data = {{0}}};
                ev.data.agent_end.duration_ms = turn_duration_ms;
                ev.data.agent_end.tokens_used = turn_tokens;
                HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
            }
            {
                hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_TURN_COMPLETE, .data = {{0}}};
                HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
            }
            if (resp.content && resp.content_len > 0) {
                /* PRM: score response reasoning chain for quality */
                if (agent->sota_initialized && agent->prm_config.enabled &&
                    resp.content_len > 100) {
                    hu_prm_result_t prm_res;
                    if (hu_prm_score_chain(agent->alloc, &agent->prm_config,
                            resp.content, resp.content_len, &prm_res) == HU_OK) {
                        prm_turn_score = prm_res.aggregate_score;
                        hu_prm_result_free(agent->alloc, &prm_res);
                    }
                }

                /* Reflection: evaluate response quality and retry if needed */
                hu_reflection_quality_t quality = hu_reflection_evaluate(
                    msg, msg_len, resp.content, resp.content_len, &agent->reflection);

                if (quality == HU_QUALITY_ACCEPTABLE && agent->reflection.use_llm &&
                    agent->reflection.enabled && reflection_retries_left > 0) {
                    quality =
                        hu_reflection_evaluate_llm(agent->alloc, &agent->provider, msg, msg_len,
                                                   resp.content, resp.content_len, quality);
                }

                if (quality == HU_QUALITY_NEEDS_RETRY && agent->reflection.enabled &&
                    reflection_retries_left > 0 && iter < agent->max_tool_iterations - 1) {
                    reflection_retries_left--;
                    char *critique = NULL;
                    size_t critique_len = 0;
                    hu_error_t cerr = hu_reflection_build_critique_prompt(
                        agent->alloc, msg, msg_len, resp.content, resp.content_len, &critique,
                        &critique_len);
                    if (cerr == HU_OK && critique) {
                        hu_error_t hist_err = hu_agent_internal_append_history(agent, HU_ROLE_ASSISTANT,
                                                               resp.content, resp.content_len, NULL,
                                                               0, NULL, 0);
                        if (hist_err != HU_OK)
                            fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                        hist_err = hu_agent_internal_append_history(agent, HU_ROLE_USER, critique,
                                                               critique_len, NULL, 0, NULL, 0);
                        if (hist_err != HU_OK)
                            fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                        agent->alloc->free(agent->alloc->ctx, critique, critique_len + 1);

                        /* DPO: save the rejected response for pairing with the
                         * chosen response after retry succeeds. */
                        if (agent->sota_initialized && resp.content && resp.content_len > 0) {
                            if (dpo_rejected_resp)
                                agent->alloc->free(agent->alloc->ctx, dpo_rejected_resp,
                                                   dpo_rejected_resp_len + 1);
                            dpo_rejected_resp = hu_strndup(agent->alloc, resp.content,
                                                           resp.content_len);
                            dpo_rejected_resp_len = resp.content_len;
                            hu_dpo_record_from_feedback(&agent->dpo_collector,
                                msg, msg_len, resp.content, resp.content_len, false);
                        }

                        hu_chat_response_free(agent->alloc, &resp);
                        iter++;
                        continue; /* retry with critique feedback */
                    }
                    if (critique)
                        agent->alloc->free(agent->alloc->ctx, critique, critique_len + 1);
                }

                /* DPO retry pair: record chosen vs rejected when reflection retry succeeded */
                if (agent->sota_initialized && dpo_rejected_resp && dpo_rejected_resp_len > 0 &&
                    resp.content && resp.content_len > 0) {
                    hu_dpo_record_from_retry(&agent->dpo_collector,
                        msg, msg_len,
                        dpo_rejected_resp, dpo_rejected_resp_len,
                        resp.content, resp.content_len);
                    agent->alloc->free(agent->alloc->ctx, dpo_rejected_resp,
                                       dpo_rejected_resp_len + 1);
                    dpo_rejected_resp = NULL;
                    dpo_rejected_resp_len = 0;
                }

                /* A/B: when first response quality < 70 and channel history available,
                 * generate up to 2 more candidates and pick the best. */
                const char *final_content = resp.content;
                size_t final_len = resp.content_len;
                bool ab_owned = false;
                uint32_t max_chars = agent->max_response_chars ? agent->max_response_chars : 0;
                if (agent->ab_history_entries && agent->ab_history_count > 0) {
                    hu_quality_score_t q0 = hu_conversation_evaluate_quality(
                        resp.content, resp.content_len, agent->ab_history_entries,
                        agent->ab_history_count, max_chars);
                    if (q0.total < 70) {
                        hu_ab_result_t ab_result;
                        memset(&ab_result, 0, sizeof(ab_result));
                        ab_result.candidates[0].response =
                            hu_strndup(agent->alloc, resp.content, resp.content_len);
                        ab_result.candidates[0].response_len = resp.content_len;
                        ab_result.candidate_count = 1;

                        /* Generate 2nd candidate with alternative phrasing prompt */
                        static const char alt_suffix[] = "\n\nAlternative phrasing:";
                        size_t alt_len = system_prompt_len + sizeof(alt_suffix) - 1;
                        char *alt_system =
                            (char *)agent->alloc->alloc(agent->alloc->ctx, alt_len + 1);
                        if (alt_system) {
                            memcpy(alt_system, system_prompt, system_prompt_len);
                            memcpy(alt_system + system_prompt_len, alt_suffix,
                                   sizeof(alt_suffix) - 1);
                            alt_system[alt_len] = '\0';
                            size_t total = msgs_count;
                            hu_chat_message_t *alt_all = (hu_chat_message_t *)agent->alloc->alloc(
                                agent->alloc->ctx, total * sizeof(hu_chat_message_t));
                            if (alt_all) {
                                alt_all[0].role = HU_ROLE_SYSTEM;
                                alt_all[0].content = alt_system;
                                alt_all[0].content_len = alt_len;
                                alt_all[0].name = NULL;
                                alt_all[0].name_len = 0;
                                alt_all[0].tool_call_id = NULL;
                                alt_all[0].tool_call_id_len = 0;
                                alt_all[0].content_parts = NULL;
                                alt_all[0].content_parts_count = 0;
                                for (size_t i = 1; i < total; i++)
                                    alt_all[i] = msgs[i];
                                hu_chat_request_t alt_req = req;
                                alt_req.messages = alt_all;
                                alt_req.messages_count = total;

                                hu_chat_response_t alt_resp;
                                memset(&alt_resp, 0, sizeof(alt_resp));
                                hu_error_t alt_err = agent->provider.vtable->chat(
                                    agent->provider.ctx, agent->alloc, &alt_req, agent->model_name,
                                    agent->model_name_len, agent->temperature, &alt_resp);
                                if (alt_err == HU_OK && alt_resp.content &&
                                    alt_resp.content_len > 0) {
                                    agent->total_tokens += alt_resp.usage.total_tokens;
                                    turn_tokens += alt_resp.usage.total_tokens;
                                    ab_result.candidates[1].response = hu_strndup(
                                        agent->alloc, alt_resp.content, alt_resp.content_len);
                                    ab_result.candidates[1].response_len = alt_resp.content_len;
                                    ab_result.candidate_count = 2;
                                    hu_chat_response_free(agent->alloc, &alt_resp);

                                    hu_quality_score_t q1 = hu_conversation_evaluate_quality(
                                        ab_result.candidates[1].response,
                                        ab_result.candidates[1].response_len,
                                        agent->ab_history_entries, agent->ab_history_count,
                                        max_chars);
                                    if (q1.total < 70) {
                                        hu_chat_response_t alt2_resp;
                                        memset(&alt2_resp, 0, sizeof(alt2_resp));
                                        hu_error_t alt2_err = agent->provider.vtable->chat(
                                            agent->provider.ctx, agent->alloc, &alt_req,
                                            agent->model_name, agent->model_name_len,
                                            agent->temperature, &alt2_resp);
                                        if (alt2_err == HU_OK && alt2_resp.content &&
                                            alt2_resp.content_len > 0) {
                                            agent->total_tokens += alt2_resp.usage.total_tokens;
                                            turn_tokens += alt2_resp.usage.total_tokens;
                                            ab_result.candidates[2].response =
                                                hu_strndup(agent->alloc, alt2_resp.content,
                                                           alt2_resp.content_len);
                                            ab_result.candidates[2].response_len =
                                                alt2_resp.content_len;
                                            ab_result.candidate_count = 3;
                                        }
                                        hu_chat_response_free(agent->alloc, &alt2_resp);
                                    }
                                } else {
                                    hu_chat_response_free(agent->alloc, &alt_resp);
                                }
                                agent->alloc->free(agent->alloc->ctx, alt_all,
                                                   total * sizeof(hu_chat_message_t));
                            }
                            agent->alloc->free(agent->alloc->ctx, alt_system, alt_len + 1);
                        }

                        if (hu_ab_evaluate(agent->alloc, &ab_result, agent->ab_history_entries,
                                           agent->ab_history_count, max_chars) == HU_OK) {
                            if (agent->bth_metrics)
                                agent->bth_metrics->ab_evaluations++;
                            size_t bi = ab_result.best_idx;
                            if (bi < ab_result.candidate_count &&
                                ab_result.candidates[bi].response) {
                                if (bi != 0 && agent->bth_metrics)
                                    agent->bth_metrics->ab_alternates_chosen++;
#ifdef HU_ENABLE_SQLITE
                                if (agent->memory) {
                                    sqlite3 *ab_db = hu_sqlite_memory_get_db(agent->memory);
                                    if (ab_db)
                                        (void)hu_ab_record_selection(ab_db, bi,
                                            ab_result.candidates[bi].quality_score,
                                            ab_result.candidate_count, (int64_t)time(NULL));
                                }
#endif
                                final_content = ab_result.candidates[bi].response;
                                final_len = ab_result.candidates[bi].response_len;
                                ab_result.candidates[bi].response = NULL;
                                ab_result.candidates[bi].response_len = 0;
                                ab_owned = true;
                            }
                        }
                        hu_ab_result_deinit(&ab_result, agent->alloc);
                    }
                }

                /* Uncertainty: evaluate confidence and add hedge if needed */
                {
                    hu_uncertainty_signals_t u_signals;
                    memset(&u_signals, 0, sizeof(u_signals));
                    hu_uncertainty_extract_signals(final_content, final_len, msg, msg_len,
                                                   0, 0, &u_signals);
                    hu_uncertainty_result_t u_result;
                    memset(&u_result, 0, sizeof(u_result));
                    if (hu_uncertainty_evaluate(agent->alloc, &u_signals, &u_result) == HU_OK) {
                        if (u_result.hedge_prefix && u_result.hedge_prefix_len > 0 &&
                            u_result.level >= HU_CONFIDENCE_MEDIUM) {
                            size_t new_len = u_result.hedge_prefix_len + final_len;
                            char *hedged = (char *)agent->alloc->alloc(agent->alloc->ctx, new_len + 1);
                            if (hedged) {
                                memcpy(hedged, u_result.hedge_prefix, u_result.hedge_prefix_len);
                                memcpy(hedged + u_result.hedge_prefix_len, final_content, final_len);
                                hedged[new_len] = '\0';
                                if (ab_owned)
                                    agent->alloc->free(agent->alloc->ctx, (void *)final_content, final_len + 1);
                                final_content = hedged;
                                final_len = new_len;
                                ab_owned = true;
                            }
                        }
                    }
                    hu_uncertainty_result_free(agent->alloc, &u_result);
                }

                /* Constitutional AI: critique response against principles */
#ifndef HU_IS_TEST
                if (agent->constitutional_enabled) {
                    hu_constitutional_config_t const_cfg = hu_constitutional_config_default();
                    hu_critique_result_t critique;
                    memset(&critique, 0, sizeof(critique));
                    if (hu_constitutional_critique(agent->alloc, &agent->provider,
                                                    agent->model_name, agent->model_name_len,
                                                    msg, msg_len, final_content, final_len,
                                                    &const_cfg, &critique) == HU_OK) {
                        if (critique.verdict == HU_CRITIQUE_REWRITE &&
                            critique.revised_response && critique.revised_response_len > 0) {
                            if (ab_owned)
                                agent->alloc->free(agent->alloc->ctx, (void *)final_content, final_len + 1);
                            final_content = critique.revised_response;
                            final_len = critique.revised_response_len;
                            ab_owned = true;
                            critique.revised_response = NULL;
                        }
                    }
                    hu_critique_result_free(agent->alloc, &critique);
                }
#endif

                hu_error_t hist_err = hu_agent_internal_append_history(agent, HU_ROLE_ASSISTANT, final_content,
                                                       final_len, NULL, 0, NULL, 0);
                if (hist_err != HU_OK)
                    fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                *response_out = hu_strndup(agent->alloc, final_content, final_len);
                if (ab_owned)
                    agent->alloc->free(agent->alloc->ctx, (void *)final_content, final_len + 1);
                if (!*response_out) {
                    hu_agent_clear_current_for_tools();
                    hu_chat_response_free(agent->alloc, &resp);
                    if (dpo_rejected_resp)
                        agent->alloc->free(agent->alloc->ctx, dpo_rejected_resp,
                                           dpo_rejected_resp_len + 1);
                    if (system_prompt)
                        agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
                    if (agent->turn_arena)
                        hu_arena_reset(agent->turn_arena);
                    return HU_ERR_OUT_OF_MEMORY;
                }
                if (response_len_out)
                    *response_len_out = final_len;

                /* Speculative: predict follow-ups and cache them */
                if (agent->speculative_cache && *response_out) {
                    char *preds[HU_SPEC_MAX_PREDICTIONS];
                    size_t pred_lens[HU_SPEC_MAX_PREDICTIONS];
                    double confs[HU_SPEC_MAX_PREDICTIONS];
                    size_t pred_count = 0;
                    memset(preds, 0, sizeof(preds));
                    if (hu_speculative_predict(agent->alloc, msg, msg_len,
                                               *response_out, final_len,
                                               preds, pred_lens, confs,
                                               HU_SPEC_MAX_PREDICTIONS, &pred_count) == HU_OK) {
                        for (size_t pi = 0; pi < pred_count; pi++) {
                            if (preds[pi])
                                agent->alloc->free(agent->alloc->ctx, preds[pi], pred_lens[pi] + 1);
                        }
                    }
                }

                hu_agent_internal_maybe_tts(agent, *response_out, final_len);
            }
            hu_chat_response_free(agent->alloc, &resp);
            hu_agent_clear_current_for_tools();
#ifdef HU_HAS_PERSONA
            hu_relationship_update(&agent->relationship, 1);
#endif
            /* Deep extraction: lightweight pattern-based fact extraction from user message */
            if (agent->memory && agent->memory->vtable && agent->memory->vtable->store) {
                hu_deep_extract_result_t de_result;
                memset(&de_result, 0, sizeof(de_result));
                if (hu_deep_extract_lightweight(agent->alloc, msg, msg_len, &de_result) == HU_OK &&
                    de_result.fact_count > 0) {
                    static const char facts_cat[] = "facts";
                    hu_memory_category_t cat = {
                        .tag = HU_MEMORY_CATEGORY_CUSTOM,
                        .data.custom = {.name = facts_cat, .name_len = sizeof(facts_cat) - 1},
                    };
                    const char *sid = agent->memory->current_session_id;
                    size_t sid_len = sid ? agent->memory->current_session_id_len : 0;
                    for (size_t fi = 0; fi < de_result.fact_count; fi++) {
                        const hu_extracted_fact_t *f = &de_result.facts[fi];
                        if (!f->subject || !f->predicate || !f->object)
                            continue;
                        size_t key_len =
                            strlen(f->subject) + 1 + strlen(f->predicate) + 1 + strlen(f->object);
                        char key_buf[256];
                        if (key_len < sizeof(key_buf)) {
                            int n = snprintf(key_buf, sizeof(key_buf), "%s:%s:%s", f->subject,
                                             f->predicate, f->object);
                            if (n > 0 && (size_t)n < sizeof(key_buf)) {
                                hu_error_t store_err = agent->memory->vtable->store(
                                    agent->memory->ctx, key_buf, (size_t)n, f->object,
                                    strlen(f->object), &cat, sid ? sid : "", sid_len);
                                if (store_err != HU_OK && store_err != HU_ERR_NOT_SUPPORTED)
                                    fprintf(stderr, "[agent] memory store failed: %d\n",
                                            (int)store_err);
                                if (agent->sota_initialized) {
                                    hu_memory_tier_t assigned;
                                    hu_tier_manager_auto_tier(&agent->tier_manager,
                                        key_buf, (size_t)n, f->object, strlen(f->object),
                                        &assigned);
                                }
                            }
                        }
                    }
                }
                hu_deep_extract_result_deinit(&de_result, agent->alloc);
            }
            /* Track agent's own commitments ("I'll check on that", "let me look into it") */
            if (agent->commitment_store && *response_out && *response_len_out > 0) {
                hu_commitment_detect_result_t cr;
                memset(&cr, 0, sizeof(cr));
                hu_error_t cerr = hu_commitment_detect(agent->alloc, *response_out,
                                                       *response_len_out, "assistant", 9, &cr);
                if (cerr == HU_OK && cr.count > 0) {
                    const char *sess = agent->memory_session_id;
                    size_t sess_len = sess ? agent->memory_session_id_len : 0;
                    for (size_t ci = 0; ci < cr.count; ci++) {
                        hu_error_t cs_err = hu_commitment_store_save(agent->commitment_store,
                                                                      &cr.commitments[ci],
                                                                      sess, sess_len);
                        if (cs_err != HU_OK)
                            fprintf(stderr, "[agent] commitment save failed: %d\n",
                                    (int)cs_err);
                    }
                }
                hu_commitment_detect_result_deinit(&cr, agent->alloc);
            }
#ifdef HU_ENABLE_SQLITE
            /* Record this turn as experience for future recall (with SQLite persistence) */
            if (agent->memory) {
                hu_experience_store_t exp_store;
                if (hu_experience_store_init(agent->alloc, agent->memory, &exp_store) == HU_OK) {
                    sqlite3 *rec_db = hu_sqlite_memory_get_db(agent->memory);
                    if (rec_db)
                        exp_store.db = rec_db;
                    const char *resp_text = *response_out ? *response_out : "";
                    size_t resp_len = response_len_out ? *response_len_out : 0;
                    {
                        hu_error_t exp_err = hu_experience_record(&exp_store, msg, msg_len,
                                                                   "agent_turn", 10,
                                                                   resp_text, resp_len, 1.0);
                        if (exp_err != HU_OK)
                            fprintf(stderr, "[agent] experience record failed: %d\n",
                                    (int)exp_err);
                    }
                    hu_experience_store_deinit(&exp_store);
                }
            }
            /* Value learning: detect approval, correction, re-asks, content-specific signals */
            if (agent->memory) {
                sqlite3 *vl_db = hu_sqlite_memory_get_db(agent->memory);
                if (vl_db) {
                    hu_value_engine_t ve;
                    if (hu_value_engine_create(agent->alloc, vl_db, &ve) == HU_OK) {
                        int64_t now_vl = (int64_t)time(NULL);

                        bool is_positive = (msg_len >= 4 && (
                            strstr(msg, "good") || strstr(msg, "great") ||
                            strstr(msg, "thanks") || strstr(msg, "perfect") ||
                            strstr(msg, "exactly")));
                        bool is_negative = (msg_len >= 2 && (
                            strstr(msg, "wrong") || strstr(msg, "bad") ||
                            strstr(msg, "incorrect") || strstr(msg, "no,")));
                        bool is_value_correction = (msg_len >= 6 && (
                            strstr(msg, "actually") || strstr(msg, "I meant") ||
                            strstr(msg, "not what") || strstr(msg, "try again")));

                        /* Re-ask detection: user repeats a similar message = previous answer was wrong */
                        bool is_reask = false;
                        if (agent->history_count >= 4 && msg_len > 10) {
                            for (size_t hi = agent->history_count - 2; hi > 0 && hi > agent->history_count - 8; hi--) {
                                if (agent->history[hi].role == HU_ROLE_USER &&
                                    agent->history[hi].content_len > 10) {
                                    size_t min_l = msg_len < agent->history[hi].content_len
                                                       ? msg_len : agent->history[hi].content_len;
                                    size_t match = 0;
                                    for (size_t ci = 0; ci < min_l && ci < 100; ci++) {
                                        if (msg[ci] == agent->history[hi].content[ci])
                                            match++;
                                    }
                                    if (min_l > 0 && match * 100 / min_l > 70) {
                                        is_reask = true;
                                        break;
                                    }
                                }
                            }
                        }

                        if (is_positive)
                            (void)hu_value_learn_from_approval(&ve, "helpfulness", 11, 0.15, now_vl);
                        if (is_negative || is_reask)
                            (void)hu_value_weaken(&ve, "helpfulness", 11, 0.15, now_vl);
                        if (is_value_correction)
                            (void)hu_value_learn_from_correction(&ve, "accuracy", 8,
                                "User corrected a factual or intent error", 42, 0.3, now_vl);
                        if (strstr(msg, "privacy") || strstr(msg, "private"))
                            (void)hu_value_learn_from_approval(&ve, "privacy", 7, 0.2, now_vl);
                        if (strstr(msg, "brief") || strstr(msg, "concise") || strstr(msg, "shorter"))
                            (void)hu_value_learn_from_correction(&ve, "conciseness", 11,
                                "User prefers shorter responses", 30, 0.2, now_vl);
                        if (strstr(msg, "detail") || strstr(msg, "elaborate") || strstr(msg, "more"))
                            (void)hu_value_learn_from_correction(&ve, "thoroughness", 12,
                                "User prefers detailed responses", 31, 0.2, now_vl);
                        /* DPO: record user feedback as preference signal */
                        if (agent->sota_initialized && agent->history_count >= 2) {
                            const char *last_resp = agent->history[agent->history_count - 2].content;
                            size_t last_resp_len = agent->history[agent->history_count - 2].content_len;
                            if (last_resp && last_resp_len > 0 && (is_positive || is_negative))
                                hu_dpo_record_from_feedback(&agent->dpo_collector,
                                    msg, msg_len, last_resp, last_resp_len, is_positive);
                        }

                        hu_value_engine_deinit(&ve);
                    }
                }
            }
#endif
            /* Adaptive RAG: record retrieval quality for learning */
            if (agent->sota_initialized && rag_strategy_used != HU_RAG_NONE && memory_ctx_len > 0)
                hu_adaptive_rag_record_outcome(&agent->adaptive_rag, rag_strategy_used,
                    memory_ctx_len > 100 ? 0.8 : 0.4);

#ifdef HU_HAS_PERSONA
            /* Style learning: adaptive schedule — early sessions learn faster,
             * then settle into a steady cadence. Also triggers on corrections. */
            {
                bool should_reanalyze = false;
                if (agent->persona_name && agent->persona_name_len > 0 && agent->memory) {
                    if (agent->history_count <= 20 && agent->history_count % 10 == 0 &&
                        agent->history_count > 0)
                        should_reanalyze = true;
                    else if (agent->history_count > 20 && agent->history_count <= 100 &&
                             agent->history_count % 25 == 0)
                        should_reanalyze = true;
                    else if (agent->history_count > 100 && agent->history_count % 50 == 0)
                        should_reanalyze = true;
                }
                if (should_reanalyze) {
                    const char *ch = agent->active_channel ? agent->active_channel : "cli";
                    size_t ch_len = agent->active_channel_len ? agent->active_channel_len : 3;
                    const char *cid = agent->memory_session_id ? agent->memory_session_id : "";
                    size_t cid_len = agent->memory_session_id_len ? agent->memory_session_id_len : 0;
                    (void)hu_persona_style_reanalyze(agent->alloc, &agent->provider,
                                                     agent->model_name, agent->model_name_len,
                                                     agent->memory,
                                                     agent->persona_name, agent->persona_name_len,
                                                     ch, ch_len, cid, cid_len);
                }
            }
#endif
#ifdef HU_ENABLE_SQLITE
            /* Goal progress: advance active goals based on turn output */
            if (agent->memory) {
                sqlite3 *goal_db = hu_sqlite_memory_get_db(agent->memory);
                if (goal_db) {
                    hu_goal_engine_t ge;
                    if (hu_goal_engine_create(agent->alloc, goal_db, &ge) == HU_OK) {
                        hu_goal_t active_goal;
                        bool gfound = false;
                        if (hu_goal_select_next(&ge, &active_goal, &gfound) == HU_OK && gfound) {
                            double pdelta = (turn_tool_results_count > 0)
                                ? 0.15 + 0.05 * (double)turn_tool_results_count
                                : 0.1;
                            double nprog = active_goal.progress + pdelta;
                            if (nprog > 1.0) nprog = 1.0;
                            (void)hu_goal_update_progress(&ge, active_goal.id,
                                                          nprog, (int64_t)time(NULL));
                        }
                        hu_goal_engine_deinit(&ge);
                    }
                }
            }

            /* Tier management: promote frequently-accessed memories, periodic consolidation */
            if (agent->sota_initialized && agent->memory) {
                if (memory_ctx && memory_ctx_len > 20) {
                    hu_tier_manager_promote(&agent->tier_manager,
                        memory_ctx, memory_ctx_len < 128 ? memory_ctx_len : 128,
                        HU_TIER_ARCHIVAL, HU_TIER_RECALL);
                }
                if (agent->history_count % 10 == 0)
                    hu_agent_consolidate_memory(agent);
            }

            /* Self-eval feedback: score this turn's output and feed into
             * the self-improvement pipeline for continuous learning */
            if (agent->memory && agent->history_count > 0 &&
                agent->history[agent->history_count - 1].role == HU_ROLE_ASSISTANT) {
                sqlite3 *eval_db = hu_sqlite_memory_get_db(agent->memory);
                if (eval_db) {
                    const char *resp_text = agent->history[agent->history_count - 1].content;
                    size_t resp_len2 = agent->history[agent->history_count - 1].content_len;
                    hu_reflection_quality_t self_q = hu_reflection_evaluate(
                        msg, msg_len, resp_text, resp_len2, &agent->reflection);
                    hu_self_improve_t si;
                    if (hu_self_improve_create(agent->alloc, eval_db, &si) == HU_OK) {
                        hu_self_improve_init_tables(&si);
                        bool quality_good = (self_q == HU_QUALITY_GOOD);
                        hu_self_improve_record_tool_outcome(
                            &si, "agent_turn", 10, quality_good,
                            (int64_t)time(NULL));
                        if (turn_tool_results_count > 0)
                            (void)hu_self_improve_apply_reflections(
                                &si, (int64_t)time(NULL));
                        hu_self_improve_deinit(&si);
                    }
                }
            }

            /* Record per-contact style evolution data from last assistant message */
            if (agent->memory && agent->memory_session_id && agent->memory_session_id_len > 0 &&
                agent->history_count > 0 &&
                agent->history[agent->history_count - 1].role == HU_ROLE_ASSISTANT) {
                const char *resp_text = agent->history[agent->history_count - 1].content;
                size_t resp_len = agent->history[agent->history_count - 1].content_len;
                if (resp_text && resp_len > 0) {
                    bool has_emoji = false;
                    bool has_question = false;
                    double formality = 0.5;
                    for (size_t si = 0; si < resp_len; si++) {
                        if (resp_text[si] == '?') has_question = true;
                        if ((unsigned char)resp_text[si] > 127) has_emoji = true;
                    }
                    size_t upper = 0, alpha = 0;
                    for (size_t si = 0; si < resp_len && si < 500; si++) {
                        if (resp_text[si] >= 'A' && resp_text[si] <= 'Z') { upper++; alpha++; }
                        else if (resp_text[si] >= 'a' && resp_text[si] <= 'z') alpha++;
                    }
                    if (alpha > 10)
                        formality = (double)upper / (double)alpha;
                    (void)hu_superhuman_style_record(agent->memory,
                        agent->memory_session_id, agent->memory_session_id_len,
                        resp_len, formality, has_emoji, has_question);
                }
            }
#endif
            if (dpo_rejected_resp)
                agent->alloc->free(agent->alloc->ctx, dpo_rejected_resp,
                                   dpo_rejected_resp_len + 1);
            if (system_prompt)
                agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
            if (plan_ctx)
                agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
            if (routed_specs)
                agent->alloc->free(agent->alloc->ctx, routed_specs,
                                   routed_specs_count * sizeof(hu_tool_spec_t));
            if (turn_cache)
                hu_tool_cache_destroy(agent->alloc, turn_cache);
            if (agent->turn_arena)
                hu_arena_reset(agent->turn_arena);
            return HU_OK;
        }

        err = hu_agent_internal_append_history_with_tool_calls(
            agent, resp.content ? resp.content : "", resp.content_len, resp.tool_calls,
            resp.tool_calls_count);
        if (err != HU_OK) {
            hu_agent_clear_current_for_tools();
            hu_chat_response_free(agent->alloc, &resp);
            if (system_prompt)
                agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
            if (plan_ctx)
                agent->alloc->free(agent->alloc->ctx, plan_ctx, plan_ctx_len + 1);
            if (agent->turn_arena)
                hu_arena_reset(agent->turn_arena);
            return err;
        }
        hu_chat_response_free(agent->alloc, &resp);

        {
            size_t tc_count = agent->history[agent->history_count - 1].tool_calls_count;
            const hu_tool_call_t *calls = agent->history[agent->history_count - 1].tool_calls;

            /* Emit TOOL_CALL_START events for all calls */
            for (size_t tc = 0; tc < tc_count; tc++) {
                char tn_buf[64];
                size_t tn = (calls[tc].name_len < sizeof(tn_buf) - 1) ? calls[tc].name_len
                                                                      : sizeof(tn_buf) - 1;
                if (tn > 0 && calls[tc].name)
                    memcpy(tn_buf, calls[tc].name, tn);
                tn_buf[tn] = '\0';
                hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_TOOL_CALL_START, .data = {{0}}};
                ev.data.tool_call_start.tool = tn_buf[0] ? tn_buf : "unknown";
                HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
            }

            /* LOCKED: skip all tool execution */
            if (agent->autonomy_level == HU_AUTONOMY_LOCKED) {
                for (size_t tc = 0; tc < tc_count; tc++) {
                    const hu_tool_call_t *call = &calls[tc];
                    hu_error_t hist_err = hu_agent_internal_append_history(
                        agent, HU_ROLE_TOOL, "Action blocked: agent is in locked mode", 38,
                        call->name, call->name_len, call->id, call->id_len);
                    if (hist_err != HU_OK)
                        fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                    if (agent->cancel_requested)
                        break;
                }
            } else {
                bool used_llm_compiler = false;
                /* LLMCompiler: when enabled, compile tool calls into a DAG for parallel execution.
                 * Note: LLMCompiler is opt-in via config; the existing dispatcher handles
                 * parallelism. */
#ifndef HU_IS_TEST
                /* LLMCompiler: if enabled and 3+ tool calls, use DAG-based execution */
                if (agent->llm_compiler_enabled && tc_count >= 3) {
                    const char *tool_names[32];
                    size_t tn_count = 0;
                    for (size_t ti = 0; ti < agent->tools_count && tn_count < 32; ti++) {
                        if (agent->tools[ti].vtable && agent->tools[ti].vtable->name)
                            tool_names[tn_count++] =
                                agent->tools[ti].vtable->name(agent->tools[ti].ctx);
                    }
                    char *compiler_prompt = NULL;
                    size_t compiler_prompt_len = 0;
                    const char *goal_text = msg;
                    size_t goal_text_len = msg_len;
                    if (hu_llm_compiler_build_prompt(agent->alloc, goal_text, goal_text_len,
                                                    tool_names, tn_count, &compiler_prompt,
                                                    &compiler_prompt_len) == HU_OK) {
                        hu_chat_message_t compiler_msgs[1] = {{
                            .role = HU_ROLE_USER,
                            .content = compiler_prompt,
                            .content_len = compiler_prompt_len,
                        }};
                        hu_chat_request_t compiler_req = {
                            .messages = compiler_msgs,
                            .messages_count = 1,
                            .tools = NULL,
                            .tools_count = 0,
                        };
                        hu_chat_response_t compiler_resp;
                        memset(&compiler_resp, 0, sizeof(compiler_resp));
                        hu_error_t cerr = agent->provider.vtable->chat(
                            agent->provider.ctx, agent->alloc, &compiler_req, agent->model_name,
                            agent->model_name_len, agent->temperature, &compiler_resp);
                        hu_str_free(agent->alloc, compiler_prompt);
                        if (cerr == HU_OK && compiler_resp.content &&
                            compiler_resp.content_len > 0) {
                            hu_dag_t dag;
                            hu_dag_init(&dag, *agent->alloc);
                            if (hu_llm_compiler_parse_plan(agent->alloc, compiler_resp.content,
                                                           compiler_resp.content_len,
                                                           &dag) == HU_OK &&
                                dag.node_count > 0) {
                                /* Execute DAG in dependency batches; parallelize ready nodes (POSIX) */
                                bool dag_executed = false;
                                size_t max_dag_iters = dag.node_count * 2;
                                for (size_t di = 0; di < max_dag_iters && !hu_dag_is_complete(&dag); di++) {
                                    hu_dag_batch_t batch;
                                    memset(&batch, 0, sizeof(batch));
                                    if (hu_dag_next_batch(&dag, &batch) != HU_OK || batch.count == 0)
                                        break;
#if (defined(__unix__) || defined(__APPLE__)) && !defined(HU_IS_TEST)
                                    if (batch.count > 1) {
                                        dag_parallel_work_t works[HU_DAG_MAX_BATCH_SIZE];
                                        pthread_t tids[HU_DAG_MAX_BATCH_SIZE];
                                        bool thread_started[HU_DAG_MAX_BATCH_SIZE];
                                        memset(thread_started, 0, sizeof(thread_started));
                                        for (size_t bi = 0; bi < batch.count; bi++) {
                                            works[bi].agent = agent;
                                            works[bi].node = batch.nodes[bi];
                                            works[bi].dag = &dag;
                                            if (pthread_create(&tids[bi], NULL, dag_parallel_worker,
                                                               &works[bi]) == 0)
                                                thread_started[bi] = true;
                                            else
                                                dag_parallel_worker(&works[bi]);
                                        }
                                        for (size_t bi = 0; bi < batch.count; bi++) {
                                            if (thread_started[bi])
                                                (void)pthread_join(tids[bi], NULL);
                                        }
                                        for (size_t bi = 0; bi < batch.count; bi++) {
                                            if (batch.nodes[bi]->status == HU_DAG_DONE)
                                                dag_executed = true;
                                        }
                                        continue;
                                    }
#endif
                                    for (size_t bi = 0; bi < batch.count; bi++) {
                                        hu_dag_node_t *node = batch.nodes[bi];
                                        node->status = HU_DAG_RUNNING;
                                        char *resolved_args = NULL;
                                        size_t resolved_len = 0;
                                        const char *use_args = node->args_json;
                                        if (node->args_json) {
                                            if (hu_dag_resolve_vars(agent->alloc, &dag,
                                                    node->args_json, strlen(node->args_json),
                                                    &resolved_args, &resolved_len) == HU_OK && resolved_args)
                                                use_args = resolved_args;
                                        }
                                        hu_tool_t *dag_tool = hu_agent_internal_find_tool(
                                            agent, node->tool_name,
                                            node->tool_name ? strlen(node->tool_name) : 0);
                                        if (!dag_tool) {
                                            node->status = HU_DAG_FAILED;
                                            if (resolved_args)
                                                agent->alloc->free(agent->alloc->ctx, resolved_args,
                                                                     resolved_len + 1);
                                            continue;
                                        }
                                        hu_tool_result_t dag_result = hu_tool_result_fail("invalid", 7);
                                        hu_json_value_t *dag_args = NULL;
                                        if (use_args) {
                                            hu_error_t jerr = hu_json_parse(agent->alloc, use_args,
                                                              strlen(use_args), &dag_args);
                                            if (jerr != HU_OK)
                                                fprintf(stderr, "[agent_turn] DAG tool args parse failed\n");
                                        }
                                        if (dag_args && dag_tool->vtable->execute) {
                                            dag_tool->vtable->execute(dag_tool->ctx, agent->alloc,
                                                                      dag_args, &dag_result);
                                        }
                                        if (dag_args)
                                            hu_json_free(agent->alloc, dag_args);
                                        if (resolved_args)
                                            agent->alloc->free(agent->alloc->ctx, resolved_args,
                                                                 resolved_len + 1);
                                        if (dag_result.success) {
                                            node->status = HU_DAG_DONE;
                                            dag_executed = true;
                                            if (dag_result.output && dag_result.output_len > 0) {
                                                node->result = hu_strndup(agent->alloc, dag_result.output,
                                                                          dag_result.output_len);
                                                node->result_len = dag_result.output_len;
                                            }
                                        } else {
                                            node->status = HU_DAG_FAILED;
                                        }
                                        hu_tool_result_free(agent->alloc, &dag_result);
                                    }
                                }
                                if (dag_executed) {
                                    used_llm_compiler = true;
                                    hu_observer_event_t ev = {
                                        .tag = HU_OBSERVER_EVENT_TOOL_CALL, .data = {{0}}};
                                    ev.data.tool_call.tool = "llm_compiler";
                                    ev.data.tool_call.duration_ms = 0;
                                    ev.data.tool_call.success = hu_dag_is_complete(&dag);
                                    HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
                                    /* Append DAG results to history so the LLM sees them */
                                    for (size_t ni = 0; ni < dag.node_count; ni++) {
                                        hu_dag_node_t *node = &dag.nodes[ni];
                                        const char *r = node->result ? node->result : (node->status == HU_DAG_DONE ? "ok" : "failed");
                                        size_t rlen = node->result ? node->result_len : strlen(r);
                                        hu_error_t hist_err = hu_agent_internal_append_history(
                                            agent, HU_ROLE_TOOL, r, rlen,
                                            node->tool_name, node->tool_name ? strlen(node->tool_name) : 0,
                                            node->id, node->id ? strlen(node->id) : 0);
                                        if (hist_err != HU_OK)
                                            fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                                    }
                                }
                            }
                            hu_dag_deinit(&dag);
                        }
                        hu_chat_response_free(agent->alloc, &compiler_resp);
                    }
                }
#endif
                /* Multi-agent orchestrator: decompose the user goal with the LLM when possible,
                 * then assign to registry agents; otherwise split on tool-call names. Consensus
                 * merge prefers the longest agreeing sub-agent output. */
                if (agent->multi_agent_enabled && agent->agent_registry &&
                    (tc_count >= 2 ||
                     (tc_count >= 1 && message_looks_multistep_for_orchestrator(msg, msg_len)))) {
                    hu_orchestrator_t orch;
                    if (hu_orchestrator_create(agent->alloc, &orch) == HU_OK) {
                        hu_orchestrator_load_from_registry(&orch, agent->agent_registry);
                        if (orch.agent_count > 0) {
                            bool have_plan = false;
                            if (agent->provider.vtable && agent->provider.vtable->chat_with_system &&
                                (tc_count >= 2 ||
                                 message_looks_multistep_for_orchestrator(msg, msg_len))) {
                                hu_decomposition_t decomp;
                                memset(&decomp, 0, sizeof(decomp));
                                hu_error_t derr = hu_orchestrator_decompose_goal(
                                    agent->alloc, &agent->provider, agent->model_name,
                                    agent->model_name_len, msg, msg_len, orch.agents,
                                    orch.agent_count, &decomp);
                                if (derr == HU_OK && decomp.task_count > 0) {
                                    hu_error_t aerr = hu_orchestrator_auto_assign(&orch, &decomp);
                                    if (aerr == HU_OK)
                                        have_plan = true;
                                }
                                hu_decomposition_free(agent->alloc, &decomp);
                            }
                            if (!have_plan && tc_count >= 2) {
                                const char *subtask_descs[HU_ORCHESTRATOR_MAX_TASKS];
                                size_t subtask_lens[HU_ORCHESTRATOR_MAX_TASKS];
                                size_t sub_n = tc_count < HU_ORCHESTRATOR_MAX_TASKS
                                                   ? tc_count : HU_ORCHESTRATOR_MAX_TASKS;
                                for (size_t s = 0; s < sub_n; s++) {
                                    subtask_descs[s] = calls[s].name;
                                    subtask_lens[s] = calls[s].name_len;
                                }
                                hu_orchestrator_propose_split(&orch, msg, msg_len, subtask_descs,
                                                              subtask_lens, sub_n);
                                for (size_t s = 0; s < orch.task_count && s < sub_n; s++) {
                                    if (orch.agents[s % orch.agent_count].agent_id_len > 0) {
                                        hu_orchestrator_assign_task(
                                            &orch, orch.tasks[s].id,
                                            orch.agents[s % orch.agent_count].agent_id,
                                            orch.agents[s % orch.agent_count].agent_id_len);
                                    }
                                }
                                have_plan = orch.task_count > 0;
                            }
                            if (have_plan) {
                            /* Use swarm for parallel execution when multiple tasks are assigned */
                            if (orch.task_count >= 2) {
                                hu_swarm_config_t swarm_cfg = hu_swarm_config_default();
                                swarm_cfg.provider = &agent->provider;
                                swarm_cfg.model = agent->model_name;
                                swarm_cfg.model_len = agent->model_name_len;
                                swarm_cfg.tools = agent->tools;
                                swarm_cfg.tools_count = agent->tools_count;
                                hu_swarm_task_t swarm_tasks[HU_ORCHESTRATOR_MAX_TASKS];
                                memset(swarm_tasks, 0, sizeof(swarm_tasks));
                                size_t swarm_n = 0;
                                for (size_t s = 0; s < orch.task_count && s < HU_ORCHESTRATOR_MAX_TASKS; s++) {
                                    if (orch.tasks[s].status == HU_TASK_ASSIGNED) {
                                        size_t dlen = orch.tasks[s].description_len;
                                        if (dlen >= sizeof(swarm_tasks[swarm_n].description))
                                            dlen = sizeof(swarm_tasks[swarm_n].description) - 1;
                                        memcpy(swarm_tasks[swarm_n].description, orch.tasks[s].description, dlen);
                                        swarm_tasks[swarm_n].description[dlen] = '\0';
                                        swarm_tasks[swarm_n].description_len = dlen;
                                        swarm_n++;
                                    }
                                }
                                if (swarm_n > 0) {
                                    hu_swarm_result_t swarm_result = {0};
                                    hu_error_t swarm_err = hu_swarm_execute(agent->alloc, &swarm_cfg,
                                                                             swarm_tasks, swarm_n, &swarm_result);
                                    if (swarm_err == HU_OK) {
                                        char swarm_merged[4096];
                                        size_t swarm_merged_len = 0;
                                        hu_swarm_aggregate(&swarm_result, HU_SWARM_AGG_CONCATENATE,
                                                           swarm_merged, sizeof(swarm_merged), &swarm_merged_len);
                                        if (swarm_merged_len > 0) {
                                            hu_error_t hist_err = hu_agent_internal_append_history(
                                                agent, HU_ROLE_TOOL, swarm_merged, swarm_merged_len,
                                                "swarm", 5, "swarm_parallel", 14);
                                            if (hist_err != HU_OK)
                                                fprintf(stderr, "[agent_turn] swarm history append failed: %d\n", hist_err);
                                        }
                                        for (size_t s = 0; s < swarm_n; s++) {
                                            size_t task_idx = 0;
                                            for (size_t t = 0; t < orch.task_count; t++) {
                                                if (orch.tasks[t].status == HU_TASK_ASSIGNED) {
                                                    if (task_idx == s) {
                                                        if (swarm_result.tasks[s].completed)
                                                            hu_orchestrator_complete_task(&orch, orch.tasks[t].id,
                                                                swarm_result.tasks[s].result, swarm_result.tasks[s].result_len);
                                                        else
                                                            hu_orchestrator_fail_task(&orch, orch.tasks[t].id,
                                                                "swarm failed", 12);
                                                        break;
                                                    }
                                                    task_idx++;
                                                }
                                            }
                                        }
                                    }
                                    hu_swarm_result_free(agent->alloc, &swarm_result);
                                }
                            } else {
                                /* Execute orchestrated tasks sequentially (single-task fallback) */
                                for (size_t s = 0; s < orch.task_count; s++) {
                                    hu_orchestrator_task_t *task = &orch.tasks[s];
                                    if (task->status != HU_TASK_ASSIGNED)
                                        continue;
                                    task->status = HU_TASK_IN_PROGRESS;
                                    hu_tool_t *orch_tool = hu_agent_internal_find_tool(
                                        agent, task->description, task->description_len);
                                    if (!orch_tool) {
                                        hu_orchestrator_fail_task(&orch, task->id, "tool not found", 14);
                                        continue;
                                    }
                                    hu_tool_result_t orch_result = hu_tool_result_fail("no args", 7);
                                    if (s < tc_count && calls[s].arguments_len > 0) {
                                        hu_json_value_t *orch_args = NULL;
                                        hu_error_t jerr = hu_json_parse(agent->alloc, calls[s].arguments,
                                                            calls[s].arguments_len, &orch_args);
                                        if (jerr != HU_OK)
                                            fprintf(stderr, "[agent_turn] tool args JSON parse failed\n");
                                        if (orch_args && orch_tool->vtable->execute) {
                                            orch_tool->vtable->execute(orch_tool->ctx, agent->alloc,
                                                                       orch_args, &orch_result);
                                        }
                                        if (orch_args)
                                            hu_json_free(agent->alloc, orch_args);
                                    }
                                    if (orch_result.success) {
                                        hu_orchestrator_complete_task(&orch, task->id,
                                            orch_result.output ? orch_result.output : "ok",
                                            orch_result.output ? orch_result.output_len : 2);
                                    } else {
                                        hu_orchestrator_fail_task(&orch, task->id,
                                            orch_result.error_msg ? orch_result.error_msg : "failed",
                                            orch_result.error_msg ? orch_result.error_msg_len : 6);
                                    }
                                    hu_tool_result_free(agent->alloc, &orch_result);
                                }
                            }
                            /* Merge and append orchestrated results */
                            char *merged = NULL;
                            size_t merged_len = 0;
                            if (hu_orchestrator_merge_results_consensus(&orch, agent->alloc, &merged,
                                                                        &merged_len) == HU_OK &&
                                merged && merged_len > 0) {
                                hu_error_t hist_err = hu_agent_internal_append_history(
                                    agent, HU_ROLE_TOOL, merged, merged_len,
                                    "orchestrator", 12, "orch_merge", 10);
                                if (hist_err != HU_OK)
                                    fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                                agent->alloc->free(agent->alloc->ctx, merged, merged_len + 1);
                            }
                            hu_observer_event_t ev = {
                                .tag = HU_OBSERVER_EVENT_TOOL_CALL, .data = {{0}}};
                            ev.data.tool_call.tool = "orchestrator";
                            ev.data.tool_call.duration_ms = 0;
                            ev.data.tool_call.success = hu_orchestrator_all_complete(&orch);
                            HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
                            }
                        }
                        hu_orchestrator_deinit(&orch);
                    }
                }

                if (!used_llm_compiler) {
                /* Use dispatcher for parallel execution when enabled (Tier 1.3) */
                hu_dispatcher_t dispatcher;
                hu_dispatcher_default(&dispatcher);
                if (tc_count > 1)
                    dispatcher.max_parallel = 4;
                dispatcher.timeout_secs = 30;
                dispatcher.cache = turn_cache;

                const hu_tool_call_t *calls_to_dispatch = calls;
#ifdef HU_ENABLE_SQLITE
                /* World model: rank tool calls by predicted outcome */
                if (tc_count >= 2 && agent->memory) {
                    sqlite3 *wm_db = hu_sqlite_memory_get_db(agent->memory);
                    if (wm_db) {
                        hu_world_model_t wm;
                        if (hu_world_model_create(agent->alloc, wm_db, &wm) == HU_OK) {
                            size_t opt_count = tc_count < 32 ? tc_count : 32;
                            hu_tool_call_t calls_buf[32];
                            for (size_t tc = 0; tc < opt_count; tc++)
                                calls_buf[tc] = calls[tc];
                            const char *action_names[32];
                            size_t action_lens[32];
                            for (size_t tc = 0; tc < opt_count; tc++) {
                                action_names[tc] = calls_buf[tc].name;
                                action_lens[tc] = calls_buf[tc].name_len;
                            }
                            hu_action_option_t options[32];
                            memset(options, 0, sizeof(options));
                            if (hu_world_evaluate_options(&wm, action_names, action_lens,
                                                          opt_count, msg, msg_len,
                                                          options) == HU_OK) {
                                /* options are sorted by score descending; reorder calls to match */
                                for (size_t i = 0; i < opt_count; i++) {
                                    for (size_t j = 0; j < opt_count; j++) {
                                        if (options[i].action_len == calls[j].name_len &&
                                            memcmp(options[i].action, calls[j].name,
                                                   options[i].action_len) == 0) {
                                            calls_buf[i] = calls[j];
                                            break;
                                        }
                                    }
                                }
                                calls_to_dispatch = calls_buf;
                                hu_observer_event_t ev = {
                                    .tag = HU_OBSERVER_EVENT_TOOL_CALL, .data = {{0}}};
                                ev.data.tool_call.tool = "world_model_reorder";
                                ev.data.tool_call.success = true;
                                HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
                            }
                            hu_world_model_deinit(&wm);
                        }
                    }
                }
#endif
                hu_dispatch_result_t dispatch_result;
                memset(&dispatch_result, 0, sizeof(dispatch_result));
                if (agent->tool_stream_cb) {
                    err = hu_dispatcher_dispatch_streaming(
                        &dispatcher, agent->alloc, agent->tools, agent->tools_count,
                        calls_to_dispatch, tc_count,
                        agent->tool_stream_cb, agent->tool_stream_ctx, &dispatch_result);
                } else {
                    err = hu_dispatcher_dispatch(&dispatcher, agent->alloc, agent->tools,
                                                 agent->tools_count, calls_to_dispatch, tc_count,
                                                 &dispatch_result);
                }
                if (err == HU_OK)
                    turn_tool_results_count += tc_count;
                (void)(turn_tool_results_count | 0); /* read to satisfy -Wunused-but-set-variable */

                if (err == HU_OK && dispatch_result.results) {
                    for (size_t tc = 0; tc < tc_count; tc++) {
                        const hu_tool_call_t *call = &calls[tc];
                        hu_tool_result_t *result = &dispatch_result.results[tc];

                        char tn_buf[64];
                        size_t tn = (call->name_len < sizeof(tn_buf) - 1) ? call->name_len
                                                                          : sizeof(tn_buf) - 1;
                        if (tn > 0 && call->name)
                            memcpy(tn_buf, call->name, tn);
                        tn_buf[tn] = '\0';
                        const char *args_str = call->arguments ? call->arguments : "";

                        /* Policy evaluation (dispatcher path) */
                        hu_policy_action_t pa = hu_agent_internal_evaluate_tool_policy(
                            agent, tn_buf[0] ? tn_buf : "unknown", args_str);
                        if (pa == HU_POLICY_DENY) {
                            if (agent->audit_logger) {
                                hu_audit_event_t aev;
                                hu_audit_event_init(&aev, HU_AUDIT_POLICY_VIOLATION);
                                hu_audit_event_with_identity(
                                    &aev, agent->agent_id,
                                    agent->model_name ? agent->model_name : "unknown", NULL);
                                hu_audit_event_with_action(&aev, tn_buf[0] ? tn_buf : "unknown",
                                                           "denied", false, false);
                                hu_audit_logger_log(agent->audit_logger, &aev);
                            }
                            hu_tool_result_free(agent->alloc, result);
                            *result = hu_tool_result_fail("denied by policy", 16);
                        } else if (pa == HU_POLICY_REQUIRE_APPROVAL) {
                            result->needs_approval = true;
                        }

                        /* Autonomy: SUPERVISED forces approval; ASSISTED for medium/high risk */
                        if (agent->autonomy_level == HU_AUTONOMY_SUPERVISED) {
                            result->needs_approval = true;
                        } else if (agent->autonomy_level == HU_AUTONOMY_ASSISTED) {
                            if (hu_tool_risk_level(tn_buf[0] ? tn_buf : "unknown") >=
                                HU_RISK_MEDIUM)
                                result->needs_approval = true;
                        }

                        /* Feature 2: explicit failure when approval required but no callback */
                        if (result->needs_approval && !agent->approval_cb) {
                            hu_tool_result_free(agent->alloc, result);
                            *result = hu_tool_result_fail("requires human approval", 23);
                        }

                        /* Approval flow: if tool needs approval, ask user and retry */
                        if (result->needs_approval && agent->approval_cb) {
                            char tn_tmp[64];
                            size_t tn2 = (call->name_len < sizeof(tn_tmp) - 1) ? call->name_len
                                                                               : sizeof(tn_tmp) - 1;
                            if (tn2 > 0 && call->name)
                                memcpy(tn_tmp, call->name, tn2);
                            tn_tmp[tn2] = '\0';
                            bool user_approved =
                                agent->approval_cb(agent->approval_ctx, tn_tmp, args_str);
                            if (user_approved) {
                                hu_tool_result_free(agent->alloc, result);
                                if (agent->policy)
                                    agent->policy->pre_approved = true;
                                hu_tool_t *tool =
                                    hu_agent_internal_find_tool(agent, call->name, call->name_len);
                                if (tool) {
                                    hu_json_value_t *retry_args = NULL;
                                    if (call->arguments_len > 0) {
                                        hu_error_t jerr = hu_json_parse(agent->alloc, call->arguments,
                                                            call->arguments_len, &retry_args);
                                        if (jerr != HU_OK)
                                            fprintf(stderr, "[agent_turn] tool args JSON parse failed\n");
                                    }
                                    *result = hu_tool_result_fail("invalid arguments", 16);
                                    if (retry_args) {
                                        if (tool->vtable->execute)
                                            tool->vtable->execute(tool->ctx, agent->alloc,
                                                                  retry_args, result);
                                        hu_json_free(agent->alloc, retry_args);
                                    }
                                }
                            } else {
                                hu_tool_result_free(agent->alloc, result);
                                *result = hu_tool_result_fail("user denied action", 18);
                            }
                        }

                        {
                            hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_TOOL_CALL,
                                                      .data = {{0}}};
                            ev.data.tool_call.tool = tn_buf[0] ? tn_buf : "unknown";
                            ev.data.tool_call.duration_ms = 0;
                            ev.data.tool_call.success = result->success;
                            ev.data.tool_call.detail =
                                result->success
                                    ? NULL
                                    : (result->error_msg ? result->error_msg : "failed");
                            HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
                        }

                        /* Outcome tracking */
                        if (agent->outcomes) {
                            const char *sum =
                                result->success
                                    ? (result->output ? result->output : "ok")
                                    : (result->error_msg ? result->error_msg : "failed");
                            hu_outcome_record_tool(agent->outcomes, tn_buf, result->success, sum);
                        }

#ifdef HU_ENABLE_SQLITE
                        /* Online learning: record tool outcome signal */
                        if (agent->memory) {
                            sqlite3 *ol_db = hu_sqlite_memory_get_db(agent->memory);
                            if (ol_db) {
                                hu_online_learning_t ol;
                                if (hu_online_learning_create(agent->alloc, ol_db, 0.1, &ol) ==
                                    HU_OK) {
                                    hu_learning_signal_t sig = {
                                        .type = result->success ? HU_SIGNAL_TOOL_SUCCESS
                                                               : HU_SIGNAL_TOOL_FAILURE,
                                        .tool_name = {0},
                                        .tool_name_len =
                                            tn < sizeof(sig.tool_name) ? tn : sizeof(sig.tool_name) - 1,
                                        .magnitude = 1.0,
                                        .timestamp = (int64_t)time(NULL),
                                    };
                                    if (tn > 0)
                                        memcpy(sig.tool_name, tn_buf, sig.tool_name_len);
                                    hu_online_learning_record(&ol, &sig);

                                    hu_self_improve_t si;
                                    if (hu_self_improve_create(agent->alloc, ol_db, &si) == HU_OK) {
                                        hu_self_improve_record_tool_outcome(&si, tn_buf, tn,
                                                                             result->success,
                                                                             sig.timestamp);
                                        hu_self_improve_deinit(&si);
                                    }
                                    hu_online_learning_deinit(&ol);
                                }
                            }
                        }
#endif

                        const char *res_content =
                            result->success ? result->output : result->error_msg;
                        size_t res_len =
                            result->success ? result->output_len : result->error_msg_len;
                        hu_error_t hist_err = hu_agent_internal_append_history(agent, HU_ROLE_TOOL, res_content,
                                                               res_len, call->name, call->name_len,
                                                               call->id, call->id_len);
                        if (hist_err != HU_OK)
                            fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);

                        if (agent->audit_logger) {
                            hu_audit_event_t aev;
                            hu_audit_event_init(&aev, HU_AUDIT_COMMAND_EXECUTION);
                            hu_audit_event_with_identity(
                                &aev, agent->agent_id,
                                agent->model_name ? agent->model_name : "unknown", NULL);
                            hu_audit_event_with_action(&aev, tn_buf, "tool", result->success, true);
                            hu_audit_event_with_result(&aev, result->success, 0, 0,
                                                       result->success ? NULL : result->error_msg);
                            hu_audit_logger_log(agent->audit_logger, &aev);
                        }

                        if (agent->cancel_requested)
                            break;
                    }
                    hu_dispatch_result_free(agent->alloc, &dispatch_result);
                } else {
                    /* Fallback: sequential if dispatcher fails */
                    for (size_t tc = 0; tc < tc_count; tc++) {
                        const hu_tool_call_t *call = &calls[tc];
                        hu_tool_t *tool =
                            hu_agent_internal_find_tool(agent, call->name, call->name_len);
                        if (!tool) {
                            hu_error_t hist_err = hu_agent_internal_append_history(
                                agent, HU_ROLE_TOOL, "tool not found", 14, call->name,
                                call->name_len, call->id, call->id_len);
                            if (hist_err != HU_OK)
                                fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                            continue;
                        }
                        char pol_tn[64];
                        size_t pol_tn_len = call->name_len < sizeof(pol_tn) - 1
                                                ? call->name_len
                                                : sizeof(pol_tn) - 1;
                        if (pol_tn_len > 0 && call->name)
                            memcpy(pol_tn, call->name, pol_tn_len);
                        pol_tn[pol_tn_len] = '\0';

                        hu_policy_action_t pa = hu_agent_internal_evaluate_tool_policy(
                            agent, pol_tn, call->arguments ? call->arguments : "");
                        bool force_approval = (agent->autonomy_level == HU_AUTONOMY_SUPERVISED) ||
                                              (agent->autonomy_level == HU_AUTONOMY_ASSISTED &&
                                               hu_tool_risk_level(pol_tn) >= HU_RISK_MEDIUM);

                        hu_tool_result_t result = hu_tool_result_fail("invalid arguments", 16);
                        if (pa == HU_POLICY_DENY) {
                            if (agent->audit_logger) {
                                hu_audit_event_t aev;
                                hu_audit_event_init(&aev, HU_AUDIT_POLICY_VIOLATION);
                                hu_audit_event_with_identity(
                                    &aev, agent->agent_id,
                                    agent->model_name ? agent->model_name : "unknown", NULL);
                                hu_audit_event_with_action(&aev, pol_tn, "denied", false, false);
                                hu_audit_logger_log(agent->audit_logger, &aev);
                            }
                            result = hu_tool_result_fail("denied by policy", 16);
                        } else if (pa == HU_POLICY_REQUIRE_APPROVAL || force_approval) {
                            result = hu_tool_result_fail("pending approval", 16);
                            result.needs_approval = true;
                        } else {
                            hu_json_value_t *args = NULL;
                            if (call->arguments_len > 0) {
                                hu_error_t pe = hu_json_parse(agent->alloc, call->arguments,
                                                              call->arguments_len, &args);
                                if (pe == HU_OK && args) {
                                    tool->vtable->execute(tool->ctx, agent->alloc, args, &result);
                                    hu_json_free(agent->alloc, args);
                                }
                            }
                        }

                        /* Feature 2: explicit failure when approval required but no callback */
                        if (result.needs_approval && !agent->approval_cb) {
                            hu_tool_result_free(agent->alloc, &result);
                            result = hu_tool_result_fail("requires human approval", 23);
                        }

                        /* Approval retry for sequential fallback path */
                        if (result.needs_approval && agent->approval_cb) {
                            char seq_tn[64];
                            size_t seq_n = (call->name_len < sizeof(seq_tn) - 1)
                                               ? call->name_len
                                               : sizeof(seq_tn) - 1;
                            if (seq_n > 0 && call->name)
                                memcpy(seq_tn, call->name, seq_n);
                            seq_tn[seq_n] = '\0';
                            if (agent->approval_cb(agent->approval_ctx, seq_tn,
                                                   call->arguments ? call->arguments : "")) {
                                hu_tool_result_free(agent->alloc, &result);
                                if (agent->policy)
                                    agent->policy->pre_approved = true;
                                hu_json_value_t *retry_args = NULL;
                                if (call->arguments_len > 0) {
                                    hu_error_t jerr = hu_json_parse(agent->alloc, call->arguments,
                                                        call->arguments_len, &retry_args);
                                    if (jerr != HU_OK)
                                        fprintf(stderr, "[agent_turn] tool args JSON parse failed\n");
                                }
                                result = hu_tool_result_fail("invalid arguments", 16);
                                if (retry_args) {
                                    tool->vtable->execute(tool->ctx, agent->alloc, retry_args,
                                                          &result);
                                    hu_json_free(agent->alloc, retry_args);
                                }
                            } else {
                                hu_tool_result_free(agent->alloc, &result);
                                result = hu_tool_result_fail("user denied action", 18);
                            }
                        }

                        const char *res_content = result.success ? result.output : result.error_msg;
                        size_t res_len = result.success ? result.output_len : result.error_msg_len;
                        hu_error_t hist_err = hu_agent_internal_append_history(agent, HU_ROLE_TOOL, res_content,
                                                               res_len, call->name, call->name_len,
                                                               call->id, call->id_len);
                        if (hist_err != HU_OK)
                            fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);

                        if (agent->audit_logger) {
                            hu_audit_event_t aev;
                            hu_audit_event_init(&aev, HU_AUDIT_COMMAND_EXECUTION);
                            hu_audit_event_with_identity(
                                &aev, agent->agent_id,
                                agent->model_name ? agent->model_name : "unknown", NULL);
                            hu_audit_event_with_action(&aev, pol_tn, "tool", result.success, true);
                            hu_audit_event_with_result(&aev, result.success, 0, 0,
                                                       result.success ? NULL : result.error_msg);
                            hu_audit_logger_log(agent->audit_logger, &aev);
                        }

                        hu_tool_result_free(agent->alloc, &result);
                        if (agent->cancel_requested)
                            break;
                    }
                }
                }
            }
        }
        /* Replan on tool failure: if any tool failed and we have a plan, generate
         * a revised plan and inject it as context for the next iteration */
        if (plan_ctx && !agent->cancel_requested) {
            size_t fail_count = 0;
            char fail_detail[512];
            size_t fail_pos = 0;
            for (size_t hi = agent->history_count; hi > 0 && hi > agent->history_count - 8; hi--) {
                if (agent->history[hi - 1].role == HU_ROLE_TOOL &&
                    agent->history[hi - 1].content &&
                    agent->history[hi - 1].content_len > 0) {
                    const char *c = agent->history[hi - 1].content;
                    if ((c[0] == 'E' || c[0] == 'e') ||
                        (agent->history[hi - 1].content_len > 6 &&
                         memcmp(c, "denied", 6) == 0)) {
                        fail_count++;
                        if (fail_pos < sizeof(fail_detail) - 2) {
                            size_t chunk = agent->history[hi - 1].content_len;
                            if (chunk > 80) chunk = 80;
                            if (fail_pos + chunk + 2 < sizeof(fail_detail)) {
                                memcpy(fail_detail + fail_pos, c, chunk);
                                fail_pos += chunk;
                                fail_detail[fail_pos++] = '\n';
                            }
                        }
                    }
                }
            }
            if (fail_count >= 2) {
                fail_detail[fail_pos] = '\0';
                hu_plan_t *revised = NULL;
                hu_error_t rp_err = hu_planner_replan(
                    agent->alloc, &agent->provider, agent->model_name, agent->model_name_len,
                    msg, msg_len, "partial progress", 16,
                    fail_detail, fail_pos, NULL, 0, &revised);
                if (rp_err == HU_OK && revised && revised->steps_count > 0) {
                    char replan_note[1024];
                    int rn = snprintf(replan_note, sizeof(replan_note),
                        "[REPLAN after %zu tool failures]: %zu new steps",
                        fail_count, revised->steps_count);
                    if (rn > 0) {
                        hu_agent_internal_append_history(
                            agent, HU_ROLE_SYSTEM, replan_note, (size_t)rn, NULL, 0, NULL, 0);
                    }
                }
                if (revised)
                    hu_plan_free(agent->alloc, revised);
            }
        }

        /* Mid-turn retrieval: after tool results, augment context with
         * memory relevant to both tool output and the evolving conversation */
        if (agent->memory && agent->memory->vtable &&
            agent->history_count > 1 &&
            agent->history[agent->history_count - 1].role == HU_ROLE_TOOL &&
            !agent->cancel_requested) {
            const char *last_result = NULL;
            size_t last_result_len = 0;
            if (agent->history_count > 0) {
                last_result = agent->history[agent->history_count - 1].content;
                last_result_len = agent->history[agent->history_count - 1].content_len;
            }
            if (last_result && last_result_len > 20) {
                char *mid_ctx = NULL;
                size_t mid_ctx_len = 0;
                hu_memory_loader_t mid_loader;
                hu_memory_loader_init(&mid_loader, agent->alloc, agent->memory,
                                       agent->retrieval_engine, 5, 2000);
                if (hu_memory_loader_load(&mid_loader, last_result, last_result_len,
                        agent->memory_session_id ? agent->memory_session_id : "",
                        agent->memory_session_id ? agent->memory_session_id_len : 0,
                        &mid_ctx, &mid_ctx_len) == HU_OK && mid_ctx && mid_ctx_len > 0) {
                    char *note = hu_sprintf(agent->alloc,
                        "[memory context from tool results]: %.*s",
                        (int)(mid_ctx_len < 2000 ? mid_ctx_len : 2000), mid_ctx);
                    if (note) {
                        hu_error_t hist_err = hu_agent_internal_append_history(
                            agent, HU_ROLE_SYSTEM, note, strlen(note), NULL, 0, NULL, 0);
                        if (hist_err != HU_OK)
                            fprintf(stderr, "[agent_turn] history append failed: %d\n", hist_err);
                        agent->alloc->free(agent->alloc->ctx, note, strlen(note) + 1);
                    }
                    agent->alloc->free(agent->alloc->ctx, mid_ctx, mid_ctx_len + 1);
                }

                /* Iterative retrieval: also retrieve against original user message
                 * to maintain relevance to the goal across iterations */
                if (iter > 1 && msg_len > 10) {
                    char *goal_ctx = NULL;
                    size_t goal_ctx_len = 0;
                    hu_memory_loader_t goal_loader;
                    hu_memory_loader_init(&goal_loader, agent->alloc, agent->memory,
                                          agent->retrieval_engine, 3, 1000);
                    if (hu_memory_loader_load(&goal_loader, msg, msg_len,
                            agent->memory_session_id ? agent->memory_session_id : "",
                            agent->memory_session_id ? agent->memory_session_id_len : 0,
                            &goal_ctx, &goal_ctx_len) == HU_OK && goal_ctx && goal_ctx_len > 0) {
                        char *gnote = hu_sprintf(agent->alloc,
                            "[goal-relevant memory]: %.*s",
                            (int)(goal_ctx_len < 1000 ? goal_ctx_len : 1000), goal_ctx);
                        if (gnote) {
                            hu_agent_internal_append_history(
                                agent, HU_ROLE_SYSTEM, gnote, strlen(gnote), NULL, 0, NULL, 0);
                            agent->alloc->free(agent->alloc->ctx, gnote, strlen(gnote) + 1);
                        }
                        agent->alloc->free(agent->alloc->ctx, goal_ctx, goal_ctx_len + 1);
                    }
                }
            }
        }
        /* Messages will be reformatted at top of next iteration via arena */
    }

    {
        hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_TOOL_ITERATIONS_EXHAUSTED,
                                  .data = {{0}}};
        ev.data.tool_iterations_exhausted.iterations = agent->max_tool_iterations;
        HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
    }
    {
        hu_observer_event_t ev = {.tag = HU_OBSERVER_EVENT_ERR, .data = {{0}}};
        ev.data.err.component = "agent";
        ev.data.err.message = "tool iterations exhausted";
        HU_OBS_SAFE_RECORD_EVENT(agent, &ev);
    }
    hu_agent_clear_current_for_tools();
    if (dpo_rejected_resp)
        agent->alloc->free(agent->alloc->ctx, dpo_rejected_resp, dpo_rejected_resp_len + 1);
    if (system_prompt)
        agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
    if (routed_specs)
        agent->alloc->free(agent->alloc->ctx, routed_specs,
                           routed_specs_count * sizeof(hu_tool_spec_t));
    if (turn_cache)
        hu_tool_cache_destroy(agent->alloc, turn_cache);
    if (agent->turn_arena)
        hu_arena_reset(agent->turn_arena);
    return HU_ERR_TIMEOUT;
}
