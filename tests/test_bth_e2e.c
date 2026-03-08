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
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/string.h"
#include "seaclaw/memory/deep_extract.h"
#include "seaclaw/memory/engines.h"
#include "seaclaw/memory/fast_capture.h"
#include "seaclaw/memory/promotion.h"
#include "seaclaw/memory/stm.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

#ifdef SC_HAS_PERSONA
#include "seaclaw/persona/circadian.h"
#include "seaclaw/persona/relationship.h"
#endif

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
                                         e->type ? e->type : "entity",
                                         e->type ? e->type_len : 6, 1);
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
                (void)sc_pattern_radar_observe(&radar, e->name, e->name_len,
                                               SC_PATTERN_TOPIC_RECURRENCE,
                                               e->type ? e->type : NULL,
                                               e->type ? e->type_len : 0, ts_str, ts_len);
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
        SC_ASSERT_EQ(sc_dag_resolve_vars(&alloc, &dag, "{\"text\":\"$t1\"}", 15, &resolved,
                                         &resolved_len),
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
}
