/*
 * Streaming voice session: binary mic chunks, STT, bus → agent, Cartesia TTS → binary PCM.
 * Gemini Live mode: forward raw PCM16 directly, receive native audio responses.
 * Micro-turn duplex FSM drives turn-taking and control-token stripping.
 */
#include "cp_internal.h"
#include "human/bus.h"
#include "human/config.h"
#include "human/core/log.h"
#include "human/core/string.h"
#include "human/gateway/voice_stream.h"
#include "human/multimodal.h"
#include "human/platform.h"
#include "human/tool.h"
#include "human/tts/cartesia_stream.h"
#include "human/voice.h"
#include "human/voice/audio_emotion.h"
#include "human/voice/duplex.h"
#include "human/voice/emotion_voice_map.h"
#include "human/voice/provider.h"
#include "human/voice/semantic_eot.h"
#include "human/voice/turn_signal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#ifdef HU_GATEWAY_POSIX

#include <time.h>

#define VS_MAX_SLOTS 8
#define VS_MAX_AUDIO (10u * 1024u * 1024u)

typedef struct {
    bool in_use;
    uint64_t conn_id;
    hu_ws_conn_t *conn;
    uint8_t *pcm_buf;
    size_t pcm_len;
    size_t pcm_cap;
    hu_cartesia_stream_t *tts;
    char session_key[HU_BUS_ID_LEN];
    char tts_context[96];
    bool tts_armed;
    unsigned turn_counter;
    char voice_id[128];
    char model_id[128];
    hu_duplex_session_t duplex;
    hu_voice_emotion_t current_emotion;
    hu_voice_params_t emotion_voice_params;
    /* Voice provider (vtable-based backend — Gemini Live or OpenAI Realtime) */
    bool provider_mode;
    bool vad_active; /* true between activityStart and activityEnd for manual VAD */
    hu_voice_provider_t provider;
    uint64_t goaway_reconnect_at_ms; /* non-zero: defer reconnect until this timestamp */
    int synth_seq; /* per-slot counter for synthetic tool call IDs */
} vs_slot_t;

static vs_slot_t s_vs[VS_MAX_SLOTS];
static hu_control_protocol_t *s_proto;
static hu_bus_t *s_bus;
static vs_slot_t *s_active_tts_slot;

static int64_t vs_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

static hu_allocator_t *vs_alloc_for_close(void) {
    if (s_proto && s_proto->alloc)
        return s_proto->alloc;
    static hu_allocator_t sys;
    static bool sys_init;
    if (!sys_init) {
        sys = hu_system_allocator();
        sys_init = true;
    }
    return &sys;
}

static const char *vs_mime_ext(const char *mime) {
    if (!mime)
        return ".webm";
    if (strstr(mime, "wav"))
        return ".wav";
    if (strstr(mime, "mp3") || strstr(mime, "mpeg"))
        return ".mp3";
    if (strstr(mime, "ogg"))
        return ".ogg";
    return ".webm";
}

static vs_slot_t *vs_find_slot_by_conn(const hu_ws_conn_t *c) {
    if (!c)
        return NULL;
    for (int i = 0; i < VS_MAX_SLOTS; i++) {
        if (s_vs[i].in_use && s_vs[i].conn_id == c->id)
            return &s_vs[i];
    }
    return NULL;
}

static vs_slot_t *vs_alloc_slot(hu_ws_conn_t *c) {
    vs_slot_t *ex = vs_find_slot_by_conn(c);
    if (ex)
        return ex;
    for (int i = 0; i < VS_MAX_SLOTS; i++) {
        if (!s_vs[i].in_use) {
            memset(&s_vs[i], 0, sizeof(s_vs[i]));
            s_vs[i].in_use = true;
            s_vs[i].conn_id = c->id;
            s_vs[i].conn = c;
            (void)snprintf(s_vs[i].session_key, sizeof(s_vs[i].session_key), "voice-%llu",
                           (unsigned long long)c->id);
            hu_duplex_session_init(&s_vs[i].duplex);
            return &s_vs[i];
        }
    }
    return NULL;
}

static void vs_free_slot(vs_slot_t *sl, hu_allocator_t *alloc) {
    if (!sl)
        return;
    if (s_active_tts_slot == sl)
        s_active_tts_slot = NULL;
    if (sl->provider.vtable) {
        sl->provider.vtable->disconnect(sl->provider.ctx, alloc);
        sl->provider.vtable = NULL;
        sl->provider.ctx = NULL;
    }
    if (sl->tts) {
        hu_cartesia_stream_close(sl->tts, alloc);
        sl->tts = NULL;
    }
    if (sl->pcm_buf) {
        alloc->free(alloc->ctx, sl->pcm_buf, sl->pcm_cap);
        sl->pcm_buf = NULL;
        sl->pcm_len = 0;
        sl->pcm_cap = 0;
    }
    memset(sl, 0, sizeof(*sl));
}

static void vs_drain_tts_to_conn(vs_slot_t *sl) {
    if (!sl || !sl->tts || !s_proto || !s_proto->ws || !sl->conn || !sl->conn->active)
        return;
    hu_allocator_t *a = s_proto->alloc;
    for (int iter = 0; iter < 4096; iter++) {
        if (!sl->conn->active)
            break;
        void *pcm = NULL;
        size_t n = 0;
        bool done = false;
        hu_error_t err = hu_cartesia_stream_recv_next(sl->tts, a, &pcm, &n, &done);
        if (err != HU_OK) {
            if (err != HU_ERR_WOULD_BLOCK && sl->conn->active)
                hu_control_send_event_to_conn(s_proto, sl->conn, "voice.error",
                                              "{\"message\":\"TTS stream error\"}");
            break;
        }
        if (pcm && n > 0)
            (void)hu_ws_server_send_binary(s_proto->ws, sl->conn, (const char *)pcm, n);
        if (pcm)
            a->free(a->ctx, pcm, n);
        if (done)
            break;
        if (!pcm)
            break;
    }
}

static void vs_finish_agent_turn(vs_slot_t *sl, hu_allocator_t *a) {
    (void)hu_cartesia_stream_flush_context(sl->tts, a, sl->tts_context);
    vs_drain_tts_to_conn(sl);
    hu_control_send_event_to_conn(s_proto, sl->conn, "voice.audio.done", "{}");
    sl->tts_armed = false;
    if (s_active_tts_slot == sl)
        s_active_tts_slot = NULL;
}

static vs_slot_t *vs_find_armed_slot_for_event(const hu_bus_event_t *ev) {
    if (!ev)
        return NULL;
    for (int i = 0; i < VS_MAX_SLOTS; i++) {
        if (s_vs[i].in_use && s_vs[i].tts_armed && s_vs[i].tts &&
            strcmp(ev->id, s_vs[i].session_key) == 0)
            return &s_vs[i];
    }
    return NULL;
}

static bool vs_bus_cb(hu_bus_event_type_t type, const hu_bus_event_t *ev, void *user_ctx) {
    (void)user_ctx;
    if (!s_proto || !ev)
        return true;
    vs_slot_t *sl = vs_find_armed_slot_for_event(ev);
    if (!sl)
        return true;

    hu_allocator_t *a = s_proto->alloc;
    int64_t now = vs_now_ms();

    switch (type) {
    case HU_BUS_THINKING_CHUNK:
        /* Model reasoning; never feed to Cartesia or advance duplex from this stream. */
        return true;

    case HU_BUS_TOOL_CALL:
        /* Tool invocation metadata only; do not speak or disturb TTS context. */
        return true;

    case HU_BUS_TOOL_CALL_RESULT:
        /* Raw tool output; spoken text arrives in subsequent HU_BUS_MESSAGE_CHUNK. */
        return true;

    case HU_BUS_MESSAGE_CHUNK: {
        const char *tok = ev->message[0] ? ev->message : NULL;
        if (!tok || !tok[0])
            return true;

        size_t tok_len = strlen(tok);
        hu_turn_signal_result_t sig;
        hu_turn_signal_extract(tok, tok_len, &sig);

        hu_turn_signal_t fsm_signal = sig.had_token ? sig.signal : HU_TURN_SIGNAL_CONTINUE;
        hu_turn_action_t action;
        hu_duplex_agent_chunk(&sl->duplex, now, fsm_signal, &action);

        if (action == HU_TURN_ACTION_FLUSH_AUDIO) {
            char stripped[HU_BUS_MSG_LEN];
            const char *tts_text = tok;
            if (sig.had_token) {
                size_t slen = hu_turn_signal_strip(tok, tok_len, stripped, sizeof(stripped));
                if (slen > 0)
                    tts_text = stripped;
                else
                    return true;
            }
            /* Emotion-aware TTS: detect emotion and apply voice controls to Cartesia */
            {
                hu_voice_emotion_t emo = HU_VOICE_EMOTION_NEUTRAL;
                float emo_conf = 0.0f;
                if (hu_emotion_detect_from_text(tts_text, strlen(tts_text), &emo, &emo_conf) ==
                        HU_OK &&
                    emo_conf > 0.3f && emo != sl->current_emotion) {
                    sl->current_emotion = emo;
                    sl->emotion_voice_params = hu_emotion_voice_map(emo);
                    static const char *const cartesia_emotions[] = {
                        [HU_VOICE_EMOTION_NEUTRAL] = NULL,
                        [HU_VOICE_EMOTION_JOY] = "positivity:high",
                        [HU_VOICE_EMOTION_SADNESS] = "sadness:high",
                        [HU_VOICE_EMOTION_EMPATHY] = "sadness:low",
                        [HU_VOICE_EMOTION_EXCITEMENT] = "surprise:high",
                        [HU_VOICE_EMOTION_CONCERN] = "curiosity:high",
                        [HU_VOICE_EMOTION_CALM] = "positivity:low",
                        [HU_VOICE_EMOTION_URGENCY] = "anger:low",
                    };
                    hu_cartesia_voice_controls_t ctrl = {0};
                    ctrl.speed = sl->emotion_voice_params.rate_factor - 1.0f;
                    ctrl.emotion_intensity = sl->emotion_voice_params.emphasis;
                    if ((size_t)emo < sizeof(cartesia_emotions) / sizeof(cartesia_emotions[0]))
                        ctrl.emotion = cartesia_emotions[emo];
                    hu_cartesia_stream_set_voice_controls(sl->tts, &ctrl);
                }
            }
            if (hu_cartesia_stream_send_generation(sl->tts, a, sl->tts_context, tts_text, true) ==
                HU_OK)
                vs_drain_tts_to_conn(sl);
        } else if (action == HU_TURN_ACTION_YIELD_FLOOR) {
            vs_finish_agent_turn(sl, a);
        }
        return true;
    }

    case HU_BUS_MESSAGE_SENT: {
        hu_turn_action_t action;
        hu_duplex_agent_chunk(&sl->duplex, now, HU_TURN_SIGNAL_YIELD, &action);
        vs_finish_agent_turn(sl, a);
        return true;
    }

    default:
        /* Other bus events (e.g. MESSAGE_RECEIVED, ERROR): no voice/TTS side effects. */
        return true;
    }
}

void hu_voice_stream_attach_bus(hu_bus_t *bus, hu_control_protocol_t *proto) {
    s_bus = bus;
    s_proto = proto;
    if (bus)
        (void)hu_bus_subscribe(bus, vs_bus_cb, NULL, HU_BUS_EVENT_COUNT);
}

void hu_voice_stream_detach_bus(hu_bus_t *bus) {
    if (bus && s_bus == bus) {
        hu_bus_unsubscribe(bus, vs_bus_cb, NULL);
        s_bus = NULL;
        s_proto = NULL;
        s_active_tts_slot = NULL;
    }
}

void hu_voice_stream_on_binary(hu_control_protocol_t *proto, hu_ws_conn_t *conn, const char *data,
                               size_t data_len) {
    if (!proto || !proto->alloc || !conn || !data || data_len == 0)
        return;
    vs_slot_t *sl = vs_find_slot_by_conn(conn);
    if (!sl)
        return;

    /* Provider mode: forward raw PCM16 directly to the voice backend */
    if (sl->provider_mode && sl->provider.vtable) {
        if (!sl->vad_active) {
            hu_error_t aerr = sl->provider.vtable->send_activity_start(sl->provider.ctx);
            if (aerr != HU_OK) {
                hu_log_warn("voice-stream", NULL, "send_activity_start failed: %s",
                            hu_error_string(aerr));
                return;
            }
            sl->vad_active = true;
        }
        hu_error_t serr = sl->provider.vtable->send_audio(sl->provider.ctx, data, data_len);
        if (serr != HU_OK)
            hu_log_warn("voice-stream", NULL, "send_audio failed: %s", hu_error_string(serr));
        return;
    }

    hu_allocator_t *alloc = proto->alloc;
    if (sl->pcm_len + data_len > VS_MAX_AUDIO)
        return;
    if (sl->pcm_len + data_len > sl->pcm_cap) {
        size_t ncap = sl->pcm_cap ? sl->pcm_cap * 2u : 65536u;
        while (ncap < sl->pcm_len + data_len)
            ncap *= 2u;
        if (ncap > VS_MAX_AUDIO)
            ncap = VS_MAX_AUDIO;
        uint8_t *nb = (uint8_t *)alloc->alloc(alloc->ctx, ncap);
        if (!nb)
            return;
        if (sl->pcm_buf && sl->pcm_len)
            memcpy(nb, sl->pcm_buf, sl->pcm_len);
        if (sl->pcm_buf)
            alloc->free(alloc->ctx, sl->pcm_buf, sl->pcm_cap);
        sl->pcm_buf = nb;
        sl->pcm_cap = ncap;
    }
    memcpy(sl->pcm_buf + sl->pcm_len, data, data_len);
    sl->pcm_len += data_len;
}

void hu_voice_stream_on_conn_close(hu_ws_conn_t *conn) {
    vs_slot_t *sl = vs_find_slot_by_conn(conn);
    if (!sl)
        return;
    vs_free_slot(sl, vs_alloc_for_close());
}

hu_error_t cp_voice_session_start(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                  const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                  char **out, size_t *out_len) {
    if (!out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;
    if (!alloc || !app || !app->config || !conn || !proto || !root)
        return HU_ERR_INVALID_ARGUMENT;

    vs_slot_t *sl = vs_alloc_slot(conn);
    if (!sl)
        return HU_ERR_PROVIDER_UNAVAILABLE;

    const hu_config_t *cfg = (const hu_config_t *)app->config;

    const char *voice_id = NULL;
    const char *model_id = NULL;
    const char *mode = NULL;
    hu_json_value_t *params = hu_json_object_get((hu_json_value_t *)root, "params");
    if (params) {
        const char *v = hu_json_get_string(params, "voice_id");
        if (v)
            voice_id = v;
        const char *m = hu_json_get_string(params, "model_id");
        if (m)
            model_id = m;
        const char *md = hu_json_get_string(params, "mode");
        if (md)
            mode = md;
    }
    if (!voice_id || !voice_id[0])
        voice_id = cfg->voice.tts_voice;
    if (!model_id || !model_id[0])
        model_id = cfg->voice.tts_model;

    /* Gemini Live mode: native end-to-end voice with no STT/TTS pipeline */
    bool use_gemini_live = (mode && strcmp(mode, "gemini_live") == 0) ||
                           (cfg->voice.mode && strcmp(cfg->voice.mode, "gemini_live") == 0);

    if (use_gemini_live) {
        /* Build tool declarations JSON array for the setup message */
        char *tools_json_str = NULL;
        if (app->tools && app->tools_count > 0) {
            hu_json_buf_t tbuf = {0};
            hu_error_t terr = hu_json_buf_init(&tbuf, alloc);
            if (terr == HU_OK) {
                terr = hu_json_buf_append_raw(&tbuf, "[", 1);
                size_t added = 0;
                for (size_t ti = 0; ti < app->tools_count && terr == HU_OK; ti++) {
                    hu_tool_t *t = &app->tools[ti];
                    if (!t->vtable || !t->vtable->name)
                        continue;
                    const char *tn = t->vtable->name(t->ctx);
                    const char *td = t->vtable->description ? t->vtable->description(t->ctx) : "";
                    const char *tp =
                        t->vtable->parameters_json ? t->vtable->parameters_json(t->ctx) : "{}";
                    if (!tn)
                        continue;
                    if (added > 0)
                        terr = hu_json_buf_append_raw(&tbuf, ",", 1);
                    if (terr == HU_OK)
                        terr = hu_json_buf_append_raw(&tbuf, "{\"name\":", 8);
                    if (terr == HU_OK)
                        terr = hu_json_append_string(&tbuf, tn, strlen(tn));
                    if (terr == HU_OK)
                        terr = hu_json_buf_append_raw(&tbuf, ",\"description\":", 15);
                    if (terr == HU_OK)
                        terr = hu_json_append_string(&tbuf, td ? td : "", td ? strlen(td) : 0);
                    if (terr == HU_OK)
                        terr = hu_json_buf_append_raw(&tbuf, ",\"parameters\":", 14);
                    if (terr == HU_OK)
                        terr = hu_json_buf_append_raw(&tbuf, tp && tp[0] ? tp : "{}",
                                                      strlen(tp && tp[0] ? tp : "{}"));
                    if (terr == HU_OK)
                        terr = hu_json_buf_append_raw(&tbuf, "}", 1);
                    if (terr == HU_OK)
                        added++;
                }
                if (terr == HU_OK)
                    terr = hu_json_buf_append_raw(&tbuf, "]", 1);
                if (tbuf.len > 2)
                    tools_json_str = hu_strndup(alloc, tbuf.ptr, tbuf.len);
                hu_json_buf_free(&tbuf);
            }
        }

        const char *sys_instr = NULL;
        if (cfg->agent.persona && cfg->agent.persona[0])
            sys_instr = cfg->agent.persona;

        hu_voice_provider_extras_t extras = {
            .system_instruction = sys_instr,
            .tools_json = tools_json_str,
            .voice_id = (voice_id && voice_id[0]) ? voice_id : NULL,
            .model_id = (model_id && model_id[0]) ? model_id : NULL,
        };

        if (sl->provider.vtable) {
            sl->provider.vtable->disconnect(sl->provider.ctx, alloc);
            sl->provider.vtable = NULL;
            sl->provider.ctx = NULL;
        }

        hu_error_t gerr =
            hu_voice_provider_create_from_config(alloc, cfg, "gemini_live", &extras, &sl->provider);
        if (tools_json_str)
            alloc->free(alloc->ctx, tools_json_str, strlen(tools_json_str) + 1);
        if (gerr != HU_OK) {
            vs_free_slot(sl, alloc);
            return gerr;
        }
        gerr = sl->provider.vtable->connect(sl->provider.ctx);
        if (gerr != HU_OK) {
            sl->provider.vtable->disconnect(sl->provider.ctx, alloc);
            sl->provider.vtable = NULL;
            sl->provider.ctx = NULL;
            vs_free_slot(sl, alloc);
            return gerr;
        }

        sl->provider_mode = true;

        hu_json_value_t *res = hu_json_object_new(alloc);
        if (!res) {
            vs_free_slot(sl, alloc);
            return HU_ERR_OUT_OF_MEMORY;
        }
        hu_json_object_set(alloc, res, "ok", hu_json_bool_new(alloc, true));
        cp_json_set_str(alloc, res, "encoding", "pcm_f32le");
        hu_json_object_set(alloc, res, "input_sample_rate", hu_json_number_new(alloc, 16000));
        hu_json_object_set(alloc, res, "output_sample_rate", hu_json_number_new(alloc, 24000));
        cp_json_set_str(alloc, res, "mode", "gemini_live");
        char sid[40];
        (void)snprintf(sid, sizeof(sid), "%llu", (unsigned long long)conn->id);
        cp_json_set_str(alloc, res, "session_id", sid);
        hu_error_t err = cp_respond_json(alloc, res, out, out_len);
        if (err != HU_OK)
            vs_free_slot(sl, alloc);
        return err;
    }

    /* OpenAI Realtime mode: full-duplex voice via OpenAI Realtime API */
    bool use_openai_rt = (mode && strcmp(mode, "openai_realtime") == 0) ||
                         (cfg->voice.mode && strcmp(cfg->voice.mode, "openai_realtime") == 0);
    if (use_openai_rt) {
        if (sl->provider.vtable) {
            sl->provider.vtable->disconnect(sl->provider.ctx, alloc);
            sl->provider.vtable = NULL;
            sl->provider.ctx = NULL;
        }

        hu_error_t rerr = hu_voice_provider_create_from_config(alloc, cfg, "openai_realtime", NULL,
                                                               &sl->provider);
        if (rerr != HU_OK) {
            vs_free_slot(sl, alloc);
            return rerr;
        }
        rerr = sl->provider.vtable->connect(sl->provider.ctx);
        if (rerr != HU_OK) {
            sl->provider.vtable->disconnect(sl->provider.ctx, alloc);
            sl->provider.vtable = NULL;
            sl->provider.ctx = NULL;
            vs_free_slot(sl, alloc);
            return rerr;
        }

        sl->provider_mode = true; /* reuse same provider-based path for polling */

        hu_json_value_t *res = hu_json_object_new(alloc);
        if (!res) {
            vs_free_slot(sl, alloc);
            return HU_ERR_OUT_OF_MEMORY;
        }
        hu_json_object_set(alloc, res, "ok", hu_json_bool_new(alloc, true));
        cp_json_set_str(alloc, res, "encoding", "pcm_f32le");
        hu_json_object_set(alloc, res, "input_sample_rate", hu_json_number_new(alloc, 24000));
        hu_json_object_set(alloc, res, "output_sample_rate", hu_json_number_new(alloc, 24000));
        cp_json_set_str(alloc, res, "mode", "openai_realtime");
        char sid[40];
        (void)snprintf(sid, sizeof(sid), "%llu", (unsigned long long)conn->id);
        cp_json_set_str(alloc, res, "session_id", sid);
        hu_error_t err = cp_respond_json(alloc, res, out, out_len);
        if (err != HU_OK)
            vs_free_slot(sl, alloc);
        return err;
    }

    /* Standard Cartesia STT+TTS mode */
    const char *key = hu_config_get_provider_key(cfg, "cartesia");
    if (!key || !key[0]) {
        vs_free_slot(sl, alloc);
        return HU_ERR_INVALID_ARGUMENT;
    }

    if (sl->tts) {
        hu_cartesia_stream_close(sl->tts, alloc);
        sl->tts = NULL;
    }

    hu_error_t oerr = hu_cartesia_stream_open(alloc, key, voice_id ? voice_id : "",
                                              model_id ? model_id : "", &sl->tts);
    if (oerr != HU_OK) {
        vs_free_slot(sl, alloc);
        return oerr;
    }

    if (voice_id && voice_id[0])
        (void)snprintf(sl->voice_id, sizeof(sl->voice_id), "%s", voice_id);
    if (model_id && model_id[0])
        (void)snprintf(sl->model_id, sizeof(sl->model_id), "%s", model_id);

    hu_json_value_t *res = hu_json_object_new(alloc);
    if (!res) {
        vs_free_slot(sl, alloc);
        return HU_ERR_OUT_OF_MEMORY;
    }
    hu_json_object_set(alloc, res, "ok", hu_json_bool_new(alloc, true));
    cp_json_set_str(alloc, res, "encoding", "pcm_f32le");
    hu_json_object_set(alloc, res, "sample_rate", hu_json_number_new(alloc, 24000));
    char sid[40];
    (void)snprintf(sid, sizeof(sid), "%llu", (unsigned long long)conn->id);
    cp_json_set_str(alloc, res, "session_id", sid);
    hu_error_t err = cp_respond_json(alloc, res, out, out_len);
    if (err != HU_OK)
        vs_free_slot(sl, alloc);
    return err;
}

hu_error_t cp_voice_session_stop(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len) {
    (void)proto;
    (void)root;
    if (!out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;
    if (!alloc || !app || !conn)
        return HU_ERR_INVALID_ARGUMENT;
    vs_slot_t *sl = vs_find_slot_by_conn(conn);
    bool had_slot = (sl != NULL);
    if (sl)
        vs_free_slot(sl, alloc);
    hu_json_value_t *res = hu_json_object_new(alloc);
    if (!res)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, res, "ok", hu_json_bool_new(alloc, true));
    hu_json_object_set(alloc, res, "stopped", hu_json_bool_new(alloc, had_slot));
    return cp_respond_json(alloc, res, out, out_len);
}

hu_error_t cp_voice_session_interrupt(hu_allocator_t *alloc, hu_app_context_t *app,
                                      hu_ws_conn_t *conn, const hu_control_protocol_t *proto,
                                      const hu_json_value_t *root, char **out, size_t *out_len) {
    (void)proto;
    (void)root;
    if (!out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;
    if (!alloc || !app || !conn)
        return HU_ERR_INVALID_ARGUMENT;
    vs_slot_t *sl = vs_find_slot_by_conn(conn);
    if (sl) {
        hu_turn_action_t action;
        hu_duplex_user_chunk(&sl->duplex, vs_now_ms(), HU_TURN_SIGNAL_INTERRUPT, &action);
    }
    if (sl && sl->tts && sl->tts_context[0])
        (void)hu_cartesia_stream_cancel_context(sl->tts, alloc, sl->tts_context);
    /* Provider mode: signal the backend to stop its current response */
    if (sl && sl->provider_mode && sl->provider.vtable) {
        if (sl->vad_active)
            (void)sl->provider.vtable->send_activity_end(sl->provider.ctx);
        sl->vad_active = false;
        (void)sl->provider.vtable->send_audio_stream_end(sl->provider.ctx);
    }
    if (sl) {
        sl->pcm_len = 0;
        sl->tts_armed = false;
        if (s_active_tts_slot == sl)
            s_active_tts_slot = NULL;
    }
    if (sl && conn->active)
        hu_control_send_event_to_conn((hu_control_protocol_t *)proto, conn,
                                      "voice.audio.interrupted", "{}");

    return cp_respond_ok(alloc, out, out_len);
}

hu_error_t cp_voice_audio_end(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                              const hu_control_protocol_t *proto, const hu_json_value_t *root,
                              char **out, size_t *out_len) {
    if (!out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;
    if (!alloc || !app || !conn || !proto)
        return HU_ERR_INVALID_ARGUMENT;

    vs_slot_t *sl = vs_find_slot_by_conn(conn);
    if (!sl)
        return HU_ERR_INVALID_ARGUMENT;

    if (sl->provider_mode && sl->provider.vtable) {
        if (sl->vad_active)
            (void)sl->provider.vtable->send_activity_end(sl->provider.ctx);
        sl->vad_active = false;
        (void)sl->provider.vtable->send_audio_stream_end(sl->provider.ctx);
        return cp_respond_ok(alloc, out, out_len);
    }

    if (!app->config || !app->bus || !sl->tts)
        return HU_ERR_INVALID_ARGUMENT;
    if (sl->pcm_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    const char *mime_type = "audio/webm";
    hu_json_value_t *params = hu_json_object_get((hu_json_value_t *)root, "params");
    if (params) {
        const char *m = hu_json_get_string(params, "mime_type");
        if (m && m[0])
            mime_type = m;
    }

    hu_voice_config_t voice_cfg = {0};
    (void)hu_voice_config_from_settings((const hu_config_t *)app->config, &voice_cfg);
    voice_cfg.language = NULL;

    char *tmp_dir = hu_platform_get_temp_dir(alloc);
    if (!tmp_dir)
        return HU_ERR_IO;
    char tmpl[512];
    (void)snprintf(tmpl, sizeof(tmpl), "%s/human_vs_XXXXXX%s", tmp_dir, vs_mime_ext(mime_type));
    alloc->free(alloc->ctx, tmp_dir, strlen(tmp_dir) + 1);

    int fd = mkstemps(tmpl, (int)strlen(vs_mime_ext(mime_type)));
    if (fd < 0) {
        return HU_ERR_IO;
    }
    {
        const uint8_t *p = sl->pcm_buf;
        size_t left = sl->pcm_len;
        while (left > 0) {
            ssize_t w = write(fd, p, left > 65536 ? 65536 : left);
            if (w <= 0) {
                close(fd);
                unlink(tmpl);
                return HU_ERR_IO;
            }
            p += (size_t)w;
            left -= (size_t)w;
        }
    }
    close(fd);

    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err = hu_voice_stt_file(alloc, &voice_cfg, tmpl, &text, &text_len);
    unlink(tmpl);

    /* Audio features: only extract from raw PCM (audio/pcm, audio/l16, audio/wav).
     * Encoded formats (webm, ogg, mp3) are NOT decodable int16 PCM — feeding them
     * to hu_audio_features_extract produces garbage energy/pitch values. */
    hu_audio_features_t feats;
    memset(&feats, 0, sizeof(feats));
    bool is_raw_pcm = (strstr(mime_type, "pcm") != NULL || strstr(mime_type, "l16") != NULL ||
                       strstr(mime_type, "wav") != NULL);
    if (is_raw_pcm && sl->pcm_len >= 320 && (sl->pcm_len % 2u) == 0u && sl->pcm_buf) {
        (void)hu_audio_features_extract((const int16_t *)sl->pcm_buf, sl->pcm_len / 2u, 16000u,
                                        &feats);
    }
    sl->pcm_len = 0;

    if (err != HU_OK || !text)
        return err == HU_OK ? HU_ERR_IO : err;

    /* Fuse text + audio emotion for TTS voice selection */
    {
        hu_voice_emotion_t text_emo = HU_VOICE_EMOTION_NEUTRAL;
        float text_conf = 0.0f;
        (void)hu_emotion_detect_from_text(text, text_len, &text_emo, &text_conf);
        hu_voice_emotion_t audio_emo = HU_VOICE_EMOTION_NEUTRAL;
        float audio_conf = 0.0f;
        if (feats.valid)
            (void)hu_audio_emotion_classify(&feats, &audio_emo, &audio_conf);
        hu_voice_emotion_t fused_emo = HU_VOICE_EMOTION_NEUTRAL;
        float fused_conf = 0.0f;
        (void)hu_emotion_fuse(text_emo, text_conf, audio_emo, audio_conf, &fused_emo, &fused_conf);
        if (fused_conf > 0.25f) {
            sl->current_emotion = fused_emo;
            sl->emotion_voice_params = hu_emotion_voice_map(fused_emo);
        }
    }

    /* Semantic EOT: transcript + acoustic features */
    hu_turn_signal_t eot_signal = HU_TURN_SIGNAL_YIELD;
    {
        hu_semantic_eot_config_t eot_cfg;
        hu_semantic_eot_config_default(&eot_cfg);
        hu_semantic_eot_result_t eot_result;
        memset(&eot_result, 0, sizeof(eot_result));
        float pitch_delta = feats.valid ? (feats.pitch_mean_hz - 120.0f) : 0.0f;
        hu_semantic_eot_classifier_t eot_cls;
        hu_semantic_eot_classifier_default(&eot_cls);
        float eot_energy = feats.valid ? feats.energy_db : -40.0f;
        hu_error_t eot_err = hu_semantic_eot_classify(&eot_cls, &eot_cfg, text, text_len, 0,
                                                      eot_energy, pitch_delta, &eot_result);
        if (eot_err == HU_OK && eot_result.is_endpoint)
            eot_signal = eot_result.suggested_signal;
    }

    hu_turn_action_t turn_action;
    hu_duplex_user_chunk(&sl->duplex, vs_now_ms(), eot_signal, &turn_action);
    hu_duplex_start_streaming(&sl->duplex, vs_now_ms());

    /* Targeted transcript event */
    hu_json_value_t *tev = hu_json_object_new(alloc);
    if (!tev) {
        alloc->free(alloc->ctx, text, text_len + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    cp_json_set_str(alloc, tev, "text", text);
    char *tpayload = NULL;
    size_t tplen = 0;
    (void)hu_json_stringify(alloc, tev, &tpayload, &tplen);
    hu_json_free(alloc, tev);
    if (tpayload)
        hu_control_send_event_to_conn((hu_control_protocol_t *)proto, conn, "voice.transcript",
                                      tpayload);
    alloc->free(alloc->ctx, tpayload, tplen + 1);

    (void)snprintf(sl->tts_context, sizeof(sl->tts_context), "ctx-%llu-%u",
                   (unsigned long long)conn->id, ++sl->turn_counter);
    sl->tts_armed = true;
    s_active_tts_slot = sl;

    hu_bus_event_t bev;
    memset(&bev, 0, sizeof(bev));
    bev.type = HU_BUS_MESSAGE_RECEIVED;
    (void)snprintf(bev.channel, sizeof(bev.channel), "control-ui");
    (void)snprintf(bev.id, sizeof(bev.id), "%s", sl->session_key);
    bev.payload = text;
    size_t tl = strlen(text);
    if (tl >= HU_BUS_MSG_LEN)
        tl = HU_BUS_MSG_LEN - 1;
    memcpy(bev.message, text, tl);
    bev.message[tl] = '\0';
    hu_bus_publish(app->bus, &bev);
    alloc->free(alloc->ctx, text, text_len + 1);

    return cp_respond_ok(alloc, out, out_len);
}

/* ── Gemini Live polling: drain audio events to connected browsers ──── */

void hu_voice_stream_poll_gemini_live(void) {
    if (!s_proto || !s_proto->alloc || !s_proto->ws)
        return;
    hu_allocator_t *a = s_proto->alloc;

    for (int i = 0; i < VS_MAX_SLOTS; i++) {
        vs_slot_t *sl = &s_vs[i];
        if (!sl->in_use || !sl->provider_mode || !sl->provider.vtable || !sl->conn ||
            !sl->conn->active)
            continue;

        /* Deferred goAway reconnect */
        if (sl->goaway_reconnect_at_ms > 0) {
            if ((uint64_t)vs_now_ms() >= sl->goaway_reconnect_at_ms) {
                sl->goaway_reconnect_at_ms = 0;
                hu_error_t rerr = sl->provider.vtable->reconnect(sl->provider.ctx);
                if (rerr == HU_OK)
                    hu_control_send_event_to_conn(s_proto, sl->conn, "voice.reconnected", "{}");
                else {
                    hu_control_send_event_to_conn(s_proto, sl->conn, "voice.error",
                                                  "{\"message\":\"reconnect failed\"}");
                    vs_free_slot(sl, s_proto->alloc);
                }
            }
            continue;
        }

        for (int drain = 0; drain < 16; drain++) {
            hu_voice_rt_event_t ev = {0};
            hu_error_t err = sl->provider.vtable->recv_event(sl->provider.ctx, a, &ev, 0);
            if (err != HU_OK) {
                if (err != HU_ERR_TIMEOUT)
                    hu_control_send_event_to_conn(s_proto, sl->conn, "voice.error",
                                                  "{\"message\":\"recv_event failed\"}");
                break;
            }

            /* Audio: decode base64 PCM16 → convert to f32le → send binary */
            if (ev.audio_base64 && ev.audio_base64_len > 0) {
                void *pcm16 = NULL;
                size_t pcm16_len = 0;
                hu_error_t dec_err = hu_multimodal_decode_base64(
                    a, ev.audio_base64, ev.audio_base64_len, &pcm16, &pcm16_len);
                if (dec_err != HU_OK || !pcm16 || pcm16_len < 2) {
                    hu_control_send_event_to_conn(s_proto, sl->conn, "voice.error",
                                                  "{\"message\":\"audio decode failed\"}");
                    if (pcm16)
                        a->free(a->ctx, pcm16, pcm16_len);
                } else if (pcm16 && pcm16_len >= 2) {
                    size_t n_samples = pcm16_len / 2;
                    size_t f32_len = n_samples * sizeof(float);
                    float *f32 = (float *)a->alloc(a->ctx, f32_len);
                    if (f32) {
                        const int16_t *src = (const int16_t *)pcm16;
                        for (size_t s = 0; s < n_samples; s++)
                            f32[s] = (float)src[s] / 32768.0f;
                        (void)hu_ws_server_send_binary(s_proto->ws, sl->conn, (const char *)f32,
                                                       f32_len);
                        a->free(a->ctx, f32, f32_len);
                    }
                    a->free(a->ctx, pcm16, pcm16_len);
                }
            }

            /* Transcript: send as JSON event */
            if (ev.transcript && ev.transcript_len > 0) {
                bool is_input = (strstr(ev.type, "inputTranscription") != NULL);
                const char *event_name =
                    is_input ? "voice.transcript" : "voice.assistant.transcript";
                hu_json_value_t *tev = hu_json_object_new(a);
                if (tev) {
                    cp_json_set_str(a, tev, "text", ev.transcript);
                    char *payload = NULL;
                    size_t plen = 0;
                    if (hu_json_stringify(a, tev, &payload, &plen) == HU_OK && payload) {
                        hu_control_send_event_to_conn(s_proto, sl->conn, event_name, payload);
                        a->free(a->ctx, payload, plen + 1);
                    }
                    hu_json_free(a, tev);
                }
            }

            /* Setup complete: session is ready */
            if (strcmp(ev.type, "setupComplete") == 0)
                hu_control_send_event_to_conn(s_proto, sl->conn, "voice.setup_complete", "{}");

            /* Session resumption update: token refreshed */
            if (strcmp(ev.type, "sessionResumptionUpdate") == 0)
                hu_control_send_event_to_conn(s_proto, sl->conn, "voice.session_resumption", "{}");

            /* Tool call: execute server-side and send result back to provider */
            if ((strcmp(ev.type, "toolCall") == 0 ||
                 strcmp(ev.type, "response.function_call") == 0) &&
                ev.transcript && ev.transcript_len > 0) {
                /* Notify UI of the tool call for display */
                hu_json_value_t *tev = hu_json_object_new(a);
                if (tev) {
                    cp_json_set_str(a, tev, "name", ev.transcript);
                    if (ev.tool_call_id)
                        cp_json_set_str(a, tev, "call_id", ev.tool_call_id);
                    if (ev.tool_args_json && ev.tool_args_json_len > 0)
                        cp_json_set_str(a, tev, "args", ev.tool_args_json);
                    char *payload = NULL;
                    size_t plen = 0;
                    if (hu_json_stringify(a, tev, &payload, &plen) == HU_OK && payload) {
                        hu_control_send_event_to_conn(s_proto, sl->conn, "voice.tool_call",
                                                      payload);
                        a->free(a->ctx, payload, plen + 1);
                    }
                    hu_json_free(a, tev);
                }
                /* Execute tool from agent registry and send result to Gemini.
                 * Generate a synthetic ID if the server omitted one so the
                 * response round-trip can still complete. */
                if (!ev.tool_call_id) {
                    char synth[32];
                    int sn = snprintf(synth, sizeof(synth), "synth-%d", sl->synth_seq++);
                    if (sn > 0) {
                        ev.tool_call_id = hu_strndup(a, synth, (size_t)sn);
                        if (ev.tool_call_id)
                            ev.tool_call_id_len = (size_t)sn;
                    }
                    if (!ev.tool_call_id) {
                        hu_voice_rt_event_free(a, &ev);
                        continue;
                    }
                }
                hu_app_context_t *app = s_proto->app_ctx;
                if (app && app->tools && ev.tool_call_id) {
                    hu_tool_t *match = NULL;
                    for (size_t ti = 0; ti < app->tools_count; ti++) {
                        hu_tool_t *t = &app->tools[ti];
                        if (t->vtable && t->vtable->name &&
                            strcmp(t->vtable->name(t->ctx), ev.transcript) == 0) {
                            match = t;
                            break;
                        }
                    }
                    const char *result_json = "{\"error\":\"tool not found\"}";
                    char *owned_result = NULL;
                    if (match && match->vtable->execute) {
                        hu_json_value_t *args = NULL;
                        if (ev.tool_args_json && ev.tool_args_json_len > 0)
                            (void)hu_json_parse(a, ev.tool_args_json, ev.tool_args_json_len, &args);
                        if (!args)
                            args = hu_json_object_new(a);
                        hu_tool_result_t tr = {0};
                        hu_error_t texec = match->vtable->execute(match->ctx, a, args, &tr);
                        if (texec == HU_OK && tr.success && tr.output && tr.output_len > 0) {
                            owned_result = hu_strndup(a, tr.output, tr.output_len);
                            if (owned_result)
                                result_json = owned_result;
                        } else if (texec == HU_OK && !tr.success && tr.error_msg &&
                                   tr.error_msg_len > 0) {
                            result_json = "{\"error\":\"tool execution failed\"}";
                        } else {
                            result_json = "{\"error\":\"tool returned no output\"}";
                        }
                        if (tr.output_owned && tr.output)
                            a->free(a->ctx, (void *)tr.output, tr.output_len + 1);
                        if (tr.error_msg_owned && tr.error_msg)
                            a->free(a->ctx, (void *)tr.error_msg, tr.error_msg_len + 1);
                        if (args)
                            hu_json_free(a, args);
                    }
                    if (sl->provider.vtable && sl->provider.vtable->send_tool_response)
                        (void)sl->provider.vtable->send_tool_response(
                            sl->provider.ctx, ev.transcript, ev.tool_call_id, result_json);
                    if (owned_result)
                        a->free(a->ctx, owned_result, strlen(owned_result) + 1);
                }
            }

            /* Tool call cancellation: forward as JSON array of IDs */
            if (strcmp(ev.type, "toolCallCancellation") == 0 && ev.transcript &&
                ev.transcript_len > 0) {
                hu_json_value_t *tcev = hu_json_object_new(a);
                if (tcev) {
                    hu_json_value_t *arr = hu_json_array_new(a);
                    if (arr) {
                        const char *p = ev.transcript;
                        while (*p) {
                            const char *comma = strchr(p, ',');
                            size_t seg = comma ? (size_t)(comma - p) : strlen(p);
                            hu_json_value_t *s = hu_json_string_new(a, p, seg);
                            if (s)
                                hu_json_array_push(a, arr, s);
                            p += seg + (comma ? 1 : 0);
                            if (!comma)
                                break;
                        }
                        hu_json_object_set(a, tcev, "ids", arr);
                    }
                    char *payload = NULL;
                    size_t plen = 0;
                    if (hu_json_stringify(a, tcev, &payload, &plen) == HU_OK && payload) {
                        hu_control_send_event_to_conn(s_proto, sl->conn, "voice.tool_cancelled",
                                                      payload);
                        a->free(a->ctx, payload, plen + 1);
                    }
                    hu_json_free(a, tcev);
                }
            }

            /* Generation complete: model done generating, playback may continue */
            if (ev.generation_complete) {
                hu_control_send_event_to_conn(s_proto, sl->conn, "voice.generation_complete", "{}");
            }

            /* Interrupted — user barged in; tell UI to clear playback queue */
            if (ev.interrupted) {
                hu_control_send_event_to_conn(s_proto, sl->conn, "voice.audio.interrupted", "{}");
            }

            /* Turn complete: end VAD activity so next mic chunk re-opens it */
            if (ev.done) {
                if (sl->vad_active) {
                    (void)sl->provider.vtable->send_activity_end(sl->provider.ctx);
                    sl->vad_active = false;
                }
                hu_control_send_event_to_conn(s_proto, sl->conn, "voice.audio.done", "{}");
            }

            /* goAway: defer reconnect by ~10% of timeLeft (capped 2s)
             * so we don't block the poll loop for other slots. */
            if (ev.go_away_ms > 0 && sl->provider.vtable->reconnect) {
                sl->vad_active = false;
                int delay_ms = ev.go_away_ms / 10;
                if (delay_ms > 2000)
                    delay_ms = 2000;
                if (delay_ms < 100)
                    delay_ms = 100;
                sl->goaway_reconnect_at_ms = (uint64_t)vs_now_ms() + (uint64_t)delay_ms;
                hu_control_send_event_to_conn(s_proto, sl->conn, "voice.goaway",
                                              "{\"message\":\"server disconnecting soon\"}");
                hu_voice_rt_event_free(a, &ev);
                break;
            }

            bool is_done = ev.done;
            hu_voice_rt_event_free(a, &ev);

            if (is_done)
                break;
        }
    }
}

hu_error_t cp_voice_tool_response(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                  const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                  char **out, size_t *out_len) {
    (void)proto;
    if (!alloc || !app || !conn || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    vs_slot_t *sl = vs_find_slot_by_conn(conn);
    if (!sl || !sl->provider_mode || !sl->provider.vtable ||
        !sl->provider.vtable->send_tool_response)
        return HU_ERR_INVALID_ARGUMENT;

    hu_json_value_t *params = hu_json_object_get((hu_json_value_t *)root, "params");
    if (!params)
        return HU_ERR_INVALID_ARGUMENT;

    const char *name = hu_json_get_string(params, "name");
    const char *call_id = hu_json_get_string(params, "call_id");
    const char *result = hu_json_get_string(params, "result");
    if (!name || !call_id)
        return HU_ERR_INVALID_ARGUMENT;

    hu_error_t err = sl->provider.vtable->send_tool_response(sl->provider.ctx, name, call_id,
                                                             result && result[0] ? result : "{}");

    hu_json_value_t *res = hu_json_object_new(alloc);
    if (!res)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, res, "ok", hu_json_bool_new(alloc, err == HU_OK));
    return cp_respond_json(alloc, res, out, out_len);
}

#else /* !HU_GATEWAY_POSIX */

void hu_voice_stream_attach_bus(hu_bus_t *bus, hu_control_protocol_t *proto) {
    (void)bus;
    (void)proto;
}

void hu_voice_stream_detach_bus(hu_bus_t *bus) {
    (void)bus;
}

void hu_voice_stream_on_binary(hu_control_protocol_t *proto, hu_ws_conn_t *conn, const char *data,
                               size_t data_len) {
    (void)proto;
    (void)conn;
    (void)data;
    (void)data_len;
}

void hu_voice_stream_on_conn_close(hu_ws_conn_t *conn) {
    (void)conn;
}

void hu_voice_stream_poll_gemini_live(void) {}

hu_error_t cp_voice_session_start(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                  const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                  char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    if (out)
        *out = NULL;
    if (out_len)
        *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_voice_session_stop(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    if (out)
        *out = NULL;
    if (out_len)
        *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_voice_session_interrupt(hu_allocator_t *alloc, hu_app_context_t *app,
                                      hu_ws_conn_t *conn, const hu_control_protocol_t *proto,
                                      const hu_json_value_t *root, char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    if (out)
        *out = NULL;
    if (out_len)
        *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_voice_audio_end(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                              const hu_control_protocol_t *proto, const hu_json_value_t *root,
                              char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    if (out)
        *out = NULL;
    if (out_len)
        *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_voice_tool_response(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                  const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                  char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    if (out) *out = NULL;
    if (out_len) *out_len = 0;
    return HU_ERR_NOT_SUPPORTED;
}

#endif /* HU_GATEWAY_POSIX */
