#include "human/core/allocator.h"
#include "human/core/error.h"
#include "test_framework.h"
#include <string.h>

#ifdef HU_ENABLE_SQLITE
#include "human/cognition/db.h"
#include "human/cognition/evolving.h"

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
    hu_error_t err = hu_evolving_init_schema(db);
    HU_ASSERT_EQ(err, HU_OK);
    teardown();
}

static void record_explicit_invocation_roundtrip(void) {
    setup();

    hu_skill_invocation_t inv = {
        .skill_name = "brainstorming",
        .skill_name_len = 13,
        .contact_id = "user_a",
        .contact_id_len = 6,
        .session_id = "sess_1",
        .session_id_len = 6,
        .explicit_run = true,
        .outcome = HU_SKILL_OUTCOME_POSITIVE,
    };
    hu_error_t err = hu_evolving_record_invocation(db, &inv);
    HU_ASSERT_EQ(err, HU_OK);

    hu_skill_profile_t *profiles = NULL;
    size_t count = 0;
    err = hu_evolving_load_profiles(db, &alloc, "user_a", 6, &profiles, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(count >= 1);
    HU_ASSERT_STR_EQ(profiles[0].skill_name, "brainstorming");
    HU_ASSERT_EQ(profiles[0].total_invocations, 1u);
    HU_ASSERT_EQ(profiles[0].positive_outcomes, 1u);

    hu_evolving_free_profiles(&alloc, profiles, count);
    teardown();
}

static void implicit_exposure_records_multiple(void) {
    setup();

    const char *skills[] = {"critical-thinking", "systems-thinking", "first-principles"};
    hu_error_t err = hu_evolving_record_implicit_exposure(
        db, skills, 3, "user_b", 6, "sess_2", 6);
    HU_ASSERT_EQ(err, HU_OK);

    hu_skill_profile_t *profiles = NULL;
    size_t count = 0;
    err = hu_evolving_load_profiles(db, &alloc, "user_b", 6, &profiles, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, (size_t)3);

    hu_evolving_free_profiles(&alloc, profiles, count);
    teardown();
}

static void outcome_collection_updates_recent(void) {
    setup();

    hu_skill_invocation_t inv = {
        .skill_name = "design-thinking",
        .skill_name_len = 15,
        .contact_id = "user_c",
        .contact_id_len = 6,
        .session_id = "sess_3",
        .session_id_len = 6,
        .explicit_run = true,
        .outcome = HU_SKILL_OUTCOME_NEUTRAL,
    };
    hu_evolving_record_invocation(db, &inv);

    hu_error_t err = hu_evolving_collect_outcome(
        db, "user_c", 6, "sess_3", 6, HU_SKILL_OUTCOME_NEGATIVE);
    HU_ASSERT_EQ(err, HU_OK);

    teardown();
}

static void compute_weight_neutral_when_no_data(void) {
    hu_skill_profile_t p = {0};
    double w = hu_evolving_compute_weight(&p);
    HU_ASSERT_TRUE(w >= 0.99 && w <= 1.01);
}

static void compute_weight_higher_with_positive(void) {
    hu_skill_profile_t p = {
        .total_invocations = 10,
        .positive_outcomes = 8,
        .negative_outcomes = 1,
        .decayed_score = 0.5,
    };
    double w = hu_evolving_compute_weight(&p);
    HU_ASSERT_TRUE(w > 1.0);
}

static void compute_weight_lower_with_negative(void) {
    hu_skill_profile_t p = {
        .total_invocations = 10,
        .positive_outcomes = 1,
        .negative_outcomes = 8,
        .decayed_score = 0.5,
    };
    double w = hu_evolving_compute_weight(&p);
    HU_ASSERT_TRUE(w < 1.0);
}

static void rebuild_profiles_aggregates_correctly(void) {
    setup();

    for (int i = 0; i < 5; i++) {
        hu_skill_invocation_t inv = {
            .skill_name = "meta-cognition",
            .skill_name_len = 14,
            .contact_id = "user_d",
            .contact_id_len = 6,
            .session_id = "sess_4",
            .session_id_len = 6,
            .explicit_run = true,
            .outcome = (i < 3) ? HU_SKILL_OUTCOME_POSITIVE : HU_SKILL_OUTCOME_NEGATIVE,
        };
        hu_evolving_record_invocation(db, &inv);
    }

    hu_error_t err = hu_evolving_rebuild_profiles(db);
    HU_ASSERT_EQ(err, HU_OK);

    hu_skill_profile_t *profiles = NULL;
    size_t count = 0;
    err = hu_evolving_load_profiles(db, &alloc, "user_d", 6, &profiles, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(count, (size_t)1);
    HU_ASSERT_EQ(profiles[0].total_invocations, 5u);
    HU_ASSERT_EQ(profiles[0].positive_outcomes, 3u);
    HU_ASSERT_EQ(profiles[0].negative_outcomes, 2u);

    hu_evolving_free_profiles(&alloc, profiles, count);
    teardown();
}

static void null_db_returns_error(void) {
    hu_error_t err = hu_evolving_init_schema(NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    err = hu_evolving_record_invocation(NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

#endif /* HU_ENABLE_SQLITE */

void run_evolving_cognition_tests(void) {
    HU_TEST_SUITE("EvolvingCognition");
#ifdef HU_ENABLE_SQLITE
    HU_RUN_TEST(schema_creation_succeeds);
    HU_RUN_TEST(record_explicit_invocation_roundtrip);
    HU_RUN_TEST(implicit_exposure_records_multiple);
    HU_RUN_TEST(outcome_collection_updates_recent);
    HU_RUN_TEST(compute_weight_neutral_when_no_data);
    HU_RUN_TEST(compute_weight_higher_with_positive);
    HU_RUN_TEST(compute_weight_lower_with_negative);
    HU_RUN_TEST(rebuild_profiles_aggregates_correctly);
    HU_RUN_TEST(null_db_returns_error);
#endif
}
