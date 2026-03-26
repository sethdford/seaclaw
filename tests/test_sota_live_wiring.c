/*
 * Tests for SOTA live wiring: spreading activation in recall, prompt cache,
 * TTL tool cache, emotion voice map, ACP bridge, KV cache, PersonaFuse,
 * audio emotion detection.
 */
#include "cp_internal.h"
#include "human/agent/acp_bridge.h"
#include "human/agent/agent_comm.h"
#include "human/agent/kv_cache.h"
#include "human/agent/mailbox.h"
#include "human/agent/prompt_cache.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/gateway/control_protocol.h"
#include "human/memory/graph_index.h"
#include "human/persona/persona_fuse.h"
#include "human/security.h"
#include "human/security/causal_armor.h"
#include "human/security/history_scorer.h"
#include "human/tools/cache_ttl.h"
#include "human/voice/audio_emotion.h"
#include "human/voice/emotion_voice_map.h"
#include "human/voice/semantic_eot.h"
#include "test_framework.h"
#include <string.h>
#include <time.h>

/* ── Prompt Cache ────────────────────────────────────────────────── */

static void test_prompt_cache_init_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prompt_cache_t cache;
    HU_ASSERT_EQ(hu_prompt_cache_init(&cache, &alloc), HU_OK);
    HU_ASSERT_EQ(cache.count, 0);
    hu_prompt_cache_deinit(&cache);
}

static void test_prompt_cache_hash_deterministic(void) {
    const char *text = "You are a helpful assistant.";
    uint64_t h1 = hu_prompt_cache_hash(text, strlen(text));
    uint64_t h2 = hu_prompt_cache_hash(text, strlen(text));
    HU_ASSERT_EQ(h1, h2);
    uint64_t h3 = hu_prompt_cache_hash("different", 9);
    HU_ASSERT(h1 != h3);
}

static void test_prompt_cache_store_and_lookup(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prompt_cache_t cache;
    hu_prompt_cache_init(&cache, &alloc);

    uint64_t hash = hu_prompt_cache_hash("test prompt", 11);
    HU_ASSERT_EQ(hu_prompt_cache_store(&cache, hash, "cache-id-1", 10, 3600), HU_OK);

    size_t id_len = 0;
    const char *id = hu_prompt_cache_lookup(&cache, hash, &id_len);
    HU_ASSERT_NOT_NULL(id);
    HU_ASSERT_EQ(id_len, 10);
    HU_ASSERT(memcmp(id, "cache-id-1", 10) == 0);

    const char *miss = hu_prompt_cache_lookup(&cache, hash + 1, &id_len);
    HU_ASSERT_NULL(miss);

    hu_prompt_cache_deinit(&cache);
}

static void test_prompt_cache_clear(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_prompt_cache_t cache;
    hu_prompt_cache_init(&cache, &alloc);

    uint64_t hash = hu_prompt_cache_hash("test", 4);
    hu_prompt_cache_store(&cache, hash, "cid", 3, 3600);
    HU_ASSERT_EQ(cache.count, 1);

    hu_prompt_cache_clear(&cache);
    HU_ASSERT_EQ(cache.count, 0);

    size_t id_len = 0;
    HU_ASSERT_NULL(hu_prompt_cache_lookup(&cache, hash, &id_len));

    hu_prompt_cache_deinit(&cache);
}

/* ── TTL Tool Cache ──────────────────────────────────────────────── */

static void test_ttl_cache_init_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_cache_ttl_t cache;
    HU_ASSERT_EQ(hu_tool_cache_ttl_init(&cache, &alloc), HU_OK);
    HU_ASSERT_EQ(cache.count, 0);
    hu_tool_cache_ttl_deinit(&cache);
}

static void test_ttl_cache_put_get(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_cache_ttl_t cache;
    hu_tool_cache_ttl_init(&cache, &alloc);

    uint64_t key = hu_tool_cache_ttl_key("read_file", 9, "{\"path\":\"/tmp/a\"}", 17);
    HU_ASSERT_EQ(hu_tool_cache_ttl_put(&cache, key, "file content", 12, 300), HU_OK);

    size_t len = 0;
    const char *result = hu_tool_cache_ttl_get(&cache, key, &len);
    HU_ASSERT_NOT_NULL(result);
    HU_ASSERT_EQ(len, 12);
    HU_ASSERT(memcmp(result, "file content", 12) == 0);

    HU_ASSERT_NULL(hu_tool_cache_ttl_get(&cache, key + 1, &len));

    hu_tool_cache_ttl_deinit(&cache);
}

static void test_ttl_cache_default_ttl_varies_by_tool(void) {
    int64_t read_ttl = hu_tool_cache_ttl_default_for("read_file", 9);
    int64_t shell_ttl = hu_tool_cache_ttl_default_for("shell", 5);
    HU_ASSERT_EQ(shell_ttl, 0);
    HU_ASSERT(read_ttl >= 0);
}

static void test_ttl_cache_evict_expired(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_cache_ttl_t cache;
    hu_tool_cache_ttl_init(&cache, &alloc);

    hu_tool_cache_ttl_put(&cache, 100, "short-lived", 11, 1);
    hu_tool_cache_ttl_put(&cache, 200, "long-lived", 10, 3600);
    HU_ASSERT_EQ(cache.count, 2);

    int64_t future = (int64_t)time(NULL) + 10;
    size_t evicted = hu_tool_cache_ttl_evict_expired(&cache, future);
    HU_ASSERT_EQ(evicted, 1);
    HU_ASSERT_EQ(cache.count, 1);

    hu_tool_cache_ttl_deinit(&cache);
}

/* ── Emotion Voice Map ───────────────────────────────────────────── */

static void test_emotion_voice_map_default(void) {
    hu_voice_params_t p = hu_voice_params_default();
    HU_ASSERT_FLOAT_EQ(p.pitch_shift, 0.0f, 0.01);
    HU_ASSERT_FLOAT_EQ(p.rate_factor, 1.0f, 0.01);
}

static void test_emotion_voice_map_joy(void) {
    hu_voice_params_t p = hu_emotion_voice_map(HU_VOICE_EMOTION_JOY);
    HU_ASSERT(p.pitch_shift > 0.0f);
    HU_ASSERT(p.warmth > 0.5f);
}

static void test_emotion_detect_from_text(void) {
    hu_voice_emotion_t emo = HU_VOICE_EMOTION_NEUTRAL;
    float conf = 0.0f;
    HU_ASSERT_EQ(hu_emotion_detect_from_text("I'm so happy and excited!", 25, &emo, &conf), HU_OK);
    HU_ASSERT(emo == HU_VOICE_EMOTION_JOY || emo == HU_VOICE_EMOTION_EXCITEMENT);
    HU_ASSERT(conf > 0.0f);
}

static void test_emotion_voice_blend(void) {
    hu_voice_params_t joy = hu_emotion_voice_map(HU_VOICE_EMOTION_JOY);
    hu_voice_params_t sad = hu_emotion_voice_map(HU_VOICE_EMOTION_SADNESS);
    hu_voice_params_t blended = hu_voice_params_blend(&joy, &sad, 0.5f);
    HU_ASSERT(blended.pitch_shift < joy.pitch_shift || blended.pitch_shift > sad.pitch_shift ||
              (blended.pitch_shift >= sad.pitch_shift && blended.pitch_shift <= joy.pitch_shift));
}

/* ── ACP Bridge ──────────────────────────────────────────────────── */

static void test_acp_bridge_map_priority(void) {
    HU_ASSERT_EQ(hu_acp_bridge_map_priority(HU_ACP_PRIORITY_LOW), HU_MSG_PRIO_LOW);
    HU_ASSERT_EQ(hu_acp_bridge_map_priority(HU_ACP_PRIORITY_NORMAL), HU_MSG_PRIO_NORMAL);
    HU_ASSERT_EQ(hu_acp_bridge_map_priority(HU_ACP_PRIORITY_HIGH), HU_MSG_PRIO_HIGH);
    HU_ASSERT_EQ(hu_acp_bridge_map_priority(HU_ACP_PRIORITY_URGENT), HU_MSG_PRIO_HIGH);
}

static void test_acp_bridge_map_type(void) {
    HU_ASSERT_EQ(hu_acp_bridge_map_type(HU_ACP_REQUEST), HU_MSG_TASK);
    HU_ASSERT_EQ(hu_acp_bridge_map_type(HU_ACP_RESPONSE), HU_MSG_RESULT);
    HU_ASSERT_EQ(hu_acp_bridge_map_type(HU_ACP_BROADCAST), HU_MSG_BROADCAST);
    HU_ASSERT_EQ(hu_acp_bridge_map_type(HU_ACP_CANCEL), HU_MSG_CANCEL);
}

static void test_acp_bridge_send_recv_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_mailbox_t *mbox = hu_mailbox_create(&alloc, 4);
    HU_ASSERT_NOT_NULL(mbox);

    HU_ASSERT_EQ(hu_mailbox_register(mbox, 1), HU_OK);
    HU_ASSERT_EQ(hu_mailbox_register(mbox, 2), HU_OK);

    hu_acp_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = HU_ACP_REQUEST;
    msg.priority = HU_ACP_PRIORITY_NORMAL;
    msg.payload = "do task X";
    msg.payload_len = 9;

    HU_ASSERT_EQ(hu_acp_bridge_send(&alloc, mbox, &msg, 1, 2), HU_OK);

    hu_message_t raw;
    HU_ASSERT_EQ(hu_mailbox_recv(mbox, 2, &raw), HU_OK);
    HU_ASSERT_NOT_NULL(raw.payload);

    hu_acp_message_t decoded;
    HU_ASSERT_EQ(hu_acp_bridge_recv(&alloc, &raw, &decoded), HU_OK);
    HU_ASSERT_EQ(decoded.type, HU_ACP_REQUEST);
    HU_ASSERT_NOT_NULL(decoded.payload);
    HU_ASSERT(strstr(decoded.payload, "do task X") != NULL);

    hu_acp_message_free(&alloc, &decoded);
    hu_message_free(&alloc, &raw);
    hu_mailbox_destroy(mbox);
}

/* ── KV Cache ────────────────────────────────────────────────────── */

static void test_kv_cache_init_deinit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_kv_cache_manager_t mgr;
    HU_ASSERT_EQ(hu_kv_cache_init(&mgr, &alloc, 4096), HU_OK);
    HU_ASSERT_FLOAT_EQ(hu_kv_cache_utilization(&mgr), 0.0f, 0.01);
    hu_kv_cache_deinit(&mgr);
}

static void test_kv_cache_add_and_utilization(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_kv_cache_manager_t mgr;
    hu_kv_cache_init(&mgr, &alloc, 1000);

    hu_kv_cache_add_segment(&mgr, "system", 6, 200, true);
    hu_kv_cache_add_segment(&mgr, "turn:1", 6, 300, false);
    hu_kv_cache_add_segment(&mgr, "turn:2", 6, 400, false);

    HU_ASSERT_EQ(mgr.segment_count, 3);
    HU_ASSERT_FLOAT_EQ(hu_kv_cache_utilization(&mgr), 0.9f, 0.01);

    hu_kv_cache_deinit(&mgr);
}

static void test_kv_cache_prune_evicts_lowest_attention(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_kv_cache_manager_t mgr;
    hu_kv_cache_init(&mgr, &alloc, 1000);
    mgr.eviction_threshold = 0.8f;

    hu_kv_cache_add_segment(&mgr, "system", 6, 200, true);
    hu_kv_cache_add_segment(&mgr, "old", 3, 400, false);
    hu_kv_cache_add_segment(&mgr, "recent", 6, 300, false);

    hu_kv_cache_boost_attention(&mgr, "recent", 6, 0.3f);

    const char *evicted[4];
    size_t n = hu_kv_cache_prune(&mgr, evicted, 4);
    HU_ASSERT(n >= 1);
    HU_ASSERT_STR_EQ(evicted[0], "old");

    for (size_t i = 0; i < n; i++)
        alloc.free(alloc.ctx, (void *)evicted[i], strlen(evicted[i]) + 1);

    hu_kv_cache_deinit(&mgr);
}

static void test_kv_cache_pinned_not_evicted(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_kv_cache_manager_t mgr;
    hu_kv_cache_init(&mgr, &alloc, 500);
    mgr.eviction_threshold = 0.5f;

    hu_kv_cache_add_segment(&mgr, "pinned", 6, 300, true);
    hu_kv_cache_add_segment(&mgr, "other", 5, 200, false);

    const char *evicted[4];
    size_t n = hu_kv_cache_prune(&mgr, evicted, 4);
    for (size_t i = 0; i < n; i++) {
        HU_ASSERT(strcmp(evicted[i], "pinned") != 0);
        alloc.free(alloc.ctx, (void *)evicted[i], strlen(evicted[i]) + 1);
    }

    hu_kv_cache_deinit(&mgr);
}

/* ── PersonaFuse ─────────────────────────────────────────────────── */

static void test_persona_fuse_init_and_add(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_fuse_t fuse;
    HU_ASSERT_EQ(hu_persona_fuse_init(&fuse, &alloc), HU_OK);

    HU_ASSERT_EQ(hu_persona_fuse_add_adapter(&fuse, "test", 4, 0.5f, -0.3f, 1.0f, 0.1f), HU_OK);
    HU_ASSERT_EQ(fuse.adapter_count, 1);

    const hu_fuse_adapter_t *got = hu_persona_fuse_get(&fuse, "test", 4);
    HU_ASSERT_NOT_NULL(got);
    HU_ASSERT_FLOAT_EQ(got->formality, 0.5f, 0.01);

    hu_persona_fuse_deinit(&fuse);
}

static void test_persona_fuse_compose_multiple(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_fuse_t fuse;
    hu_persona_fuse_init(&fuse, &alloc);
    hu_persona_fuse_add_builtin_adapters(&fuse);

    const char *adapters[] = {"professional", "brief"};
    hu_fuse_result_t result;
    HU_ASSERT_EQ(hu_persona_fuse_compose(&fuse, adapters, 2, &result), HU_OK);

    HU_ASSERT(result.formality > 0.2f);
    HU_ASSERT(result.verbosity < 0.0f);

    hu_persona_fuse_deinit(&fuse);
}

static void test_persona_fuse_builtin_presets(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_fuse_t fuse;
    hu_persona_fuse_init(&fuse, &alloc);
    HU_ASSERT_EQ(hu_persona_fuse_add_builtin_adapters(&fuse), HU_OK);
    HU_ASSERT_EQ(fuse.adapter_count, 5);

    HU_ASSERT_NOT_NULL(hu_persona_fuse_get(&fuse, "professional", 12));
    HU_ASSERT_NOT_NULL(hu_persona_fuse_get(&fuse, "casual", 6));
    HU_ASSERT_NOT_NULL(hu_persona_fuse_get(&fuse, "brief", 5));
    HU_ASSERT_NOT_NULL(hu_persona_fuse_get(&fuse, "verbose", 7));
    HU_ASSERT_NOT_NULL(hu_persona_fuse_get(&fuse, "empathetic", 10));

    hu_persona_fuse_deinit(&fuse);
}

/* ── Audio Emotion ───────────────────────────────────────────────── */

static void test_audio_features_extract_silence(void) {
    int16_t pcm[4800];
    memset(pcm, 0, sizeof(pcm));
    hu_audio_features_t features;
    HU_ASSERT_EQ(hu_audio_features_extract(pcm, 4800, 24000, &features), HU_OK);
    HU_ASSERT_TRUE(features.valid);
    HU_ASSERT_FLOAT_EQ(features.silence_ratio, 1.0f, 0.01);
}

static void test_audio_features_extract_f32_silence(void) {
    float pcm[4800];
    memset(pcm, 0, sizeof(pcm));
    hu_audio_features_t features;
    HU_ASSERT_EQ(hu_audio_features_extract_f32(pcm, 4800, 24000, &features), HU_OK);
    HU_ASSERT_TRUE(features.valid);
    HU_ASSERT_FLOAT_EQ(features.silence_ratio, 1.0f, 0.01);
}

static void test_audio_emotion_classify_neutral_on_silence(void) {
    hu_audio_features_t features;
    memset(&features, 0, sizeof(features));
    features.valid = true;
    features.energy_db = -60.0f;
    features.silence_ratio = 0.9f;

    hu_voice_emotion_t emo;
    float conf;
    HU_ASSERT_EQ(hu_audio_emotion_classify(&features, &emo, &conf), HU_OK);
    HU_ASSERT(emo != HU_VOICE_EMOTION_EXCITEMENT);
    HU_ASSERT(emo != HU_VOICE_EMOTION_URGENCY);
}

static void test_audio_emotion_fuse_agreeing(void) {
    hu_voice_emotion_t out;
    float conf;
    HU_ASSERT_EQ(
        hu_emotion_fuse(HU_VOICE_EMOTION_JOY, 0.7f, HU_VOICE_EMOTION_JOY, 0.5f, &out, &conf),
        HU_OK);
    HU_ASSERT_EQ(out, HU_VOICE_EMOTION_JOY);
    HU_ASSERT(conf > 0.7f);
}

static void test_audio_emotion_fuse_disagreeing(void) {
    hu_voice_emotion_t out;
    float conf;
    HU_ASSERT_EQ(
        hu_emotion_fuse(HU_VOICE_EMOTION_JOY, 0.8f, HU_VOICE_EMOTION_SADNESS, 0.3f, &out, &conf),
        HU_OK);
    HU_ASSERT_EQ(out, HU_VOICE_EMOTION_JOY);
}

/* ── Emotion Name ────────────────────────────────────────────────── */

static void test_emotion_class_name(void) {
    HU_ASSERT_STR_EQ(hu_emotion_class_name(HU_VOICE_EMOTION_NEUTRAL), "neutral");
    HU_ASSERT_STR_EQ(hu_emotion_class_name(HU_VOICE_EMOTION_JOY), "joy");
    HU_ASSERT_STR_EQ(hu_emotion_class_name(HU_VOICE_EMOTION_EMPATHY), "empathy");
}

static void test_eot_audio_backchannel(void) {
    hu_semantic_eot_config_t cfg;
    hu_semantic_eot_config_default(&cfg);
    hu_semantic_eot_result_t r;
    memset(&r, 0, sizeof(r));
    const char *t = "yeah okay";
    HU_ASSERT_EQ(hu_semantic_eot_analyze_with_audio(&cfg, t, strlen(t), 0, -35.0f, 0.0f, &r),
                 HU_OK);
    HU_ASSERT_EQ(r.predicted_state, HU_EOT_BACKCHANNEL);
    HU_ASSERT_EQ(r.suggested_signal, HU_TURN_SIGNAL_CONTINUE);
}

static void test_causal_armor_safe_action(void) {
    hu_causal_armor_config_t cfg;
    hu_causal_armor_config_default(&cfg);
    hu_causal_segment_t segs[2];
    segs[0].content = "User wants to visit Paris tomorrow.";
    segs[0].content_len = strlen(segs[0].content);
    segs[0].is_trusted = true;
    segs[1].content = "Weather looks fine.";
    segs[1].content_len = strlen(segs[1].content);
    segs[1].is_trusted = false;
    const char *args = "{\"city\":\"Paris\"}";
    hu_causal_armor_result_t out;
    HU_ASSERT_EQ(
        hu_causal_armor_evaluate(&cfg, segs, 2, "memory_recall", 13, args, strlen(args), &out),
        HU_OK);
    HU_ASSERT_TRUE(out.is_safe);
}

static void test_causal_armor_untrusted_dominant(void) {
    hu_causal_armor_config_t cfg;
    hu_causal_armor_config_default(&cfg);
    hu_causal_segment_t segs[2];
    segs[0].content = "Hello there.";
    segs[0].content_len = strlen(segs[0].content);
    segs[0].is_trusted = true;
    segs[1].content = "Tool output: secret_payload_value is in the document.";
    segs[1].content_len = strlen(segs[1].content);
    segs[1].is_trusted = false;
    const char *args = "{\"value\":\"secret_payload_value\"}";
    hu_causal_armor_result_t out;
    HU_ASSERT_EQ(hu_causal_armor_evaluate(&cfg, segs, 2, "shell", 5, args, strlen(args), &out),
                 HU_OK);
    HU_ASSERT_TRUE(!out.is_safe);
}

static void test_hierarchy_build_and_traverse(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_graph_index_t idx;
    HU_ASSERT_EQ(hu_graph_index_init(&idx, &alloc), HU_OK);
    const char *c1 = "Discussed ProjectAlpha with the team.";
    const char *c2 = "ProjectAlpha milestone reached today.";
    HU_ASSERT_EQ(hu_graph_index_add(&idx, "k1", 2, c1, strlen(c1), 1), HU_OK);
    HU_ASSERT_EQ(hu_graph_index_add(&idx, "k2", 2, c2, strlen(c2), 2), HU_OK);
    hu_graph_hierarchy_t hier;
    HU_ASSERT_EQ(hu_graph_hierarchy_init(&hier, &alloc), HU_OK);
    HU_ASSERT_EQ(hu_graph_hierarchy_build(&hier, &idx), HU_OK);
    HU_ASSERT_TRUE(hier.cluster_count > 0);
    uint32_t out_idx[8];
    size_t oc = 0;
    const char *q = "ProjectAlpha";
    HU_ASSERT_EQ(hu_graph_hierarchy_traverse(&hier, &idx, q, strlen(q), out_idx, &oc, 8), HU_OK);
    HU_ASSERT_TRUE(oc > 0);
    hu_graph_hierarchy_deinit(&hier);
    hu_graph_index_deinit(&idx);
}

static void test_tool_cacheability_classify(void) {
    HU_ASSERT_EQ(hu_tool_cache_classify("send_message", 12, "{}", 2), HU_TOOL_CACHE_NEVER);
    HU_ASSERT_EQ(hu_tool_cache_classify("get_weather", 11, "{}", 2), HU_TOOL_CACHE_SHORT);
    HU_ASSERT_EQ(hu_tool_cache_classify("web_search", 10, "{}", 2), HU_TOOL_CACHE_MEDIUM);
    HU_ASSERT_EQ(hu_tool_cache_classify("list_tools", 10, "{}", 2), HU_TOOL_CACHE_LONG);
}

static void test_history_scorer_escalation(void) {
    hu_tool_history_entry_t h[6];
    memset(h, 0, sizeof(h));
    h[0].tool_name = "memory_recall";
    h[0].name_len = 13;
    h[0].succeeded = true;
    h[0].risk_level = (uint32_t)HU_RISK_LOW;
    h[1].tool_name = "http_request";
    h[1].name_len = 12;
    h[1].succeeded = true;
    h[1].risk_level = (uint32_t)HU_RISK_MEDIUM;
    h[2].tool_name = "shell";
    h[2].name_len = 5;
    h[2].succeeded = true;
    h[2].risk_level = (uint32_t)HU_RISK_HIGH;
    h[3].tool_name = "file_read";
    h[3].name_len = 9;
    h[3].succeeded = true;
    h[3].risk_level = (uint32_t)HU_RISK_LOW;
    h[4].tool_name = "file_read";
    h[4].name_len = 9;
    h[4].succeeded = true;
    h[4].risk_level = (uint32_t)HU_RISK_LOW;
    h[5].tool_name = "send_message";
    h[5].name_len = 12;
    h[5].succeeded = true;
    h[5].risk_level = (uint32_t)HU_RISK_MEDIUM;
    hu_history_score_result_t out;
    HU_ASSERT_EQ(hu_history_scorer_evaluate(h, 6, "shell", 5, (uint32_t)HU_RISK_HIGH, &out), HU_OK);
    HU_ASSERT_TRUE(out.is_suspicious);
    HU_ASSERT_TRUE(out.escalation_score >= 0.6);
}

static void test_persona_vector_to_directive(void) {
    hu_fuse_adapter_t ad;
    memset(&ad, 0, sizeof(ad));
    ad.formality = 0.6f;
    ad.verbosity = -0.5f;
    ad.emoji_factor = 0.4f;
    ad.warmth_offset = 0.25f;
    hu_persona_vector_t v;
    HU_ASSERT_EQ(hu_persona_vector_from_adapter(&ad, &v), HU_OK);
    char buf[512];
    size_t bl = 0;
    HU_ASSERT_EQ(hu_persona_vector_to_directive(&v, buf, sizeof(buf), &bl), HU_OK);
    HU_ASSERT_TRUE(bl > 20);
    HU_ASSERT_TRUE(strstr(buf, "formal") != NULL || strstr(buf, "Formal") != NULL);
}

/* ── Security gateway ──────────────────────────────────────────────── */

static void test_cp_security_cot_summary_returns_empty_summary(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = cp_security_cot_summary(&alloc, NULL, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    HU_ASSERT(strstr(out, "total_audited") != NULL);
    HU_ASSERT(strstr(out, "entries") != NULL);

    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
}

/* ── Suite registration ──────────────────────────────────────────── */

void run_sota_live_wiring_tests(void) {
    HU_TEST_SUITE("SOTA Live Wiring");

    HU_RUN_TEST(test_prompt_cache_init_deinit);
    HU_RUN_TEST(test_prompt_cache_hash_deterministic);
    HU_RUN_TEST(test_prompt_cache_store_and_lookup);
    HU_RUN_TEST(test_prompt_cache_clear);

    HU_RUN_TEST(test_ttl_cache_init_deinit);
    HU_RUN_TEST(test_ttl_cache_put_get);
    HU_RUN_TEST(test_ttl_cache_default_ttl_varies_by_tool);
    HU_RUN_TEST(test_ttl_cache_evict_expired);

    HU_RUN_TEST(test_emotion_voice_map_default);
    HU_RUN_TEST(test_emotion_voice_map_joy);
    HU_RUN_TEST(test_emotion_detect_from_text);
    HU_RUN_TEST(test_emotion_voice_blend);

    HU_RUN_TEST(test_acp_bridge_map_priority);
    HU_RUN_TEST(test_acp_bridge_map_type);
    HU_RUN_TEST(test_acp_bridge_send_recv_roundtrip);

    HU_RUN_TEST(test_kv_cache_init_deinit);
    HU_RUN_TEST(test_kv_cache_add_and_utilization);
    HU_RUN_TEST(test_kv_cache_prune_evicts_lowest_attention);
    HU_RUN_TEST(test_kv_cache_pinned_not_evicted);

    HU_RUN_TEST(test_persona_fuse_init_and_add);
    HU_RUN_TEST(test_persona_fuse_compose_multiple);
    HU_RUN_TEST(test_persona_fuse_builtin_presets);

    HU_RUN_TEST(test_audio_features_extract_silence);
    HU_RUN_TEST(test_audio_features_extract_f32_silence);
    HU_RUN_TEST(test_audio_emotion_classify_neutral_on_silence);
    HU_RUN_TEST(test_audio_emotion_fuse_agreeing);
    HU_RUN_TEST(test_audio_emotion_fuse_disagreeing);

    HU_RUN_TEST(test_emotion_class_name);

    HU_RUN_TEST(test_eot_audio_backchannel);
    HU_RUN_TEST(test_causal_armor_safe_action);
    HU_RUN_TEST(test_causal_armor_untrusted_dominant);
    HU_RUN_TEST(test_hierarchy_build_and_traverse);
    HU_RUN_TEST(test_tool_cacheability_classify);
    HU_RUN_TEST(test_history_scorer_escalation);
    HU_RUN_TEST(test_persona_vector_to_directive);

    HU_RUN_TEST(test_cp_security_cot_summary_returns_empty_summary);
}
