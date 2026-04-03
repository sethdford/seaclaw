#include "human/core/allocator.h"
#include "human/persona/temporal.h"
#include "test_framework.h"
#include <string.h>

/* --- Season --- */

static void test_temporal_season_winter_december(void) {
    HU_ASSERT_EQ(hu_temporal_season(12), HU_SEASON_WINTER);
}

static void test_temporal_season_winter_january(void) {
    HU_ASSERT_EQ(hu_temporal_season(1), HU_SEASON_WINTER);
}

static void test_temporal_season_winter_february(void) {
    HU_ASSERT_EQ(hu_temporal_season(2), HU_SEASON_WINTER);
}

static void test_temporal_season_spring_march(void) {
    HU_ASSERT_EQ(hu_temporal_season(3), HU_SEASON_SPRING);
}

static void test_temporal_season_spring_may(void) {
    HU_ASSERT_EQ(hu_temporal_season(5), HU_SEASON_SPRING);
}

static void test_temporal_season_summer_june(void) {
    HU_ASSERT_EQ(hu_temporal_season(6), HU_SEASON_SUMMER);
}

static void test_temporal_season_summer_july(void) {
    HU_ASSERT_EQ(hu_temporal_season(7), HU_SEASON_SUMMER);
}

static void test_temporal_season_summer_august(void) {
    HU_ASSERT_EQ(hu_temporal_season(8), HU_SEASON_SUMMER);
}

static void test_temporal_season_autumn_september(void) {
    HU_ASSERT_EQ(hu_temporal_season(9), HU_SEASON_AUTUMN);
}

static void test_temporal_season_autumn_november(void) {
    HU_ASSERT_EQ(hu_temporal_season(11), HU_SEASON_AUTUMN);
}

static void test_temporal_season_name_spring(void) {
    HU_ASSERT_STR_EQ(hu_temporal_season_name(HU_SEASON_SPRING), "spring");
}

static void test_temporal_season_name_winter(void) {
    HU_ASSERT_STR_EQ(hu_temporal_season_name(HU_SEASON_WINTER), "winter");
}

static void test_temporal_season_invalid_month(void) {
    /* Invalid months default to spring */
    HU_ASSERT_EQ(hu_temporal_season(0), HU_SEASON_SPRING);
    HU_ASSERT_EQ(hu_temporal_season(13), HU_SEASON_SPRING);
}

/* --- Anniversaries --- */

static void test_temporal_anniversary_birthday_today(void) {
    hu_date_entry_t dates[] = {{.label = "birthday", .label_len = 8, .month = 4, .day = 3}};
    hu_anniversary_t out[4];
    size_t count = hu_temporal_check_anniversaries(dates, 1, 2026, 4, 3, 3, out, 4);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(out[0].days_away, 0);
    HU_ASSERT_STR_EQ(out[0].label, "birthday");
}

static void test_temporal_anniversary_within_3_days(void) {
    hu_date_entry_t dates[] = {{.label = "birthday", .label_len = 8, .month = 4, .day = 5}};
    hu_anniversary_t out[4];
    size_t count = hu_temporal_check_anniversaries(dates, 1, 2026, 4, 3, 3, out, 4);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(out[0].days_away, 2);
}

static void test_temporal_anniversary_outside_window(void) {
    hu_date_entry_t dates[] = {{.label = "birthday", .label_len = 8, .month = 5, .day = 15}};
    hu_anniversary_t out[4];
    size_t count = hu_temporal_check_anniversaries(dates, 1, 2026, 4, 3, 3, out, 4);
    HU_ASSERT_EQ(count, 0);
}

static void test_temporal_anniversary_year_wrap(void) {
    /* Dec 30, checking for Jan 1 birthday — 2 days away */
    hu_date_entry_t dates[] = {{.label = "new year", .label_len = 8, .month = 1, .day = 1}};
    hu_anniversary_t out[4];
    size_t count = hu_temporal_check_anniversaries(dates, 1, 2026, 12, 30, 3, out, 4);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(out[0].days_away, 2);
}

static void test_temporal_anniversary_multiple(void) {
    hu_date_entry_t dates[] = {
        {.label = "birthday", .label_len = 8, .month = 4, .day = 3},
        {.label = "work anniversary", .label_len = 16, .month = 4, .day = 4},
        {.label = "wedding", .label_len = 7, .month = 8, .day = 15},
    };
    hu_anniversary_t out[4];
    size_t count = hu_temporal_check_anniversaries(dates, 3, 2026, 4, 3, 3, out, 4);
    HU_ASSERT_EQ(count, 2); /* birthday today + work anniversary tomorrow */
}

static void test_temporal_anniversary_null_input(void) {
    hu_anniversary_t out[4];
    HU_ASSERT_EQ(hu_temporal_check_anniversaries(NULL, 0, 2026, 4, 3, 3, out, 4), 0);
}

static void test_temporal_leap_year_feb29_anniversary(void) {
    /* Leap year 2024: Feb 29 birthday should match on Feb 27 within 3-day window */
    hu_date_entry_t dates[] = {{.label = "birthday", .label_len = 8, .month = 2, .day = 29}};
    hu_anniversary_t out[4];
    size_t count = hu_temporal_check_anniversaries(dates, 1, 2024, 2, 27, 3, out, 4);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(out[0].days_away, 2);
}

static void test_temporal_nonleap_feb29_fallback_to_feb28(void) {
    /* Non-leap year 2025: Feb 29 anniversary treated as Feb 28 */
    hu_date_entry_t dates[] = {{.label = "birthday", .label_len = 8, .month = 2, .day = 29}};
    hu_anniversary_t out[4];
    /* Check from Feb 26 with 3-day window — Feb 28 is 2 days away */
    size_t count = hu_temporal_check_anniversaries(dates, 1, 2025, 2, 26, 3, out, 4);
    HU_ASSERT_EQ(count, 1);
    HU_ASSERT_EQ(out[0].days_away, 2);
}

static void test_temporal_day_of_year_month_bounds(void) {
    /* Month < 1 should not crash — anniversaries with invalid months are skipped */
    hu_date_entry_t dates[] = {{.label = "bad", .label_len = 3, .month = 0, .day = 5}};
    hu_anniversary_t out[4];
    size_t count = hu_temporal_check_anniversaries(dates, 1, 2026, 4, 3, 3, out, 4);
    HU_ASSERT_EQ(count, 0);
}

/* --- Life transition detection --- */

static void test_temporal_detect_transition_job_change(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "I got a new job at a tech company", .text_len = 32},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_JOB_CHANGE);
}

static void test_temporal_detect_transition_move(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "We are moving to Seattle next month", .text_len = 35},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_MOVE);
}

static void test_temporal_detect_transition_breakup(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "We broke up last week", .text_len = 21},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_BREAKUP);
}

static void test_temporal_detect_transition_new_baby(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "We are expecting a baby in June", .text_len = 31},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_NEW_BABY);
}

static void test_temporal_detect_transition_graduation(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "I am graduating next month", .text_len = 26},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_GRADUATION);
}

static void test_temporal_detect_transition_retirement(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "I am finally retiring after 30 years", .text_len = 36},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_RETIREMENT);
}

static void test_temporal_detect_transition_loss(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "My grandmother passed away yesterday", .text_len = 36},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_LOSS);
}

static void test_temporal_detect_transition_none(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "Had a great lunch today", .text_len = 23},
        {.text = "The weather is nice", .text_len = 19},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 2), HU_TRANSITION_NONE);
}

static void test_temporal_detect_transition_null_messages(void) {
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(NULL, 0), HU_TRANSITION_NONE);
}

static void test_temporal_detect_transition_case_insensitive(void) {
    hu_temporal_message_t msgs[] = {
        {.text = "I GOT A NEW JOB at Google", .text_len = 25},
    };
    HU_ASSERT_EQ(hu_temporal_detect_life_transition(msgs, 1), HU_TRANSITION_JOB_CHANGE);
}

/* --- Build context --- */

static void test_temporal_build_context_basic(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_temporal_build_context(&alloc, 7, 15, NULL, 0, HU_TRANSITION_NONE, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_CONTAINS(out, "summer");
    HU_ASSERT_STR_CONTAINS(out, "month 7");
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_temporal_build_context_with_anniversary(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_anniversary_t anns[] = {
        {.label = "birthday", .label_len = 8, .month = 4, .day = 5, .days_away = 2},
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_temporal_build_context(&alloc, 4, 3, anns, 1, HU_TRANSITION_NONE, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_CONTAINS(out, "birthday");
    HU_ASSERT_STR_CONTAINS(out, "2 days");
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_temporal_build_context_anniversary_today(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_anniversary_t anns[] = {
        {.label = "birthday", .label_len = 8, .month = 4, .day = 3, .days_away = 0},
    };
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_temporal_build_context(&alloc, 4, 3, anns, 1, HU_TRANSITION_NONE, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_CONTAINS(out, "TODAY");
    HU_ASSERT_STR_CONTAINS(out, "birthday");
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_temporal_build_context_with_transition(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_temporal_build_context(&alloc, 12, 1, NULL, 0, HU_TRANSITION_BREAKUP, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_CONTAINS(out, "winter");
    HU_ASSERT_STR_CONTAINS(out, "relationship change");
    HU_ASSERT_STR_CONTAINS(out, "sensitive");
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void test_temporal_build_context_null_returns_error(void) {
    HU_ASSERT_EQ(hu_temporal_build_context(NULL, 4, 3, NULL, 0, HU_TRANSITION_NONE, NULL, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

void run_temporal_tests(void) {
    HU_TEST_SUITE("temporal");
    /* Season */
    HU_RUN_TEST(test_temporal_season_winter_december);
    HU_RUN_TEST(test_temporal_season_winter_january);
    HU_RUN_TEST(test_temporal_season_winter_february);
    HU_RUN_TEST(test_temporal_season_spring_march);
    HU_RUN_TEST(test_temporal_season_spring_may);
    HU_RUN_TEST(test_temporal_season_summer_june);
    HU_RUN_TEST(test_temporal_season_summer_july);
    HU_RUN_TEST(test_temporal_season_summer_august);
    HU_RUN_TEST(test_temporal_season_autumn_september);
    HU_RUN_TEST(test_temporal_season_autumn_november);
    HU_RUN_TEST(test_temporal_season_name_spring);
    HU_RUN_TEST(test_temporal_season_name_winter);
    HU_RUN_TEST(test_temporal_season_invalid_month);
    /* Anniversaries */
    HU_RUN_TEST(test_temporal_anniversary_birthday_today);
    HU_RUN_TEST(test_temporal_anniversary_within_3_days);
    HU_RUN_TEST(test_temporal_anniversary_outside_window);
    HU_RUN_TEST(test_temporal_anniversary_year_wrap);
    HU_RUN_TEST(test_temporal_anniversary_multiple);
    HU_RUN_TEST(test_temporal_anniversary_null_input);
    HU_RUN_TEST(test_temporal_leap_year_feb29_anniversary);
    HU_RUN_TEST(test_temporal_nonleap_feb29_fallback_to_feb28);
    HU_RUN_TEST(test_temporal_day_of_year_month_bounds);
    /* Life transitions */
    HU_RUN_TEST(test_temporal_detect_transition_job_change);
    HU_RUN_TEST(test_temporal_detect_transition_move);
    HU_RUN_TEST(test_temporal_detect_transition_breakup);
    HU_RUN_TEST(test_temporal_detect_transition_new_baby);
    HU_RUN_TEST(test_temporal_detect_transition_graduation);
    HU_RUN_TEST(test_temporal_detect_transition_retirement);
    HU_RUN_TEST(test_temporal_detect_transition_loss);
    HU_RUN_TEST(test_temporal_detect_transition_none);
    HU_RUN_TEST(test_temporal_detect_transition_null_messages);
    HU_RUN_TEST(test_temporal_detect_transition_case_insensitive);
    /* Build context */
    HU_RUN_TEST(test_temporal_build_context_basic);
    HU_RUN_TEST(test_temporal_build_context_with_anniversary);
    HU_RUN_TEST(test_temporal_build_context_anniversary_today);
    HU_RUN_TEST(test_temporal_build_context_with_transition);
    HU_RUN_TEST(test_temporal_build_context_null_returns_error);
}
