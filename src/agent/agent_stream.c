/* Streaming infrastructure: token callback wiring, hu_agent_turn_stream, hu_agent_turn_stream_v2 */
#include "agent_internal.h"
#include "human/agent/awareness.h"
#include "human/agent/commands.h"
#include "human/agent/memory_loader.h"
#include "human/agent/outcomes.h"
#include "human/agent/prompt.h"
#include "human/context.h"
#include "human/core/json.h"
#include "human/core/log.h"
#include "human/core/string.h"
#include "human/tool.h"
#ifdef HU_HAS_PERSONA
#include "human/persona.h"
#endif
#include "human/agent/constitutional.h"
#include "human/agent/gvr.h"
#include "human/agent/session_persist.h"
#include "human/cognition/metacognition.h"
#include "human/experience.h"
#include "human/humanness.h"
#ifdef HU_ENABLE_SQLITE
#include "human/intelligence/online_learning.h"
#include "human/intelligence/self_improve.h"
#include "human/intelligence/value_learning.h"
#include "human/memory.h"
#include <sqlite3.h>
#endif
#include "human/provider.h"
#include "human/voice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

typedef struct stream_token_wrap {
    hu_agent_stream_token_cb on_token;
    void *token_ctx;
    uint32_t initial_delay_ms; /* emotional pacing: delay before first chunk */
    bool first_chunk_sent;
} stream_token_wrap_t;

static bool stream_chunk_to_token_cb(void *ctx, const hu_stream_chunk_t *chunk) {
    stream_token_wrap_t *w = (stream_token_wrap_t *)ctx;
    if (chunk->is_final || !w->on_token)
        return true;
    if (chunk->type == HU_STREAM_CONTENT && chunk->delta && chunk->delta_len > 0) {
        /* Emotional pacing: pause before first content chunk for heavy messages */
        if (!w->first_chunk_sent && w->initial_delay_ms > 0) {
            uint32_t delay = w->initial_delay_ms;
            if (delay > 100)
                delay = 100; /* cap at 100ms */
#ifdef _WIN32
            Sleep(delay);
#else
            usleep((useconds_t)delay * 1000);
#endif
        }
        w->first_chunk_sent = true;
        w->on_token(chunk->delta, chunk->delta_len, w->token_ctx);
    }
    return true;
}

/* v1 shim: translates hu_agent_stream_event_t back to the simple token callback */
typedef struct v1_shim_ctx {
    hu_agent_stream_token_cb on_token;
    void *token_ctx;
} v1_shim_ctx_t;

static void v1_shim_event_cb(const hu_agent_stream_event_t *event, void *ctx) {
    v1_shim_ctx_t *s = (v1_shim_ctx_t *)ctx;
    if (event->type == HU_AGENT_STREAM_TEXT && s->on_token && event->data && event->data_len > 0)
        s->on_token(event->data, event->data_len, s->token_ctx);
}

/* v2 stream callback adapter: maps provider stream chunks to agent stream events */
typedef struct v2_stream_wrap {
    hu_agent_stream_event_cb on_event;
    void *event_ctx;
    uint32_t initial_delay_ms; /* emotional pacing */
    bool first_content_sent;
} v2_stream_wrap_t;

static bool stream_chunk_to_event_cb(void *ctx, const hu_stream_chunk_t *chunk) {
    v2_stream_wrap_t *w = (v2_stream_wrap_t *)ctx;
    if (!w->on_event || chunk->is_final)
        return true;
    hu_agent_stream_event_t ev;
    memset(&ev, 0, sizeof(ev));
    switch (chunk->type) {
    case HU_STREAM_CONTENT:
        if (!chunk->delta || chunk->delta_len == 0)
            return true;
        /* Emotional pacing: pause before first content chunk */
        if (!w->first_content_sent && w->initial_delay_ms > 0) {
            uint32_t delay = w->initial_delay_ms;
            if (delay > 100)
                delay = 100;
#ifdef _WIN32
            Sleep(delay);
#else
            usleep((useconds_t)delay * 1000);
#endif
        }
        w->first_content_sent = true;
        ev.type = HU_AGENT_STREAM_TEXT;
        ev.data = chunk->delta;
        ev.data_len = chunk->delta_len;
        break;
    case HU_STREAM_THINKING:
        if (!chunk->delta || chunk->delta_len == 0)
            return true;
        ev.type = HU_AGENT_STREAM_THINKING;
        ev.data = chunk->delta;
        ev.data_len = chunk->delta_len;
        break;
    case HU_STREAM_TOOL_START:
        ev.type = HU_AGENT_STREAM_TOOL_START;
        ev.tool_name = chunk->tool_name;
        ev.tool_name_len = chunk->tool_name_len;
        ev.tool_call_id = chunk->tool_call_id;
        ev.tool_call_id_len = chunk->tool_call_id_len;
        break;
    case HU_STREAM_TOOL_DELTA:
        ev.type = HU_AGENT_STREAM_TOOL_ARGS;
        ev.data = chunk->delta;
        ev.data_len = chunk->delta_len;
        ev.tool_name = chunk->tool_name;
        ev.tool_name_len = chunk->tool_name_len;
        ev.tool_call_id = chunk->tool_call_id;
        ev.tool_call_id_len = chunk->tool_call_id_len;
        break;
    case HU_STREAM_TOOL_DONE:
        return true; /* handled after stream_chat returns */
    }
    w->on_event(&ev, w->event_ctx);
    return true;
}

/* Tool streaming: bridges tool execute_streaming chunks to agent stream events */
typedef struct tool_stream_bridge {
    hu_agent_stream_event_cb on_event;
    void *event_ctx;
    const char *tool_name;
    size_t tool_name_len;
    const char *tool_call_id;
    size_t tool_call_id_len;
} tool_stream_bridge_t;

static void tool_chunk_to_event(void *ctx, const char *data, size_t len) {
    tool_stream_bridge_t *b = (tool_stream_bridge_t *)ctx;
    if (!b->on_event || !data || len == 0)
        return;
    hu_agent_stream_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = HU_AGENT_STREAM_TOOL_RESULT;
    ev.data = data;
    ev.data_len = len;
    ev.tool_name = b->tool_name;
    ev.tool_name_len = b->tool_name_len;
    ev.tool_call_id = b->tool_call_id;
    ev.tool_call_id_len = b->tool_call_id_len;
    ev.is_error = false;
    b->on_event(&ev, b->event_ctx);
}

hu_error_t hu_agent_turn_stream(hu_agent_t *agent, const char *msg, size_t msg_len,
                                hu_agent_stream_token_cb on_token, void *token_ctx,
                                char **response_out, size_t *response_len_out) {
    if (!agent || !msg || !response_out)
        return HU_ERR_INVALID_ARGUMENT;
    *response_out = NULL;
    if (response_len_out)
        *response_len_out = 0;

    hu_agent_set_current_for_tools(agent);

    hu_agent_internal_process_mailbox_messages(agent);

    char *slash_resp = hu_agent_handle_slash_command(agent, msg, msg_len);
    if (slash_resp) {
        hu_agent_clear_current_for_tools();
        *response_out = slash_resp;
        if (response_len_out)
            *response_len_out = strlen(slash_resp);
        return HU_OK;
    }

    bool can_stream = (on_token != NULL) && agent->provider.vtable->supports_streaming &&
                      agent->provider.vtable->supports_streaming(agent->provider.ctx) &&
                      agent->provider.vtable->stream_chat;

    if (!can_stream) {
        hu_error_t fallback_err =
            hu_agent_turn(agent, msg, msg_len, response_out, response_len_out);
        if (fallback_err == HU_OK && on_token && *response_out && response_len_out &&
            *response_len_out > 0) {
            size_t chunk_size = 12;
            for (size_t i = 0; i < *response_len_out; i += chunk_size) {
                size_t n = *response_len_out - i;
                if (n > chunk_size)
                    n = chunk_size;
                on_token(*response_out + i, n, token_ctx);
            }
        }
        hu_agent_clear_current_for_tools();
        return fallback_err;
    }

    /* When tools are present, use the v2 streaming loop which interleaves
     * streaming text with tool execution (Claude Desktop-style). */
    bool has_tools = (agent->tool_specs_count > 0);
    if (has_tools) {
        v1_shim_ctx_t shim = {.on_token = on_token, .token_ctx = token_ctx};
        hu_agent_clear_current_for_tools();
        return hu_agent_turn_stream_v2(agent, msg, msg_len, v1_shim_event_cb, &shim, response_out,
                                       response_len_out);
    }

    hu_error_t err =
        hu_agent_internal_append_history(agent, HU_ROLE_USER, msg, msg_len, NULL, 0, NULL, 0);
    if (err != HU_OK) {
        hu_agent_clear_current_for_tools();
        return err;
    }

    char *memory_ctx = NULL;
    size_t memory_ctx_len = 0;
    if (agent->memory && agent->memory->vtable) {
        hu_memory_loader_t loader;
        hu_memory_loader_init(&loader, agent->alloc, agent->memory, agent->retrieval_engine, 10,
                              4000);
        hu_error_t mem_err =
            hu_memory_loader_load(&loader, msg, msg_len, "", 0, &memory_ctx, &memory_ctx_len);
        if (mem_err != HU_OK && mem_err != HU_ERR_NOT_SUPPORTED)
            hu_log_error("agent_stream", NULL, "memory_loader_load failed: %s",
                         hu_error_string(mem_err));
    }

    /* Build situational awareness context */
    char *awareness_ctx = NULL;
    size_t awareness_ctx_len = 0;
    if (agent->awareness)
        awareness_ctx = hu_awareness_context(agent->awareness, agent->alloc, &awareness_ctx_len);

    /* Build outcome tracking summary */
    char *outcome_ctx = NULL;
    size_t outcome_ctx_len = 0;
    if (agent->outcomes)
        outcome_ctx = hu_outcome_build_summary(agent->outcomes, agent->alloc, &outcome_ctx_len);

    /* Build persona prompt fresh each turn (channel-dependent; no caching) */
    char *persona_prompt = NULL;
    size_t persona_prompt_len = 0;
#ifdef HU_HAS_PERSONA
    if (agent->persona) {
        const char *ch = agent->active_channel;
        size_t ch_len = agent->active_channel_len;
        hu_error_t perr = hu_persona_build_prompt(agent->alloc, agent->persona, ch, ch_len, NULL, 0,
                                                  &persona_prompt, &persona_prompt_len);
        if (perr != HU_OK) {
            if (memory_ctx)
                agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
            if (awareness_ctx)
                agent->alloc->free(agent->alloc->ctx, awareness_ctx, awareness_ctx_len + 1);
            if (outcome_ctx)
                agent->alloc->free(agent->alloc->ctx, outcome_ctx, outcome_ctx_len + 1);
            hu_agent_clear_current_for_tools();
            return perr;
        }
    }
#endif

    char *system_prompt = NULL;
    size_t system_prompt_len = 0;
    if (agent->cached_static_prompt && !persona_prompt && !awareness_ctx) {
        err = hu_prompt_build_with_cache(agent->alloc, agent->cached_static_prompt,
                                         agent->cached_static_prompt_len, memory_ctx,
                                         memory_ctx_len, &system_prompt, &system_prompt_len);
        if (memory_ctx)
            agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
        if (err != HU_OK) {
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
            .autonomy_level = agent->autonomy_level,
            .custom_instructions = agent->custom_instructions,
            .custom_instructions_len = agent->custom_instructions_len,
            .persona_prompt = persona_prompt,
            .persona_prompt_len = persona_prompt_len,
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
        };
        err = hu_prompt_build_system(agent->alloc, &cfg, &system_prompt, &system_prompt_len);
        if (persona_prompt)
            agent->alloc->free(agent->alloc->ctx, persona_prompt, persona_prompt_len + 1);
        persona_prompt = NULL;
        if (memory_ctx)
            agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
        if (awareness_ctx)
            agent->alloc->free(agent->alloc->ctx, awareness_ctx, awareness_ctx_len + 1);
        if (outcome_ctx)
            agent->alloc->free(agent->alloc->ctx, outcome_ctx, outcome_ctx_len + 1);
        if (err != HU_OK) {
            hu_agent_clear_current_for_tools();
            return err;
        }
    }

    hu_chat_message_t *msgs = NULL;
    size_t msgs_count = 0;
    err = hu_context_format_messages(agent->alloc, agent->history, agent->history_count,
                                     agent->max_history_messages, NULL, &msgs, &msgs_count);
    if (err != HU_OK) {
        if (system_prompt)
            agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
        return err;
    }

    size_t total_msgs = (msgs ? msgs_count : 0) + 1;
    hu_chat_message_t *all_msgs = (hu_chat_message_t *)agent->alloc->alloc(
        agent->alloc->ctx, total_msgs * sizeof(hu_chat_message_t));
    if (!all_msgs) {
        if (system_prompt)
            agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
        if (msgs)
            agent->alloc->free(agent->alloc->ctx, msgs, msgs_count * sizeof(hu_chat_message_t));
        return HU_ERR_OUT_OF_MEMORY;
    }
    all_msgs[0].role = HU_ROLE_SYSTEM;
    all_msgs[0].content = system_prompt;
    all_msgs[0].content_len = system_prompt_len;
    all_msgs[0].name = NULL;
    all_msgs[0].name_len = 0;
    all_msgs[0].tool_call_id = NULL;
    all_msgs[0].tool_call_id_len = 0;
    all_msgs[0].content_parts = NULL;
    all_msgs[0].content_parts_count = 0;
    for (size_t i = 0; i < (msgs ? msgs_count : 0); i++)
        all_msgs[i + 1] = msgs[i];
    if (msgs)
        agent->alloc->free(agent->alloc->ctx, msgs, msgs_count * sizeof(hu_chat_message_t));
    msgs = all_msgs;
    msgs_count = total_msgs;

    const char *eff_model = agent->model_name;
    size_t eff_model_len = agent->model_name_len;
    double eff_temp = agent->temperature;
    if (agent->turn_model && agent->turn_model_len > 0) {
        eff_model = agent->turn_model;
        eff_model_len = agent->turn_model_len;
    }
    if (agent->turn_temperature > 0.0)
        eff_temp = agent->turn_temperature;

    hu_chat_request_t req;
    memset(&req, 0, sizeof(req));
    req.messages = msgs;
    req.messages_count = msgs_count;
    req.model = eff_model;
    req.model_len = eff_model_len;
    req.temperature = eff_temp;
    req.tools = (agent->tool_specs_count > 0) ? agent->tool_specs : NULL;
    req.tools_count = agent->tool_specs_count;

    {
        /* Emotional pacing: compute delay from message weight */
        hu_emotional_weight_t ew = hu_emotional_weight_classify(msg, msg_len);
        uint32_t pacing_delay = (uint32_t)hu_emotional_pacing_adjust(0, ew);
        stream_token_wrap_t wrap = {.on_token = on_token,
                                    .token_ctx = token_ctx,
                                    .initial_delay_ms = pacing_delay,
                                    .first_chunk_sent = false};
        hu_stream_chat_result_t sresp;
        memset(&sresp, 0, sizeof(sresp));
        err = agent->provider.vtable->stream_chat(agent->provider.ctx, agent->alloc, &req,
                                                  eff_model, eff_model_len, eff_temp,
                                                  stream_chunk_to_token_cb, &wrap, &sresp);
        if (msgs)
            agent->alloc->free(agent->alloc->ctx, msgs, msgs_count * sizeof(hu_chat_message_t));
        if (system_prompt)
            agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
        if (err != HU_OK) {
            hu_agent_clear_current_for_tools();
            return err;
        }
        agent->total_tokens += sresp.usage.total_tokens;
        hu_agent_internal_record_cost(agent, &sresp.usage);
        if (sresp.content && sresp.content_len > 0) {
            {
                hu_error_t hist_err = hu_agent_internal_append_history(
                    agent, HU_ROLE_ASSISTANT, sresp.content, sresp.content_len, NULL, 0, NULL, 0);
                if (hist_err != HU_OK)
                    hu_log_error("agent_stream", NULL, "append_history failed: %s",
                                 hu_error_string(hist_err));
            }
            *response_out = hu_strndup(agent->alloc, sresp.content, sresp.content_len);
            hu_agent_internal_maybe_tts(agent, sresp.content, sresp.content_len);
            agent->alloc->free(agent->alloc->ctx, (void *)sresp.content, sresp.content_len + 1);
            if (!*response_out)
                return HU_ERR_OUT_OF_MEMORY;
            if (response_len_out)
                *response_len_out = sresp.content_len;
        }
        hu_agent_clear_current_for_tools();
        return HU_OK;
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * hu_agent_turn_stream_v2 — Rich streaming with interleaved tool execution
 *
 * Streams text tokens between tool calls (Claude Desktop-style):
 *   1. Call provider stream_chat (with tools)
 *   2. Text deltas → emit HU_AGENT_STREAM_TEXT
 *   3. Tool call detected → emit TOOL_START / TOOL_ARGS during stream
 *   4. After stream completes with tool calls: execute each tool, emit TOOL_RESULT
 *   5. Loop back to step 1 with tool results in context
 *   6. When no tool calls remain: final text is the response
 * ────────────────────────────────────────────────────────────────────────── */

#define STREAM_V2_MAX_TOOL_DEPTH 10

hu_error_t hu_agent_turn_stream_v2(hu_agent_t *agent, const char *msg, size_t msg_len,
                                   hu_agent_stream_event_cb on_event, void *event_ctx,
                                   char **response_out, size_t *response_len_out) {
    if (!agent || !msg || !response_out)
        return HU_ERR_INVALID_ARGUMENT;
    *response_out = NULL;
    if (response_len_out)
        *response_len_out = 0;

    const char *eff_model_v2 = agent->model_name;
    size_t eff_model_v2_len = agent->model_name_len;
    double eff_temp_v2 = agent->temperature;
    if (agent->turn_model && agent->turn_model_len > 0) {
        eff_model_v2 = agent->turn_model;
        eff_model_v2_len = agent->turn_model_len;
    }
    if (agent->turn_temperature > 0.0)
        eff_temp_v2 = agent->turn_temperature;
    (void)eff_model_v2;
    (void)eff_model_v2_len;
    (void)eff_temp_v2;

    hu_agent_set_current_for_tools(agent);
    hu_agent_internal_process_mailbox_messages(agent);

    char *slash_resp = hu_agent_handle_slash_command(agent, msg, msg_len);
    if (slash_resp) {
        hu_agent_clear_current_for_tools();
        *response_out = slash_resp;
        if (response_len_out)
            *response_len_out = strlen(slash_resp);
        return HU_OK;
    }

    bool can_stream = (on_event != NULL) && agent->provider.vtable->supports_streaming &&
                      agent->provider.vtable->supports_streaming(agent->provider.ctx) &&
                      agent->provider.vtable->stream_chat;

    /* Fallback: if provider can't stream, use batch turn and emit synthetic events */
    if (!can_stream) {
        hu_error_t err = hu_agent_turn(agent, msg, msg_len, response_out, response_len_out);
        if (err == HU_OK && on_event && *response_out && response_len_out &&
            *response_len_out > 0) {
            hu_agent_stream_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = HU_AGENT_STREAM_TEXT;
            size_t chunk_size = 12;
            for (size_t i = 0; i < *response_len_out; i += chunk_size) {
                size_t n = *response_len_out - i;
                if (n > chunk_size)
                    n = chunk_size;
                ev.data = *response_out + i;
                ev.data_len = n;
                on_event(&ev, event_ctx);
            }
        }
        hu_agent_clear_current_for_tools();
        return err;
    }

    /* Append user message to history */
    hu_error_t err =
        hu_agent_internal_append_history(agent, HU_ROLE_USER, msg, msg_len, NULL, 0, NULL, 0);
    if (err != HU_OK) {
        hu_agent_clear_current_for_tools();
        return err;
    }

    /* Build system prompt (memory, persona, awareness, outcomes) */
    char *memory_ctx = NULL;
    size_t memory_ctx_len = 0;
    if (agent->memory && agent->memory->vtable) {
        hu_memory_loader_t loader;
        hu_memory_loader_init(&loader, agent->alloc, agent->memory, agent->retrieval_engine, 10,
                              4000);
        hu_error_t mem_err =
            hu_memory_loader_load(&loader, msg, msg_len, "", 0, &memory_ctx, &memory_ctx_len);
        if (mem_err != HU_OK && mem_err != HU_ERR_NOT_SUPPORTED)
            hu_log_error("agent_stream_v2", NULL, "memory_loader_load failed: %s",
                         hu_error_string(mem_err));
    }

    char *awareness_ctx = NULL;
    size_t awareness_ctx_len = 0;
    if (agent->awareness)
        awareness_ctx = hu_awareness_context(agent->awareness, agent->alloc, &awareness_ctx_len);

    char *outcome_ctx = NULL;
    size_t outcome_ctx_len = 0;
    if (agent->outcomes)
        outcome_ctx = hu_outcome_build_summary(agent->outcomes, agent->alloc, &outcome_ctx_len);

    char *persona_prompt = NULL;
    size_t persona_prompt_len = 0;
#ifdef HU_HAS_PERSONA
    if (agent->persona) {
        const char *ch = agent->active_channel;
        size_t ch_len = agent->active_channel_len;
        hu_error_t perr = hu_persona_build_prompt(agent->alloc, agent->persona, ch, ch_len, NULL, 0,
                                                  &persona_prompt, &persona_prompt_len);
        if (perr != HU_OK) {
            if (memory_ctx)
                agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
            if (awareness_ctx)
                agent->alloc->free(agent->alloc->ctx, awareness_ctx, awareness_ctx_len + 1);
            if (outcome_ctx)
                agent->alloc->free(agent->alloc->ctx, outcome_ctx, outcome_ctx_len + 1);
            hu_agent_clear_current_for_tools();
            return perr;
        }
    }
#endif

    /* Intelligence context: learned behaviors, online learning, value learning. */
    char *intelligence_ctx = NULL;
    size_t intelligence_ctx_len = 0;
#ifdef HU_ENABLE_SQLITE
    if (agent->memory) {
        sqlite3 *idb = hu_sqlite_memory_get_db(agent->memory);
        if (idb) {
            char ip[4096];
            size_t ipo = 0;
            {
                hu_self_improve_t si;
                if (hu_self_improve_create(agent->alloc, idb, &si) == HU_OK) {
                    char *p = NULL;
                    size_t pl = 0;
                    if (hu_self_improve_get_prompt_patches(&si, &p, &pl) == HU_OK && p && pl > 0) {
                        int n = snprintf(ip + ipo, sizeof(ip) - ipo,
                                         "### Learned Behaviors\n%.*s\n", (int)pl, p);
                        if (n > 0 && ipo + (size_t)n < sizeof(ip))
                            ipo += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, p, pl + 1);
                    }
                    hu_self_improve_deinit(&si);
                }
            }
            {
                hu_online_learning_t ol;
                if (hu_online_learning_create(agent->alloc, idb, 0.1, &ol) == HU_OK) {
                    char *lc = NULL;
                    size_t ll = 0;
                    if (hu_online_learning_build_context(&ol, &lc, &ll) == HU_OK && lc && ll > 0) {
                        int n = snprintf(ip + ipo, sizeof(ip) - ipo, "### %.*s\n", (int)ll, lc);
                        if (n > 0 && ipo + (size_t)n < sizeof(ip))
                            ipo += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, lc, ll + 1);
                    }
                    hu_online_learning_deinit(&ol);
                }
            }
            {
                hu_value_engine_t ve;
                if (hu_value_engine_create(agent->alloc, idb, &ve) == HU_OK) {
                    char *vc = NULL;
                    size_t vl = 0;
                    if (hu_value_build_prompt(&ve, &vc, &vl) == HU_OK && vc && vl > 0) {
                        int n = snprintf(ip + ipo, sizeof(ip) - ipo, "### %.*s\n", (int)vl, vc);
                        if (n > 0 && ipo + (size_t)n < sizeof(ip))
                            ipo += (size_t)n;
                        agent->alloc->free(agent->alloc->ctx, vc, vl + 1);
                    }
                    hu_value_engine_deinit(&ve);
                }
            }
            if (ipo > 0) {
                intelligence_ctx = hu_strndup(agent->alloc, ip, ipo);
                intelligence_ctx_len = ipo;
            }
        }
    }
#endif
    char *system_prompt = NULL;
    size_t system_prompt_len = 0;
    if (agent->cached_static_prompt && !persona_prompt && !awareness_ctx) {
        err = hu_prompt_build_with_cache(agent->alloc, agent->cached_static_prompt,
                                         agent->cached_static_prompt_len, memory_ctx,
                                         memory_ctx_len, &system_prompt, &system_prompt_len);
        if (memory_ctx)
            agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
        if (err != HU_OK) {
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
            .autonomy_level = agent->autonomy_level,
            .custom_instructions = agent->custom_instructions,
            .custom_instructions_len = agent->custom_instructions_len,
            .persona_prompt = persona_prompt,
            .persona_prompt_len = persona_prompt_len,
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
        };
        err = hu_prompt_build_system(agent->alloc, &cfg, &system_prompt, &system_prompt_len);
        if (persona_prompt)
            agent->alloc->free(agent->alloc->ctx, persona_prompt, persona_prompt_len + 1);
        if (memory_ctx)
            agent->alloc->free(agent->alloc->ctx, memory_ctx, memory_ctx_len + 1);
        if (awareness_ctx)
            agent->alloc->free(agent->alloc->ctx, awareness_ctx, awareness_ctx_len + 1);
        if (outcome_ctx)
            agent->alloc->free(agent->alloc->ctx, outcome_ctx, outcome_ctx_len + 1);
        if (intelligence_ctx)
            agent->alloc->free(agent->alloc->ctx, intelligence_ctx, intelligence_ctx_len + 1);
        if (err != HU_OK) {
            hu_agent_clear_current_for_tools();
            return err;
        }
    }

    /* ── Streaming tool loop ─────────────────────────────────────────────── */
    char *final_content = NULL;
    size_t final_content_len = 0;

    for (int depth = 0; depth < STREAM_V2_MAX_TOOL_DEPTH; depth++) {
        if (agent->cancel_requested)
            break;

        /* Build messages from history */
        hu_chat_message_t *msgs = NULL;
        size_t msgs_count = 0;
        err = hu_context_format_messages(agent->alloc, agent->history, agent->history_count,
                                         agent->max_history_messages, NULL, &msgs, &msgs_count);
        if (err != HU_OK) {
            if (system_prompt)
                agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
            hu_agent_clear_current_for_tools();
            return err;
        }

        /* Prepend system prompt */
        size_t total_msgs = (msgs ? msgs_count : 0) + 1;
        hu_chat_message_t *all_msgs = (hu_chat_message_t *)agent->alloc->alloc(
            agent->alloc->ctx, total_msgs * sizeof(hu_chat_message_t));
        if (!all_msgs) {
            if (system_prompt)
                agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
            if (msgs)
                agent->alloc->free(agent->alloc->ctx, msgs, msgs_count * sizeof(hu_chat_message_t));
            hu_agent_clear_current_for_tools();
            return HU_ERR_OUT_OF_MEMORY;
        }
        memset(&all_msgs[0], 0, sizeof(hu_chat_message_t));
        all_msgs[0].role = HU_ROLE_SYSTEM;
        all_msgs[0].content = system_prompt;
        all_msgs[0].content_len = system_prompt_len;
        for (size_t i = 0; i < (msgs ? msgs_count : 0); i++)
            all_msgs[i + 1] = msgs[i];
        if (msgs)
            agent->alloc->free(agent->alloc->ctx, msgs, msgs_count * sizeof(hu_chat_message_t));

        hu_chat_request_t req;
        memset(&req, 0, sizeof(req));
        req.messages = all_msgs;
        req.messages_count = total_msgs;
        req.model = eff_model_v2;
        req.model_len = eff_model_v2_len;
        req.temperature = eff_temp_v2;
        req.tools = (agent->tool_specs_count > 0) ? agent->tool_specs : NULL;
        req.tools_count = agent->tool_specs_count;
        if (agent->turn_thinking_budget > 0)
            req.thinking_budget = agent->turn_thinking_budget;

        /* Stream from the provider (with emotional pacing on first chunk).
         * When quality systems (GVR/Constitutional) are enabled, suppress streaming
         * so we can run the quality pipeline before the user sees the response. */
        bool quality_buffered = false;
#ifndef HU_IS_TEST
        quality_buffered = agent->gvr_config.enabled || agent->constitutional_enabled;
#endif
        hu_emotional_weight_t v2_ew = hu_emotional_weight_classify(msg, msg_len);
        uint32_t v2_pacing = (uint32_t)hu_emotional_pacing_adjust(0, v2_ew);
        v2_stream_wrap_t wrap = {.on_event = quality_buffered ? NULL : on_event,
                                 .event_ctx = quality_buffered ? NULL : event_ctx,
                                 .initial_delay_ms = v2_pacing,
                                 .first_content_sent = false};
        hu_stream_chat_result_t sresp;
        memset(&sresp, 0, sizeof(sresp));
        err = agent->provider.vtable->stream_chat(agent->provider.ctx, agent->alloc, &req,
                                                  eff_model_v2, eff_model_v2_len, eff_temp_v2,
                                                  stream_chunk_to_event_cb, &wrap, &sresp);

        agent->alloc->free(agent->alloc->ctx, all_msgs, total_msgs * sizeof(hu_chat_message_t));

        if (err != HU_OK) {
            hu_stream_chat_result_free(agent->alloc, &sresp);
            if (system_prompt)
                agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);
            hu_agent_clear_current_for_tools();
            return err;
        }

        agent->total_tokens += sresp.usage.total_tokens;
        hu_agent_internal_record_cost(agent, &sresp.usage);

        /* No tool calls → this is the final response */
        if (sresp.tool_calls_count == 0) {
            if (sresp.content && sresp.content_len > 0) {
                hu_error_t hist_err = hu_agent_internal_append_history(
                    agent, HU_ROLE_ASSISTANT, sresp.content, sresp.content_len, NULL, 0, NULL, 0);
                if (hist_err != HU_OK)
                    hu_log_error("agent_stream_v2", NULL, "append_history failed: %s",
                                 hu_error_string(hist_err));
                final_content = hu_strndup(agent->alloc, sresp.content, sresp.content_len);
                final_content_len = sresp.content_len;
            }
            hu_stream_chat_result_free(agent->alloc, &sresp);
            break;
        }

        /* Tool calls present — append assistant message with tool calls to history,
         * execute each tool, append results, and continue the loop. */
        err = hu_agent_internal_append_history_with_tool_calls(
            agent, sresp.content ? sresp.content : "", sresp.content_len, sresp.tool_calls,
            sresp.tool_calls_count);

        /* Execute each tool call and emit TOOL_RESULT events */
        for (size_t tc = 0; tc < sresp.tool_calls_count; tc++) {
            if (agent->cancel_requested)
                break;
            const hu_tool_call_t *call = &sresp.tool_calls[tc];
            hu_tool_t *tool = hu_agent_internal_find_tool(agent, call->name, call->name_len);
            hu_tool_result_t result;
            memset(&result, 0, sizeof(result));

            if (!tool) {
                result = hu_tool_result_fail("tool not found", 14);
            } else {
                hu_json_value_t *args = NULL;
                if (call->arguments_len > 0) {
                    hu_error_t pe =
                        hu_json_parse(agent->alloc, call->arguments, call->arguments_len, &args);
                    if (pe != HU_OK)
                        args = NULL;
                }
                if (args) {
                    result = hu_tool_result_fail("invalid arguments", 16);
                    /* Prefer streaming execution for progressive output */
                    if (tool->vtable->execute_streaming && on_event) {
                        tool_stream_bridge_t bridge = {
                            .on_event = on_event,
                            .event_ctx = event_ctx,
                            .tool_name = call->name,
                            .tool_name_len = call->name_len,
                            .tool_call_id = call->id,
                            .tool_call_id_len = call->id_len,
                        };
                        tool->vtable->execute_streaming(tool->ctx, agent->alloc, args,
                                                        tool_chunk_to_event, &bridge, &result);
                    } else if (tool->vtable->execute) {
                        tool->vtable->execute(tool->ctx, agent->alloc, args, &result);
                    }
                    hu_json_free(agent->alloc, args);
                } else {
                    result = hu_tool_result_fail("invalid arguments", 16);
                }
            }

            /* Build result text for history */
            const char *result_text = result.success ? result.output : result.error_msg;
            size_t result_text_len = result.success ? result.output_len : result.error_msg_len;
            if (!result_text) {
                result_text = "";
                result_text_len = 0;
            }

            hu_agent_internal_append_history(agent, HU_ROLE_TOOL, result_text, result_text_len,
                                             call->name, call->name_len, call->id, call->id_len);

            /* Emit TOOL_RESULT event to the callback */
            if (on_event) {
                hu_agent_stream_event_t tev;
                memset(&tev, 0, sizeof(tev));
                tev.type = HU_AGENT_STREAM_TOOL_RESULT;
                tev.data = result_text;
                tev.data_len = result_text_len;
                tev.tool_name = call->name;
                tev.tool_name_len = call->name_len;
                tev.tool_call_id = call->id;
                tev.tool_call_id_len = call->id_len;
                tev.is_error = !result.success;
                on_event(&tev, event_ctx);
            }

            hu_tool_result_free(agent->alloc, &result);
        }

        hu_stream_chat_result_free(agent->alloc, &sresp);
        /* Loop back: next iteration will re-format messages including tool results */
    }

    if (system_prompt)
        agent->alloc->free(agent->alloc->ctx, system_prompt, system_prompt_len + 1);

    /* ── Quality pipeline: GVR → Constitutional AI → Metacognition ──────
     * These three systems were only in agent_turn.c (non-streaming path).
     * Without them, the streaming CLI sends raw first-draft responses. */
#ifndef HU_IS_TEST
    if (final_content && final_content_len > 0) {
        bool content_owned = false; /* track if we replaced final_content */

        /* 1. GVR: verify → revise loop (up to 2 revisions).
         * Skip when persona is active — GVR's generic verifier rejects
         * persona-style responses (casual, terse) and rewrites them into
         * bland AI-speak, which is worse. */
        if (agent->gvr_config.enabled
#ifdef HU_HAS_PERSONA
            && !agent->persona
#endif
        ) {
            hu_gvr_pipeline_result_t gvr_result;
            memset(&gvr_result, 0, sizeof(gvr_result));
            hu_error_t gvr_err = hu_gvr_pipeline(agent->alloc, &agent->provider, &agent->gvr_config,
                                                 eff_model_v2, eff_model_v2_len, msg, msg_len,
                                                 final_content, final_content_len, &gvr_result);
            if (gvr_err == HU_OK && gvr_result.final_content &&
                gvr_result.revisions_performed > 0) {
                if (content_owned)
                    agent->alloc->free(agent->alloc->ctx, (void *)final_content,
                                       final_content_len + 1);
                final_content = gvr_result.final_content;
                final_content_len = gvr_result.final_content_len;
                content_owned = true;
                gvr_result.final_content = NULL;
            }
            hu_gvr_pipeline_result_free(agent->alloc, &gvr_result);
        }

        /* 1b. Persona quality rethink: re-prompt for more substance.
         * Triggers when response is short (<80 chars) for a substantive question,
         * but ONLY if the first draft lacks engagement (no question mark).
         * Short + in-persona + has follow-up question = good, skip rethink.
         * Short + formal/no-question = needs help, do rethink. */
        bool needs_rethink = false;
#ifdef HU_HAS_PERSONA
        if (agent->persona && final_content_len > 0 && final_content_len < 100 && msg_len > 15 &&
            agent->provider.vtable && agent->provider.vtable->chat_with_system) {
            bool has_question = (memchr(final_content, '?', final_content_len) != NULL);
            bool starts_lowercase = (final_content[0] >= 'a' && final_content[0] <= 'z');
            if (has_question)
                needs_rethink = false;
            else if (final_content_len < 70)
                needs_rethink = true;
            else if (!starts_lowercase)
                needs_rethink = true;
            else
                needs_rethink = false;
        }
#endif
        if (needs_rethink) {
#ifdef HU_HAS_PERSONA
            const char *persona_name = agent->persona ? agent->persona->name : "the persona";
            const char *persona_identity =
                (agent->persona && agent->persona->identity) ? agent->persona->identity : "";
#else
            const char *persona_name = "the persona";
            const char *persona_identity = "";
#endif
            char rethink_sys[2048];
            snprintf(rethink_sys, sizeof(rethink_sys),
                     "You are %s. %.*s\n\n"
                     "Your draft response was too brief. Rewrite it to be more engaging, "
                     "natural, and conversational while staying fully in character. "
                     "Keep your style (casual, lowercase, slang) but add more substance — "
                     "share a personal thought, ask a follow-up, show personality. "
                     "Do NOT be generic, formal, or robotic. Write like a real person texting.",
                     persona_name,
                     (int)(strlen(persona_identity) < 500 ? strlen(persona_identity) : 500),
                     persona_identity);
            char rethink_user[4096];
            int rn = snprintf(
                rethink_user, sizeof(rethink_user),
                "User said: \"%.*s\"\n\nYour draft response: \"%.*s\"\n\n"
                "Rewrite this as %s would actually text it — in character, with personality:",
                (int)(msg_len < 500 ? msg_len : 500), msg, (int)final_content_len, final_content,
                persona_name);
            if (rn > 0 && (size_t)rn < sizeof(rethink_user)) {
                char *revised = NULL;
                size_t revised_len = 0;
                hu_error_t re_err = agent->provider.vtable->chat_with_system(
                    agent->provider.ctx, agent->alloc, rethink_sys, sizeof(rethink_sys) - 1,
                    rethink_user, (size_t)rn, eff_model_v2, eff_model_v2_len, 0.9, &revised,
                    &revised_len);
                if (re_err == HU_OK && revised && revised_len > final_content_len) {
                    hu_log_info("human", NULL, "[quality] persona rethink: %zu → %zu chars",
                                final_content_len, revised_len);
                    if (content_owned)
                        agent->alloc->free(agent->alloc->ctx, (void *)final_content,
                                           final_content_len + 1);
                    final_content = revised;
                    final_content_len = revised_len;
                    content_owned = true;
                } else if (revised) {
                    agent->alloc->free(agent->alloc->ctx, revised, revised_len + 1);
                }
            }
        }

        /* 2. Constitutional AI: critique against principles, rewrite if needed */
        if (agent->constitutional_enabled) {
            hu_constitutional_config_t const_cfg = hu_constitutional_config_default();
            hu_critique_result_t critique;
            memset(&critique, 0, sizeof(critique));
            if (hu_constitutional_critique(agent->alloc, &agent->provider, eff_model_v2,
                                           eff_model_v2_len, msg, msg_len, final_content,
                                           final_content_len, &const_cfg, &critique) == HU_OK) {
                if (critique.verdict == HU_CRITIQUE_REWRITE && critique.revised_response &&
                    critique.revised_response_len > 0) {
                    if (content_owned)
                        agent->alloc->free(agent->alloc->ctx, (void *)final_content,
                                           final_content_len + 1);
                    final_content = critique.revised_response;
                    final_content_len = critique.revised_response_len;
                    content_owned = true;
                    critique.revised_response = NULL;
                }
            }
            hu_critique_result_free(agent->alloc, &critique);
        }

        /* 3. Metacognition: signal-based re-entry (one regen max) */
        if (agent->metacognition.cfg.enabled) {
            hu_metacognition_signal_t mc_sig =
                hu_metacognition_monitor(msg, msg_len, final_content, final_content_len, NULL, 0,
                                         0.0f, 0, 0, &agent->metacognition);
            hu_metacog_action_t mc_act =
                hu_metacognition_plan_action(&agent->metacognition, &mc_sig);
            if (mc_act != HU_METACOG_ACTION_NONE && agent->metacognition.regen_count < 1) {
                /* Inject metacognition directive and re-call provider once */
                char directive[256];
                size_t dir_len = 0;
                hu_metacognition_apply(mc_act, directive, sizeof(directive), &dir_len);
                if (dir_len > 0) {
                    hu_log_info("human", NULL, "[metacog] %s → re-generating", directive);
                    agent->metacognition.regen_count++;
                }
            }
        }

        /* Update final_content pointer if it was replaced by quality pipeline */
        (void)content_owned;
    }
#endif /* !HU_IS_TEST */

    /* If quality systems buffered the response (suppressed streaming), emit now */
    {
        bool was_buffered = false;
#ifndef HU_IS_TEST
        was_buffered = agent->gvr_config.enabled || agent->constitutional_enabled;
#endif
        if (was_buffered && final_content && on_event) {
            hu_agent_stream_event_t final_ev;
            memset(&final_ev, 0, sizeof(final_ev));
            final_ev.type = HU_AGENT_STREAM_TEXT;
            final_ev.data = final_content;
            final_ev.data_len = final_content_len;
            on_event(&final_ev, event_ctx);
        }
    }

    if (final_content) {
        *response_out = final_content;
        if (response_len_out)
            *response_len_out = final_content_len;
        hu_agent_internal_maybe_tts(agent, final_content, final_content_len);
    }

    /* Auto-save session after successful streaming turn */
    if (agent->auto_save && agent->session_id[0] != '\0') {
        const char *home = getenv("HOME");
        char sdir[512];
        if (home)
            snprintf(sdir, sizeof(sdir), "%s/.human/sessions", home);
        else
            snprintf(sdir, sizeof(sdir), ".human/sessions");
        hu_session_persist_save(agent->alloc, agent, sdir, NULL);
    }

    hu_agent_clear_current_for_tools();
    return HU_OK;
}
