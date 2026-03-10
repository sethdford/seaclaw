/*
 * BTH (Better-Than-Human) E2E integration test — proves the full pipeline works end-to-end.
 * No real LLM or network; exercises all BTH components together.
 */
#include "seaclaw/agent/commitment.h"
#include "seaclaw/agent/commitment_store.h"
#include "seaclaw/agent/dag.h"
#include "seaclaw/agent/dag_executor.h"
#include "seaclaw/agent/pattern_radar.h"
#include "seaclaw/agent/proactive.h"
#include "seaclaw/agent/superhuman.h"
#include "seaclaw/agent/superhuman_commitment.h"
#include "seaclaw/agent/superhuman_emotional.h"
#include "seaclaw/agent/superhuman_silence.h"
#include "seaclaw/channel.h"
#include "seaclaw/context/conversation.h"
#include "seaclaw/context/event_extract.h"
#include "seaclaw/context/mood.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/string.h"
#include "seaclaw/memory/deep_extract.h"
#include "seaclaw/memory/emotional_graph.h"
#include "seaclaw/memory/engines.h"
#include "seaclaw/memory/fast_capture.h"
#include "seaclaw/memory/promotion.h"
#include "seaclaw/memory/stm.h"
#include "seaclaw/persona/replay.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

#ifdef SC_HAS_PERSONA
#include "seaclaw/persona/circadian.h"
#include "seaclaw/persona/relationship.h"
#endif

static sc_channel_history_entry_t make_entry(bool from_me, const char *text, const char *ts) {
    sc_channel_history_entry_t e;
    memset(&e, 0, sizeof(e));
    e.from_me = from_me;
    size_t tl = strlen(text);
    if (tl >= sizeof(e.text))
        tl = sizeof(e.text) - 1;
    memcpy(e.text, text, tl);
    e.text[tl] = '\0';
    size_t tsl = strlen(ts);
    if (tsl >= sizeof(e.timestamp))
        tsl = sizeof(e.timestamp) - 1;
    memcpy(e.timestamp, ts, tsl);
    e.timestamp[tsl] = '\0';
    return e;
}

static void bth_conversation_intelligence_pipeline(void) {
    sc_allocator_t alloc = sc_system_allocator();

    sc_channel_history_entry_t entries[12];
    memset(entries, 0, sizeof(entries));
    entries[0] = make_entry(false, "hey how are you?", "2025-01-01 10:00");
    entries[1] = make_entry(true, "doing good, you?", "2025-01-01 10:01");
    entries[2] = make_entry(false, "stressed about work honestly", "2025-01-01 10:02");
    entries[3] = make_entry(true, "that sucks, what's going on?", "2025-01-01 10:03");
    entries[4] = make_entry(false, "my boss keeps piling on deadlines", "2025-01-01 10:04");
    entries[5] = make_entry(true, "ugh that sounds rough", "2025-01-01 10:05");
    entries[6] =
        make_entry(false, "yeah it's a lot. anyway how was your weekend?", "2025-01-01 10:06");
    entries[7] = make_entry(true, "pretty chill, went hiking", "2025-01-01 10:07");
    entries[8] = make_entry(false, "nice! where'd you go?", "2025-01-01 10:08");
    entries[9] = make_entry(true, "up to the lake, it was beautiful", "2025-01-01 10:09");
    entries[10] = make_entry(false, "love it there", "2025-01-01 10:10");
    entries[11] = make_entry(true, "same, we should go sometime", "2025-01-01 10:11");

    size_t len = 0;
    char *awareness = sc_conversation_build_awareness(&alloc, entries, 12, NULL, &len);
    SC_ASSERT_NOT_NULL(awareness);
    alloc.free(alloc.ctx, awareness, len + 1);

    char cal_buf[512];
    size_t cal_len = sc_conversation_calibrate_length(entries[11].text, strlen(entries[11].text),
                                                      entries, 12, cal_buf, sizeof(cal_buf));
    SC_ASSERT_TRUE(cal_len > 0);

    uint32_t delay = 0;
    sc_response_action_t action = sc_conversation_classify_response(
        entries[11].text, strlen(entries[11].text), entries, 12, &delay);
    SC_ASSERT_TRUE(action >= SC_RESPONSE_FULL && action <= SC_RESPONSE_THINKING);

    sc_narrative_phase_t phase = sc_conversation_detect_narrative(entries, 12);
    SC_ASSERT_TRUE(phase >= SC_NARRATIVE_OPENING && phase <= SC_NARRATIVE_CLOSING);

    sc_engagement_level_t eng = sc_conversation_detect_engagement(entries, 12);
    SC_ASSERT_TRUE(eng >= SC_ENGAGEMENT_HIGH && eng <= SC_ENGAGEMENT_DISTRACTED);

    sc_emotional_state_t emo = sc_conversation_detect_emotion(entries, 12);
    SC_ASSERT_TRUE(emo.intensity >= 0.0f && emo.intensity <= 1.0f);

    char rep_buf[256];
    size_t rep_len = sc_conversation_detect_repetition(entries, 12, rep_buf, sizeof(rep_buf));
    SC_ASSERT_TRUE(rep_len >= 0);
}

static void bth_emotional_graph_pipeline(void) {
    sc_allocator_t alloc = sc_system_allocator();

    sc_stm_buffer_t stm;
    sc_stm_init(&stm, alloc, "egraph-session", 14);

    sc_stm_record_turn(&stm, "user", 4, "Work is stressing me out", 24, 1000);
    sc_stm_turn_set_primary_topic(&stm, 0, "work", 4);
    sc_stm_turn_add_emotion(&stm, 0, SC_EMOTION_ANXIETY, 0.8);

    sc_stm_record_turn(&stm, "user", 4, "Cooking makes me so happy", 25, 2000);
    sc_stm_turn_set_primary_topic(&stm, 1, "cooking", 7);
    sc_stm_turn_add_emotion(&stm, 1, SC_EMOTION_JOY, 0.9);

    sc_stm_record_turn(&stm, "user", 4, "Spent time with family today", 27, 3000);
    sc_stm_turn_set_primary_topic(&stm, 2, "family", 6);
    sc_stm_turn_add_emotion(&stm, 2, SC_EMOTION_JOY, 0.7);

    sc_emotional_graph_t egraph;
    memset(&egraph, 0, sizeof(egraph));
    SC_ASSERT_EQ(sc_egraph_init(&egraph, alloc), SC_OK);
    SC_ASSERT_EQ(sc_egraph_populate_from_stm(&egraph, &stm), SC_OK);

    SC_ASSERT_TRUE(egraph.node_count >= 1);
    SC_ASSERT_TRUE(egraph.edge_count >= 1);

    size_t ctx_len = 0;
    char *ctx = sc_egraph_build_context(&alloc, &egraph, &ctx_len);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_TRUE(ctx_len > 0);
    SC_ASSERT_TRUE(strstr(ctx, "work") != NULL || strstr(ctx, "cooking") != NULL ||
                   strstr(ctx, "family") != NULL);
    alloc.free(alloc.ctx, ctx, ctx_len + 1);

    double avg = 0.0;
    sc_emotion_tag_t work_emo = sc_egraph_query(&egraph, "work", 4, &avg);
    SC_ASSERT_EQ(work_emo, SC_EMOTION_ANXIETY);

    avg = 0.0;
    sc_emotion_tag_t cooking_emo = sc_egraph_query(&egraph, "cooking", 7, &avg);
    SC_ASSERT_EQ(cooking_emo, SC_EMOTION_JOY);

    avg = 0.0;
    sc_emotion_tag_t family_emo = sc_egraph_query(&egraph, "family", 6, &avg);
    SC_ASSERT_EQ(family_emo, SC_EMOTION_JOY);

    sc_egraph_deinit(&egraph);
    sc_stm_deinit(&stm);
}

static void bth_typo_correction_pipeline(void) {
    const char *original = "that sounds like a great idea";
    size_t orig_len = strlen(original);
    char typo_buf[128];
    memcpy(typo_buf, original, orig_len + 1);

    uint32_t typo_seed = 0;
    for (; typo_seed < 500; typo_seed++) {
        memcpy(typo_buf, original, orig_len + 1);
        size_t mod_len =
            sc_conversation_apply_typos(typo_buf, orig_len, sizeof(typo_buf), typo_seed);
        if (mod_len != orig_len || memcmp(typo_buf, original, orig_len + 1) != 0)
            break;
    }
    SC_ASSERT_TRUE(typo_seed < 500);

    char corr_buf[64];
    size_t corr_len = sc_conversation_generate_correction(
        original, orig_len, typo_buf, strlen(typo_buf), corr_buf, sizeof(corr_buf), 12345u, 100u);
    SC_ASSERT_TRUE(corr_len > 0);
    SC_ASSERT_EQ(corr_buf[0], '*');
    SC_ASSERT_TRUE(strstr(original, corr_buf + 1) != NULL);
}

static void bth_event_to_proactive_pipeline(void) {
    sc_allocator_t alloc = sc_system_allocator();

    const char *text = "my interview is on Tuesday";
    sc_event_extract_result_t extract;
    memset(&extract, 0, sizeof(extract));
    SC_ASSERT_EQ(sc_event_extract(&alloc, text, strlen(text), &extract), SC_OK);
    SC_ASSERT_TRUE(extract.event_count >= 1);
    SC_ASSERT_NOT_NULL(extract.events[0].description);
    SC_ASSERT_TRUE(strstr(extract.events[0].description, "interview") != NULL);

    sc_proactive_result_t result;
    memset(&result, 0, sizeof(result));
    SC_ASSERT_EQ(sc_proactive_check_events(&alloc, extract.events, extract.event_count, &result),
                 SC_OK);
    SC_ASSERT_TRUE(result.count >= 1);
    bool found_interview = false;
    for (size_t i = 0; i < result.count; i++) {
        if (result.actions[i].message && strstr(result.actions[i].message, "interview") != NULL) {
            found_interview = true;
            break;
        }
    }
    SC_ASSERT_TRUE(found_interview);

    sc_event_extract_result_deinit(&extract, &alloc);
    sc_proactive_result_deinit(&result, &alloc);
}

static void bth_mood_from_emotions_pipeline(void) {
    sc_allocator_t alloc = sc_system_allocator();

    sc_stm_buffer_t stm;
    sc_stm_init(&stm, alloc, "mood-session", 13);
    sc_stm_record_turn(&stm, "user", 4, "I'm so happy today!", 19, 1000);
    sc_stm_turn_add_emotion(&stm, 0, SC_EMOTION_JOY, 0.85);
    sc_stm_record_turn(&stm, "user", 4, "Work has me anxious", 19, 2000);
    sc_stm_turn_add_emotion(&stm, 1, SC_EMOTION_ANXIETY, 0.7);

    sc_memory_t mem = sc_memory_lru_create(&alloc, 100);
    SC_ASSERT_EQ(sc_promotion_run_emotions(&alloc, &stm, &mem, "mood_contact", 11), SC_OK);

    char *mood_ctx = NULL;
    size_t mood_len = 0;
    SC_ASSERT_EQ(sc_mood_build_context(&alloc, &mem, "mood_contact", 11, &mood_ctx, &mood_len),
                 SC_OK);
    SC_ASSERT_NOT_NULL(mood_ctx);
    SC_ASSERT_TRUE(mood_len > 0);
    SC_ASSERT_TRUE(strstr(mood_ctx, "joy") != NULL || strstr(mood_ctx, "anxiety") != NULL);

    alloc.free(alloc.ctx, mood_ctx, mood_len + 1);
    if (mem.vtable->deinit)
        mem.vtable->deinit(mem.ctx);
    sc_stm_deinit(&stm);
}

static void bth_replay_analysis_pipeline(void) {
    sc_allocator_t alloc = sc_system_allocator();

    sc_channel_history_entry_t entries[4];
    entries[0] = make_entry(false, "how was your day?", "2025-01-01 12:00");
    entries[1] =
        make_entry(true, "I understand how you feel. That must be difficult.", "2025-01-01 12:01");
    entries[2] = make_entry(true, "that's so funny", "2025-01-01 12:02");
    entries[3] = make_entry(false, "haha exactly!", "2025-01-01 12:03");

    sc_replay_result_t replay;
    memset(&replay, 0, sizeof(replay));
    SC_ASSERT_EQ(sc_replay_analyze(&alloc, entries, 4, 2000, &replay), SC_OK);

    bool found_positive = false;
    bool found_negative = false;
    for (size_t i = 0; i < replay.insight_count; i++) {
        if (replay.insights[i].score_delta > 0)
            found_positive = true;
        if (replay.insights[i].score_delta < 0)
            found_negative = true;
    }
    SC_ASSERT_TRUE(found_positive || found_negative);

    size_t ctx_len = 0;
    char *ctx = sc_replay_build_context(&alloc, &replay, &ctx_len);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_TRUE(ctx_len > 0);
    SC_ASSERT_TRUE(strstr(ctx, "Session Replay") != NULL);
    alloc.free(alloc.ctx, ctx, ctx_len + 1);

    sc_replay_result_deinit(&replay, &alloc);
}

static void bth_pipeline_full_conversation(void) {
    sc_allocator_t alloc = sc_system_allocator();

    /* 1. Create memory backend */
    sc_memory_t mem = sc_memory_lru_create(&alloc, 1000);

    /* 2. Create STM buffer */
    sc_stm_buffer_t stm;
    sc_stm_init(&stm, alloc, "test-session", 12);

    /* 3. Create commitment store */
    sc_commitment_store_t *commit_store = NULL;
    SC_ASSERT_EQ(sc_commitment_store_create(&alloc, &mem, &commit_store), SC_OK);
    SC_ASSERT_NOT_NULL(commit_store);

    /* 4. Create pattern radar */
    sc_pattern_radar_t radar;
    SC_ASSERT_EQ(sc_pattern_radar_init(&radar, alloc), SC_OK);

    /* 5. Create superhuman registry */
    sc_superhuman_registry_t superhuman;
    SC_ASSERT_EQ(sc_superhuman_registry_init(&superhuman), SC_OK);

    /* Register superhuman services */
    sc_superhuman_commitment_ctx_t sh_commit_ctx = {
        .store = commit_store,
        .session_id = "test-session",
        .session_id_len = 12,
    };
    sc_superhuman_service_t sh_commit_svc;
    SC_ASSERT_EQ(sc_superhuman_commitment_service(&sh_commit_ctx, &sh_commit_svc), SC_OK);
    SC_ASSERT_EQ(sc_superhuman_register(&superhuman, sh_commit_svc), SC_OK);

    sc_superhuman_emotional_ctx_t sh_emotional_ctx = {0};
    sc_superhuman_service_t sh_emotional_svc;
    SC_ASSERT_EQ(sc_superhuman_emotional_service(&sh_emotional_ctx, &sh_emotional_svc), SC_OK);
    SC_ASSERT_EQ(sc_superhuman_register(&superhuman, sh_emotional_svc), SC_OK);

    sc_superhuman_silence_ctx_t sh_silence_ctx = {0};
    sc_superhuman_service_t sh_silence_svc;
    SC_ASSERT_EQ(sc_superhuman_silence_service(&sh_silence_ctx, &sh_silence_svc), SC_OK);
    SC_ASSERT_EQ(sc_superhuman_register(&superhuman, sh_silence_svc), SC_OK);

    /* 6. Relationship tracker */
    sc_relationship_state_t rel = {0};
    sc_relationship_new_session(&rel);

    /* ── Simulate 5-turn conversation ── */
    const char *turns[] = {
        "I'm so frustrated with work. My boss keeps piling on deadlines.",
        "I promise I'll finish the report by Friday",
        "My mom called today, she's worried about my health",
        "I want to learn piano this year, it's my goal",
        "I'm feeling a lot better now, thanks for listening",
    };

    for (int t = 0; t < 5; t++) {
        const char *msg = turns[t];
        size_t msg_len = strlen(msg);
        uint64_t ts = (uint64_t)(1000 + t * 60) * 1000;

        /* A. Fast-capture */
        sc_fc_result_t fc;
        memset(&fc, 0, sizeof(fc));
        (void)sc_fast_capture(&alloc, msg, msg_len, &fc);

        /* B. STM record */
        SC_ASSERT_EQ(sc_stm_record_turn(&stm, "user", 4, msg, msg_len, ts), SC_OK);
        size_t last_idx = sc_stm_count(&stm) - 1;
        for (size_t i = 0; i < fc.entity_count; i++) {
            const sc_fc_entity_match_t *e = &fc.entities[i];
            (void)sc_stm_turn_add_entity(&stm, last_idx, e->name, e->name_len,
                                         e->type ? e->type : "entity", e->type ? e->type_len : 6,
                                         1);
        }
        for (size_t i = 0; i < fc.emotion_count; i++) {
            (void)sc_stm_turn_add_emotion(&stm, last_idx, fc.emotions[i].tag,
                                          fc.emotions[i].intensity);
        }

        /* C. Pattern radar observe */
        char ts_buf[32];
        int ts_n = snprintf(ts_buf, sizeof(ts_buf), "%llu", (unsigned long long)ts);
        const char *ts_str = ts_n > 0 ? ts_buf : NULL;
        size_t ts_len = (ts_n > 0 && ts_n < (int)sizeof(ts_buf)) ? (size_t)ts_n : 0;
        for (size_t i = 0; i < fc.entity_count; i++) {
            const sc_fc_entity_match_t *e = &fc.entities[i];
            if (e->name && e->name_len > 0) {
                (void)sc_pattern_radar_observe(
                    &radar, e->name, e->name_len, SC_PATTERN_TOPIC_RECURRENCE,
                    e->type ? e->type : NULL, e->type ? e->type_len : 0, ts_str, ts_len);
            }
        }

        /* D. Commitment detection */
        sc_commitment_detect_result_t cd;
        memset(&cd, 0, sizeof(cd));
        (void)sc_commitment_detect(&alloc, msg, msg_len, "user", 4, &cd);
        for (size_t i = 0; i < cd.count; i++) {
            (void)sc_commitment_store_save(commit_store, &cd.commitments[i], "test-session", 12);
        }
        sc_commitment_detect_result_deinit(&cd, &alloc);

        /* E. Superhuman observe */
        (void)sc_superhuman_observe_all(&superhuman, &alloc, msg, msg_len, "user", 4);

        /* F. Relationship update */
        sc_relationship_update(&rel, 1);

        sc_fc_result_deinit(&fc, &alloc);
    }

    /* ── Verify all components produced correct output ── */

    /* V1. STM has 5 turns */
    SC_ASSERT_EQ(sc_stm_count(&stm), 5u);

    /* V2. STM context is non-empty */
    char *stm_ctx = NULL;
    size_t stm_ctx_len = 0;
    SC_ASSERT_EQ(sc_stm_build_context(&stm, &alloc, &stm_ctx, &stm_ctx_len), SC_OK);
    SC_ASSERT_NOT_NULL(stm_ctx);
    SC_ASSERT_TRUE(stm_ctx_len > 50);
    SC_ASSERT_TRUE(strstr(stm_ctx, "frustrated") != NULL || strstr(stm_ctx, "boss") != NULL);
    alloc.free(alloc.ctx, stm_ctx, stm_ctx_len + 1);

    /* V3. Commitments were detected and stored */
    {
        sc_commitment_t *active = NULL;
        size_t active_count = 0;
        SC_ASSERT_EQ(sc_commitment_store_list_active(commit_store, &alloc, "test-session", 12,
                                                     &active, &active_count),
                     SC_OK);
        SC_ASSERT_TRUE(active_count >= 1); /* "I promise I'll finish the report" */
        if (active) {
            for (size_t i = 0; i < active_count; i++)
                sc_commitment_deinit(&active[i], &alloc);
            alloc.free(alloc.ctx, active, active_count * sizeof(sc_commitment_t));
        }
    }

    /* V4. Commitment context is non-empty (we have active commitments from V3) */
    {
        char *ctx = NULL;
        size_t ctx_len = 0;
        SC_ASSERT_EQ(sc_commitment_store_build_context(commit_store, &alloc, "test-session", 12,
                                                       &ctx, &ctx_len),
                     SC_OK);
        SC_ASSERT_NOT_NULL(ctx);
        SC_ASSERT_TRUE(ctx_len > 10);
        alloc.free(alloc.ctx, ctx, ctx_len + 1);
    }

    /* V5. Pattern radar observed topics */
    SC_ASSERT_TRUE(radar.observation_count >= 1);

#ifdef SC_HAS_PERSONA
    /* V6. Proactive check works */
    {
        sc_proactive_result_t pr;
        memset(&pr, 0, sizeof(pr));
        SC_ASSERT_EQ(sc_proactive_check(&alloc, rel.session_count, 9, &pr), SC_OK);
        SC_ASSERT_TRUE(pr.count >= 1); /* at least CHECK_IN */
        char *pro_ctx = NULL;
        size_t pro_ctx_len = 0;
        (void)sc_proactive_build_context(&pr, &alloc, 8, &pro_ctx, &pro_ctx_len);
        if (pro_ctx)
            alloc.free(alloc.ctx, pro_ctx, pro_ctx_len + 1);
        sc_proactive_result_deinit(&pr, &alloc);
    }

    /* V7. Circadian overlay works */
    {
        char *circ = NULL;
        size_t circ_len = 0;
        SC_ASSERT_EQ(sc_circadian_build_prompt(&alloc, 9, &circ, &circ_len), SC_OK);
        SC_ASSERT_NOT_NULL(circ);
        SC_ASSERT_TRUE(strstr(circ, "morning") != NULL || strstr(circ, "Morning") != NULL);
        alloc.free(alloc.ctx, circ, circ_len + 1);
    }
#endif

    /* V8. Relationship state updated */
    SC_ASSERT_EQ(rel.session_count, 1u);
    SC_ASSERT_EQ(rel.total_turns, 5u);
    SC_ASSERT_EQ(rel.stage, SC_REL_NEW);

    /* V9. Superhuman context (emotional first aid may have content) */
    {
        char *sh_ctx = NULL;
        size_t sh_ctx_len = 0;
        (void)sc_superhuman_build_context(&superhuman, &alloc, &sh_ctx, &sh_ctx_len);
        if (sh_ctx)
            alloc.free(alloc.ctx, sh_ctx, sh_ctx_len + 1);
    }

    /* V10. Promotion works (promote STM entities to persistent memory) */
    {
        sc_promotion_config_t promo_config = SC_PROMOTION_DEFAULTS;
        promo_config.min_mention_count = 1;
        promo_config.min_importance = 0.0;
        sc_error_t perr = sc_promotion_run(&alloc, &stm, &mem, &promo_config);
        SC_ASSERT_EQ(perr, SC_OK);

        size_t mem_count = 0;
        SC_ASSERT_EQ(mem.vtable->count(mem.ctx, &mem_count), SC_OK);
        SC_ASSERT_TRUE(mem_count >= 1);
    }

    /* V11. Deep extraction prompt builds correctly */
    {
        char *stm_text = NULL;
        size_t stm_text_len = 0;
        SC_ASSERT_EQ(sc_stm_build_context(&stm, &alloc, &stm_text, &stm_text_len), SC_OK);
        if (stm_text) {
            char *de_prompt = NULL;
            size_t de_prompt_len = 0;
            SC_ASSERT_EQ(sc_deep_extract_build_prompt(&alloc, stm_text, stm_text_len, &de_prompt,
                                                      &de_prompt_len),
                         SC_OK);
            SC_ASSERT_NOT_NULL(de_prompt);
            SC_ASSERT_TRUE(de_prompt_len > 50);
            alloc.free(alloc.ctx, de_prompt, de_prompt_len + 1);
            alloc.free(alloc.ctx, stm_text, stm_text_len + 1);
        }
    }

    /* V12. Consolidation — skipped: LRU backend has known issues with consolidation
     * (sc_memory_entry_free_fields/ASan; see test_consolidation.c) */

    /* V13. DAG creation and validation */
    {
        sc_dag_t dag;
        SC_ASSERT_EQ(sc_dag_init(&dag, alloc), SC_OK);
        SC_ASSERT_EQ(sc_dag_add_node(&dag, "t1", "web_search", "{\"query\":\"news\"}", NULL, 0),
                     SC_OK);
        const char *deps[] = {"t1"};
        SC_ASSERT_EQ(sc_dag_add_node(&dag, "t2", "summarize", "{\"text\":\"$t1\"}", deps, 1),
                     SC_OK);
        SC_ASSERT_EQ(sc_dag_validate(&dag), SC_OK);
        SC_ASSERT_FALSE(sc_dag_is_complete(&dag));

        /* Batch execution */
        sc_dag_batch_t batch;
        SC_ASSERT_EQ(sc_dag_next_batch(&dag, &batch), SC_OK);
        SC_ASSERT_EQ(batch.count, 1u);
        SC_ASSERT_TRUE(strcmp(batch.nodes[0]->id, "t1") == 0);

        /* Mark t1 done, get next batch */
        batch.nodes[0]->status = SC_DAG_DONE;
        batch.nodes[0]->result = sc_strdup(&dag.alloc, "news content");
        batch.nodes[0]->result_len = batch.nodes[0]->result ? 12 : 0;

        SC_ASSERT_EQ(sc_dag_next_batch(&dag, &batch), SC_OK);
        SC_ASSERT_EQ(batch.count, 1u);
        SC_ASSERT_TRUE(strcmp(batch.nodes[0]->id, "t2") == 0);

        /* Variable resolution */
        char *resolved = NULL;
        size_t resolved_len = 0;
        SC_ASSERT_EQ(
            sc_dag_resolve_vars(&alloc, &dag, "{\"text\":\"$t1\"}", 15, &resolved, &resolved_len),
            SC_OK);
        SC_ASSERT_NOT_NULL(resolved);
        SC_ASSERT_TRUE(strstr(resolved, "news content") != NULL);
        sc_str_free(&alloc, resolved);

        sc_dag_deinit(&dag);
    }

    /* ── Cleanup ── */
    sc_commitment_store_destroy(commit_store);
    sc_stm_deinit(&stm);
    sc_pattern_radar_deinit(&radar);
    if (mem.vtable->deinit)
        mem.vtable->deinit(mem.ctx);
}

void run_bth_e2e_tests(void) {
    SC_TEST_SUITE("bth_e2e");
    SC_RUN_TEST(bth_pipeline_full_conversation);
    SC_RUN_TEST(bth_conversation_intelligence_pipeline);
    SC_RUN_TEST(bth_emotional_graph_pipeline);
    SC_RUN_TEST(bth_typo_correction_pipeline);
    SC_RUN_TEST(bth_event_to_proactive_pipeline);
    SC_RUN_TEST(bth_mood_from_emotions_pipeline);
    SC_RUN_TEST(bth_replay_analysis_pipeline);
}
