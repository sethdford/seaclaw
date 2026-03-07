#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/doctor.h"
#include "seaclaw/memory/retrieval.h"
#include "seaclaw/memory/retrieval/query_expansion.h"
#include "seaclaw/skillforge.h"
#include "seaclaw/tool.h"
#include "seaclaw/tools/calendar_tool.h"
#include "seaclaw/tools/crm.h"
#include "seaclaw/tools/firebase.h"
#include "seaclaw/tools/gcloud.h"
#include "seaclaw/tools/invoice.h"
#include "seaclaw/tools/jira.h"
#include "seaclaw/tools/spreadsheet.h"
#include "seaclaw/tools/web_search_providers.h"
#include "seaclaw/tools/workflow.h"
#include "test_framework.h"
#include <string.h>

/* ─── Query Expansion ────────────────────────────────────────────────────── */

static void test_query_expand_basic(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_expanded_query_t eq = {0};
    sc_error_t err = sc_query_expand(&alloc, "how do embeddings work", 22, &eq);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(eq.fts5_query);
    sc_expanded_query_free(&alloc, &eq);
}

static void test_query_expand_null_alloc(void) {
    sc_expanded_query_t eq = {0};
    sc_error_t err = sc_query_expand(NULL, "how do embeddings work", 22, &eq);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_query_expand_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_expanded_query_t eq = {0};
    sc_error_t err = sc_query_expand(&alloc, NULL, 22, &eq);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_query_expand_stop_words_removed(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_expanded_query_t eq = {0};
    const char *q = "the a an is are";
    sc_error_t err = sc_query_expand(&alloc, q, strlen(q), &eq);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_TRUE(eq.filtered_count < eq.original_count);
    sc_expanded_query_free(&alloc, &eq);
}

static void test_expanded_query_free_null_alloc(void) {
    sc_expanded_query_t eq = {0};
    sc_expanded_query_free(NULL, &eq);
    /* Should not crash */
}

/* ─── Temporal Decay ─────────────────────────────────────────────────────── */

static void test_temporal_decay_recent(void) {
    const char *ts = "2026-03-06T12:00:00Z";
    double score = sc_temporal_decay_score(1.0, 0.01, ts, strlen(ts));
    SC_ASSERT_TRUE(score > 0.9);
    SC_ASSERT_TRUE(score <= 1.0);
}

static void test_temporal_decay_old(void) {
    const char *ts = "2020-01-01T00:00:00Z";
    double score = sc_temporal_decay_score(1.0, 0.01, ts, strlen(ts));
    SC_ASSERT_TRUE(score < 0.5);
    SC_ASSERT_TRUE(score >= 0.0);
}

static void test_temporal_decay_null_timestamp(void) {
    double score = sc_temporal_decay_score(1.0, 0.5, NULL, 0);
    SC_ASSERT_FLOAT_EQ(score, 1.0, 1e-9);
}

static void test_temporal_decay_zero_factor(void) {
    const char *ts = "2020-01-01T00:00:00Z";
    double score = sc_temporal_decay_score(1.0, 0.0, ts, strlen(ts));
    SC_ASSERT_FLOAT_EQ(score, 1.0, 1e-9);
}

/* ─── Business Tools ─────────────────────────────────────────────────────── */

static void test_spreadsheet_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_spreadsheet_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_invoice_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_invoice_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_crm_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_crm_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_jira_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_jira_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_calendar_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_calendar_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_workflow_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_workflow_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_gcloud_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_gcloud_create(&alloc, NULL, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_firebase_create(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool;
    sc_error_t err = sc_firebase_create(&alloc, NULL, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.vtable->name);
    SC_ASSERT_NOT_NULL(tool.vtable->description);
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json);
    const char *name = tool.vtable->name(tool.ctx);
    SC_ASSERT_NOT_NULL(name);
    SC_ASSERT_TRUE(strlen(name) > 0);
    if (tool.vtable->deinit)
        tool.vtable->deinit(tool.ctx, &alloc);
}

/* ─── Web Search Providers (null/invalid arg paths only) ───────────────────── */

static void test_web_search_brave_null_alloc(void) {
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_brave(NULL, "q", 1, 1, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_brave_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_brave(&alloc, NULL, 0, 1, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_brave_zero_count(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_brave(&alloc, "q", 1, 0, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_duckduckgo_null_alloc(void) {
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_duckduckgo(NULL, "q", 1, 1, &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_duckduckgo_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_duckduckgo(&alloc, NULL, 0, 1, &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_duckduckgo_zero_count(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_duckduckgo(&alloc, "q", 1, 0, &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_exa_null_alloc(void) {
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_exa(NULL, "q", 1, 1, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_exa_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_exa(&alloc, NULL, 0, 1, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_exa_zero_count(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_exa(&alloc, "q", 1, 0, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_perplexity_null_alloc(void) {
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_perplexity(NULL, "q", 1, 1, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_perplexity_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_perplexity(&alloc, NULL, 0, 1, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_perplexity_zero_count(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_perplexity(&alloc, "q", 1, 0, "key", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_searxng_null_alloc(void) {
    sc_tool_result_t result = {0};
    sc_error_t err = sc_web_search_searxng(NULL, "q", 1, 1, "https://search.example.com", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_searxng_null_query(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err =
        sc_web_search_searxng(&alloc, NULL, 0, 1, "https://search.example.com", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_web_search_searxng_zero_count(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_result_t result = {0};
    sc_error_t err =
        sc_web_search_searxng(&alloc, "q", 1, 0, "https://search.example.com", &result);
    SC_ASSERT_NEQ(err, SC_OK);
}

/* ─── Config Getters ──────────────────────────────────────────────────────── */

static void test_config_get_provider_key_null(void) {
    const char *key = sc_config_get_provider_key(NULL, "openai");
    SC_ASSERT_NULL(key);
}

static void test_config_get_provider_key_null_name(void) {
    sc_config_t cfg = {0};
    const char *key = sc_config_get_provider_key(&cfg, NULL);
    SC_ASSERT_NULL(key);
}

static void test_config_get_web_search_provider_null(void) {
    /* NULL cfg returns default "duckduckgo", not NULL */
    const char *provider = sc_config_get_web_search_provider(NULL);
    SC_ASSERT_NOT_NULL(provider);
}

/* ─── Doctor ─────────────────────────────────────────────────────────────── */

static void test_doctor_parse_df_basic(void) {
    const char *df_output = "Filesystem  1K-blocks  Used  Available\n"
                            "/dev/disk1  500000  200000  300000";
    unsigned long mb = sc_doctor_parse_df_available_mb(df_output, strlen(df_output));
    SC_ASSERT_TRUE(mb > 0);
}

static void test_doctor_parse_df_null(void) {
    unsigned long mb = sc_doctor_parse_df_available_mb(NULL, 0);
    SC_ASSERT_EQ(mb, 0u);
}

static void test_doctor_truncate_null_alloc(void) {
    char *out = NULL;
    sc_error_t err = sc_doctor_truncate_for_display(NULL, "hello", 5, 10, &out);
    SC_ASSERT_NEQ(err, SC_OK);
}

/* ─── SkillForge ─────────────────────────────────────────────────────────── */

static void test_skillforge_create_destroy(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skillforge_t sf = {0};
    sc_error_t err = sc_skillforge_create(&alloc, &sf);
    SC_ASSERT_EQ(err, SC_OK);
    sc_skillforge_destroy(&sf);
}

static void test_skillforge_create_null_alloc(void) {
    sc_skillforge_t sf = {0};
    sc_error_t err = sc_skillforge_create(NULL, &sf);
    SC_ASSERT_NEQ(err, SC_OK);
}

static void test_skillforge_list_empty(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skillforge_t sf = {0};
    sc_skillforge_create(&alloc, &sf);
    sc_skill_t *out = NULL;
    size_t count = 0;
    sc_error_t err = sc_skillforge_list_skills(&sf, &out, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);
    sc_skillforge_destroy(&sf);
}

static void test_skillforge_get_unknown(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_skillforge_t sf = {0};
    sc_skillforge_create(&alloc, &sf);
    sc_skill_t *s = sc_skillforge_get_skill(&sf, "nonexistent");
    SC_ASSERT_NULL(s);
    sc_skillforge_destroy(&sf);
}

/* ─── Run ────────────────────────────────────────────────────────────────── */

void run_coverage_new_tests(void) {
    SC_TEST_SUITE("coverage_new");

    SC_RUN_TEST(test_query_expand_basic);
    SC_RUN_TEST(test_query_expand_null_alloc);
    SC_RUN_TEST(test_query_expand_null_query);
    SC_RUN_TEST(test_query_expand_stop_words_removed);
    SC_RUN_TEST(test_expanded_query_free_null_alloc);

    SC_RUN_TEST(test_temporal_decay_recent);
    SC_RUN_TEST(test_temporal_decay_old);
    SC_RUN_TEST(test_temporal_decay_null_timestamp);
    SC_RUN_TEST(test_temporal_decay_zero_factor);

    SC_RUN_TEST(test_spreadsheet_create);
    SC_RUN_TEST(test_invoice_create);
    SC_RUN_TEST(test_crm_create);
    SC_RUN_TEST(test_jira_create);
    SC_RUN_TEST(test_calendar_create);
    SC_RUN_TEST(test_workflow_create);
    SC_RUN_TEST(test_gcloud_create);
    SC_RUN_TEST(test_firebase_create);

    SC_RUN_TEST(test_web_search_brave_null_alloc);
    SC_RUN_TEST(test_web_search_brave_null_query);
    SC_RUN_TEST(test_web_search_brave_zero_count);
    SC_RUN_TEST(test_web_search_duckduckgo_null_alloc);
    SC_RUN_TEST(test_web_search_duckduckgo_null_query);
    SC_RUN_TEST(test_web_search_duckduckgo_zero_count);
    SC_RUN_TEST(test_web_search_exa_null_alloc);
    SC_RUN_TEST(test_web_search_exa_null_query);
    SC_RUN_TEST(test_web_search_exa_zero_count);
    SC_RUN_TEST(test_web_search_perplexity_null_alloc);
    SC_RUN_TEST(test_web_search_perplexity_null_query);
    SC_RUN_TEST(test_web_search_perplexity_zero_count);
    SC_RUN_TEST(test_web_search_searxng_null_alloc);
    SC_RUN_TEST(test_web_search_searxng_null_query);
    SC_RUN_TEST(test_web_search_searxng_zero_count);

    SC_RUN_TEST(test_config_get_provider_key_null);
    SC_RUN_TEST(test_config_get_provider_key_null_name);
    SC_RUN_TEST(test_config_get_web_search_provider_null);

    SC_RUN_TEST(test_doctor_parse_df_basic);
    SC_RUN_TEST(test_doctor_parse_df_null);
    SC_RUN_TEST(test_doctor_truncate_null_alloc);

    SC_RUN_TEST(test_skillforge_create_destroy);
    SC_RUN_TEST(test_skillforge_create_null_alloc);
    SC_RUN_TEST(test_skillforge_list_empty);
    SC_RUN_TEST(test_skillforge_get_unknown);
}
