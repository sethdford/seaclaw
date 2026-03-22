#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include "human/cognition/db.h"
#include "human/cognition/episodic.h"

static hu_allocator_t alloc;
static sqlite3 *db;

static void setup(void) {
    alloc = hu_system_allocator();
    hu_error_t err = hu_cognition_db_open(&db);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(db);
}

static void teardown(void) {
    hu_cognition_db_close(db);
    db = NULL;
}

static void schema_creation_succeeds(void) {
    setup();
    hu_error_t err = hu_episodic_init_schema(db);
    HU_ASSERT_EQ(err, HU_OK);
    teardown();
}

static void store_and_retrieve_pattern(void) {
    setup();

    hu_episodic_pattern_t p = {
        .id = "test-id-001",
        .problem_type = "architecture",
        .approach = "Used first-principles decomposition",
        .skills_used = "first-principles,design-thinking",
        .outcome_quality = 0.9f,
        .support_count = 1,
        .insight = "Decomposing before designing prevented rework",
        .session_id = "sess_1",
        .timestamp = NULL,
    };

    hu_error_t err = hu_episodic_store_pattern(db, &alloc, &p);
    HU_ASSERT_EQ(err, HU_OK);

    hu_episodic_pattern_t *results = NULL;
    size_t count = 0;
    err = hu_episodic_retrieve(db, &alloc, "architecture", 12, 5, &results, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, (size_t)1);
    HU_ASSERT_STR_EQ(results[0].problem_type, "architecture");
    HU_ASSERT_TRUE(results[0].outcome_quality > 0.8f);

    hu_episodic_free_patterns(&alloc, results, count);
    teardown();
}

static void extract_from_session_with_skills(void) {
    setup();

    const char *tools[] = {"shell", "file_read", "file_write"};
    const char *skills[] = {"design-thinking", "brainstorming"};

    hu_episodic_session_summary_t summary = {
        .session_id = "sess_2",
        .session_id_len = 6,
        .tool_names = tools,
        .tool_count = 3,
        .skill_names = skills,
        .skill_count = 2,
        .had_positive_feedback = true,
        .had_correction = false,
        .topic = "refactoring",
        .topic_len = 11,
    };

    hu_error_t err = hu_episodic_extract_and_store(db, &alloc, &summary);
    HU_ASSERT_EQ(err, HU_OK);

    hu_episodic_pattern_t *results = NULL;
    size_t count = 0;
    err = hu_episodic_retrieve(db, &alloc, "refactoring", 11, 5, &results, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 1);
    HU_ASSERT_TRUE(results[0].outcome_quality > 0.7f);

    hu_episodic_free_patterns(&alloc, results, count);
    teardown();
}

static void extract_skips_empty_session(void) {
    setup();

    hu_episodic_session_summary_t summary = {
        .session_id = "sess_empty",
        .session_id_len = 10,
        .tool_names = NULL,
        .tool_count = 0,
        .skill_names = NULL,
        .skill_count = 0,
        .had_positive_feedback = false,
        .had_correction = false,
        .topic = NULL,
        .topic_len = 0,
    };

    hu_error_t err = hu_episodic_extract_and_store(db, &alloc, &summary);
    HU_ASSERT_EQ(err, HU_OK);
    teardown();
}

static void build_replay_formats_patterns(void) {
    alloc = hu_system_allocator();

    hu_episodic_pattern_t patterns[] = {
        {.problem_type = "debugging", .approach = "Used systematic debugging",
         .outcome_quality = 0.85f, .support_count = 3, .insight = "Start with logs"},
        {.problem_type = "testing", .approach = "TDD approach",
         .outcome_quality = 0.7f, .support_count = 1, .insight = NULL},
    };

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_episodic_build_replay(&alloc, patterns, 2, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Cognitive Replay") != NULL);
    HU_ASSERT_TRUE(strstr(out, "debugging") != NULL);
    HU_ASSERT_TRUE(strstr(out, "85%") != NULL);
    HU_ASSERT_TRUE(strstr(out, "3 times") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void build_replay_empty_returns_null(void) {
    alloc = hu_system_allocator();

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_episodic_build_replay(&alloc, NULL, 0, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(out == NULL);
}

static void compress_merges_similar_patterns(void) {
    setup();

    for (int i = 0; i < 4; i++) {
        char id[32];
        snprintf(id, sizeof(id), "cmp-%d", i);
        hu_episodic_pattern_t p = {
            .id = id,
            .problem_type = "config-parsing",
            .approach = "Used structured validation",
            .skills_used = "critical-thinking",
            .outcome_quality = 0.7f + (float)i * 0.05f,
            .support_count = 1,
            .insight = "Validate early",
            .session_id = "sess_cmp",
            .timestamp = NULL,
        };
        hu_episodic_store_pattern(db, &alloc, &p);
    }

    hu_error_t err = hu_episodic_compress(db, &alloc);
    HU_ASSERT_EQ(err, HU_OK);

    hu_episodic_pattern_t *results = NULL;
    size_t count = 0;
    err = hu_episodic_retrieve(db, &alloc, "config-parsing", 14, 10, &results, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, (size_t)1);
    HU_ASSERT_TRUE(results[0].support_count >= 4);

    hu_episodic_free_patterns(&alloc, results, count);
    teardown();
}

static void metacog_history_insert_and_outcome_update(void) {
    setup();
    HU_ASSERT_EQ(hu_metacog_history_insert(db, "trace-metacog-1", 1, 0.7f, 0.6f, 0.1f, 0.2f, 0.8f,
                                           0.55f, "reflect", "medium", 0, NULL),
                 HU_OK);
    HU_ASSERT_EQ(hu_metacog_history_update_outcome(db, "trace-metacog-1", 0.9f), HU_OK);
    teardown();
}

static void null_db_returns_error(void) {
    hu_error_t err = hu_episodic_init_schema(NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

#endif /* HU_ENABLE_SQLITE */

void run_episodic_tests(void) {
    HU_TEST_SUITE("Episodic");
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(schema_creation_succeeds);
    HU_RUN_TEST(store_and_retrieve_pattern);
    HU_RUN_TEST(extract_from_session_with_skills);
    HU_RUN_TEST(extract_skips_empty_session);
    HU_RUN_TEST(build_replay_formats_patterns);
    HU_RUN_TEST(build_replay_empty_returns_null);
    HU_RUN_TEST(compress_merges_similar_patterns);
    HU_RUN_TEST(metacog_history_insert_and_outcome_update);
    HU_RUN_TEST(null_db_returns_error);
#endif
}
