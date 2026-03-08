#include "seaclaw/core/allocator.h"
#include "seaclaw/memory/deep_extract.h"
#include "test_framework.h"
#include <string.h>

static void deep_extract_build_prompt_includes_conversation(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *conv = "User: I have a meeting with Alice.\nAssistant: Good luck!";
    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_deep_extract_build_prompt(&alloc, conv, strlen(conv), &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_TRUE(strstr(out, "Extract structured") != NULL);
    SC_ASSERT_TRUE(strstr(out, "Conversation:") != NULL);
    SC_ASSERT_TRUE(strstr(out, "Alice") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void deep_extract_parse_extracts_facts(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *json =
        "{\"facts\":[{\"subject\":\"Alice\",\"predicate\":\"works_at\",\"object\":\"Acme\","
        "\"confidence\":0.9}],\"relations\":[],\"summary\":\"User mentioned Alice.\"}";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_parse(&alloc, json, strlen(json), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "Alice");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "works_at");
    SC_ASSERT_STR_EQ(out.facts[0].object, "Acme");
    SC_ASSERT_FLOAT_EQ(out.facts[0].confidence, 0.9, 0.001);
    SC_ASSERT_NOT_NULL(out.summary);
    SC_ASSERT_TRUE(strstr(out.summary, "Alice") != NULL);
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void deep_extract_parse_extracts_relations(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *json =
        "{\"facts\":[],\"relations\":[{\"entity_a\":\"Bob\",\"relation\":\"reports_to\","
        "\"entity_b\":\"Alice\",\"confidence\":0.85}],\"summary\":\"Bob reports to Alice.\"}";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_parse(&alloc, json, strlen(json), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.relation_count, 1);
    SC_ASSERT_STR_EQ(out.relations[0].entity_a, "Bob");
    SC_ASSERT_STR_EQ(out.relations[0].relation, "reports_to");
    SC_ASSERT_STR_EQ(out.relations[0].entity_b, "Alice");
    SC_ASSERT_FLOAT_EQ(out.relations[0].confidence, 0.85, 0.001);
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void deep_extract_parse_handles_empty_response(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_parse(&alloc, "", 0, &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 0);
    SC_ASSERT_EQ(out.relation_count, 0);
    SC_ASSERT_NULL(out.summary);
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void deep_extract_parse_handles_malformed_json(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_parse(&alloc, "{invalid json", 13, &out);
    SC_ASSERT_NEQ(err, SC_OK);
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_work_at(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I work at Google";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "works_at");
    SC_ASSERT_STR_EQ(out.facts[0].object, "Google");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_like(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I like hiking";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "likes");
    SC_ASSERT_STR_EQ(out.facts[0].object, "hiking");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_live_in(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I live in Austin";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "lives_in");
    SC_ASSERT_STR_EQ(out.facts[0].object, "Austin");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_null_input(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, NULL, 0, &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 0);
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_case_insensitive(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I WORK AT Acme Corp";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "works_at");
    SC_ASSERT_STR_EQ(out.facts[0].object, "Acme Corp");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_no_match(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "hello how are you";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 0);
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_is_a(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I'm a software engineer";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "is_a");
    SC_ASSERT_STR_EQ(out.facts[0].object, "software engineer");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_loves(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I love cooking pasta";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "loves");
    SC_ASSERT_STR_EQ(out.facts[0].object, "cooking pasta");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_hates(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I hate mornings";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "hates");
    SC_ASSERT_STR_EQ(out.facts[0].object, "mornings");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_name(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "my name is Sarah";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "name");
    SC_ASSERT_STR_EQ(out.facts[0].object, "Sarah");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_extracts_job(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "my job is teaching";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 1);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "job");
    SC_ASSERT_STR_EQ(out.facts[0].object, "teaching");
    sc_deep_extract_result_deinit(&out, &alloc);
}

static void lightweight_build_prompt_null_alloc(void) {
    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_deep_extract_build_prompt(NULL, "hello", 5, &out, &out_len);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_NULL(out);
}

static void lightweight_multiple_facts(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *text = "I work at Google. I live in Austin";
    sc_deep_extract_result_t out;
    sc_error_t err = sc_deep_extract_lightweight(&alloc, text, strlen(text), &out);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(out.fact_count, 2);
    SC_ASSERT_STR_EQ(out.facts[0].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[0].predicate, "works_at");
    SC_ASSERT_STR_EQ(out.facts[0].object, "Google");
    SC_ASSERT_STR_EQ(out.facts[1].subject, "user");
    SC_ASSERT_STR_EQ(out.facts[1].predicate, "lives_in");
    SC_ASSERT_STR_EQ(out.facts[1].object, "Austin");
    sc_deep_extract_result_deinit(&out, &alloc);
}

void run_deep_extract_tests(void) {
    SC_TEST_SUITE("deep_extract");
    SC_RUN_TEST(deep_extract_build_prompt_includes_conversation);
    SC_RUN_TEST(deep_extract_parse_extracts_facts);
    SC_RUN_TEST(deep_extract_parse_extracts_relations);
    SC_RUN_TEST(deep_extract_parse_handles_empty_response);
    SC_RUN_TEST(deep_extract_parse_handles_malformed_json);
    SC_RUN_TEST(lightweight_extracts_work_at);
    SC_RUN_TEST(lightweight_extracts_like);
    SC_RUN_TEST(lightweight_extracts_live_in);
    SC_RUN_TEST(lightweight_extracts_case_insensitive);
    SC_RUN_TEST(lightweight_null_input);
    SC_RUN_TEST(lightweight_no_match);
    SC_RUN_TEST(lightweight_extracts_is_a);
    SC_RUN_TEST(lightweight_extracts_loves);
    SC_RUN_TEST(lightweight_extracts_hates);
    SC_RUN_TEST(lightweight_extracts_name);
    SC_RUN_TEST(lightweight_extracts_job);
    SC_RUN_TEST(lightweight_build_prompt_null_alloc);
    SC_RUN_TEST(lightweight_multiple_facts);
}
