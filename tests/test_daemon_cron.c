#include "human/daemon.h"
#include "human/daemon_cron.h"
#include "test_framework.h"
#include <string.h>
#include <time.h>

/* ── hu_cron_atom_matches ────────────────────────────────────────────── */

static void test_atom_matches_exact_value(void) {
    HU_ASSERT_TRUE(hu_cron_atom_matches("5", 1, 5));
    HU_ASSERT_FALSE(hu_cron_atom_matches("5", 1, 6));
}

static void test_atom_matches_star(void) {
    HU_ASSERT_TRUE(hu_cron_atom_matches("*", 1, 0));
    HU_ASSERT_TRUE(hu_cron_atom_matches("*", 1, 59));
}

static void test_atom_matches_step(void) {
    HU_ASSERT_TRUE(hu_cron_atom_matches("*/5", 3, 0));
    HU_ASSERT_TRUE(hu_cron_atom_matches("*/5", 3, 10));
    HU_ASSERT_FALSE(hu_cron_atom_matches("*/5", 3, 3));
}

static void test_atom_matches_range(void) {
    HU_ASSERT_TRUE(hu_cron_atom_matches("1-5", 3, 1));
    HU_ASSERT_TRUE(hu_cron_atom_matches("1-5", 3, 3));
    HU_ASSERT_TRUE(hu_cron_atom_matches("1-5", 3, 5));
    HU_ASSERT_FALSE(hu_cron_atom_matches("1-5", 3, 0));
    HU_ASSERT_FALSE(hu_cron_atom_matches("1-5", 3, 6));
}

static void test_atom_matches_range_step(void) {
    HU_ASSERT_TRUE(hu_cron_atom_matches("1-10/3", 6, 1));
    HU_ASSERT_TRUE(hu_cron_atom_matches("1-10/3", 6, 4));
    HU_ASSERT_TRUE(hu_cron_atom_matches("1-10/3", 6, 7));
    HU_ASSERT_TRUE(hu_cron_atom_matches("1-10/3", 6, 10));
    HU_ASSERT_FALSE(hu_cron_atom_matches("1-10/3", 6, 2));
}

static void test_atom_matches_zero_len(void) {
    HU_ASSERT_FALSE(hu_cron_atom_matches("5", 0, 5));
}

static void test_atom_matches_invalid_step(void) {
    HU_ASSERT_FALSE(hu_cron_atom_matches("*/0", 3, 0));
    HU_ASSERT_FALSE(hu_cron_atom_matches("*/-1", 4, 0));
}

static void test_atom_matches_overflow_len(void) {
    /* Atom longer than 32 chars should fail */
    char big[40];
    memset(big, '1', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    HU_ASSERT_FALSE(hu_cron_atom_matches(big, sizeof(big) - 1, 1));
}

/* ── hu_cron_field_matches ───────────────────────────────────────────── */

static void test_field_matches_star(void) {
    HU_ASSERT_TRUE(hu_cron_field_matches("*", 42));
}

static void test_field_matches_exact(void) {
    HU_ASSERT_TRUE(hu_cron_field_matches("30", 30));
    HU_ASSERT_FALSE(hu_cron_field_matches("30", 15));
}

static void test_field_matches_comma_list(void) {
    HU_ASSERT_TRUE(hu_cron_field_matches("0,15,30,45", 15));
    HU_ASSERT_TRUE(hu_cron_field_matches("0,15,30,45", 45));
    HU_ASSERT_FALSE(hu_cron_field_matches("0,15,30,45", 10));
}

static void test_field_matches_null(void) {
    HU_ASSERT_FALSE(hu_cron_field_matches(NULL, 5));
}

/* ── hu_cron_schedule_matches ────────────────────────────────────────── */

static void test_schedule_matches_all_star(void) {
    struct tm t = {.tm_min = 30, .tm_hour = 14, .tm_mday = 15, .tm_mon = 5, .tm_wday = 3};
    HU_ASSERT_TRUE(hu_cron_schedule_matches("* * * * *", &t));
}

static void test_schedule_matches_exact_time(void) {
    struct tm t = {.tm_min = 30, .tm_hour = 14, .tm_mday = 15, .tm_mon = 5, .tm_wday = 3};
    HU_ASSERT_TRUE(hu_cron_schedule_matches("30 14 15 6 3", &t));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("0 9 * * *", &t));
}

static void test_schedule_matches_step_minutes(void) {
    struct tm t0 = {.tm_min = 0, .tm_hour = 10, .tm_mday = 1, .tm_mon = 0, .tm_wday = 1};
    struct tm t5 = {.tm_min = 5, .tm_hour = 10, .tm_mday = 1, .tm_mon = 0, .tm_wday = 1};
    struct tm t3 = {.tm_min = 3, .tm_hour = 10, .tm_mday = 1, .tm_mon = 0, .tm_wday = 1};
    HU_ASSERT_TRUE(hu_cron_schedule_matches("*/5 * * * *", &t0));
    HU_ASSERT_TRUE(hu_cron_schedule_matches("*/5 * * * *", &t5));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("*/5 * * * *", &t3));
}

static void test_schedule_matches_weekday_range(void) {
    struct tm mon = {.tm_min = 0, .tm_hour = 9, .tm_mday = 1, .tm_mon = 0, .tm_wday = 1};
    struct tm sat = {.tm_min = 0, .tm_hour = 9, .tm_mday = 1, .tm_mon = 0, .tm_wday = 6};
    HU_ASSERT_TRUE(hu_cron_schedule_matches("0 9 * * 1-5", &mon));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("0 9 * * 1-5", &sat));
}

static void test_schedule_matches_null_input(void) {
    struct tm t = {0};
    HU_ASSERT_FALSE(hu_cron_schedule_matches(NULL, &t));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("* * * * *", NULL));
    HU_ASSERT_FALSE(hu_cron_schedule_matches(NULL, NULL));
}

static void test_schedule_matches_too_few_fields(void) {
    struct tm t = {0};
    HU_ASSERT_FALSE(hu_cron_schedule_matches("* * *", &t));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("*", &t));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("", &t));
}

static void test_schedule_matches_specific_date(void) {
    struct tm t = {.tm_min = 0, .tm_hour = 0, .tm_mday = 25, .tm_mon = 11, .tm_wday = 3};
    HU_ASSERT_TRUE(hu_cron_schedule_matches("0 0 25 12 *", &t));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("0 0 24 12 *", &t));
}

static void test_schedule_matches_comma_minutes(void) {
    struct tm t5 = {.tm_min = 5, .tm_hour = 12, .tm_mday = 1, .tm_mon = 0, .tm_wday = 0};
    struct tm t10 = {.tm_min = 10, .tm_hour = 12, .tm_mday = 1, .tm_mon = 0, .tm_wday = 0};
    struct tm t7 = {.tm_min = 7, .tm_hour = 12, .tm_mday = 1, .tm_mon = 0, .tm_wday = 0};
    HU_ASSERT_TRUE(hu_cron_schedule_matches("5,10 * * * *", &t5));
    HU_ASSERT_TRUE(hu_cron_schedule_matches("5,10 * * * *", &t10));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("5,10 * * * *", &t7));
}

static void test_schedule_matches_range_step_combined(void) {
    /* Business hours: every 5 min, 9-17, Mon-Fri */
    struct tm match = {.tm_min = 10, .tm_hour = 12, .tm_mday = 1, .tm_mon = 0, .tm_wday = 3};
    struct tm no_min = {.tm_min = 3, .tm_hour = 12, .tm_mday = 1, .tm_mon = 0, .tm_wday = 3};
    struct tm no_dow = {.tm_min = 10, .tm_hour = 12, .tm_mday = 1, .tm_mon = 0, .tm_wday = 6};
    HU_ASSERT_TRUE(hu_cron_schedule_matches("*/5 9-17 * * 1-5", &match));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("*/5 9-17 * * 1-5", &no_min));
    HU_ASSERT_FALSE(hu_cron_schedule_matches("*/5 9-17 * * 1-5", &no_dow));
}

/* ── hu_daemon_cron_tick ─────────────────────────────────────────────── */

static void test_cron_tick_null_alloc(void) {
    /* Should not crash with NULL allocator */
    hu_daemon_cron_tick(NULL);
}

static void test_cron_tick_runs_without_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    /* In HU_IS_TEST mode this is a no-op since run_cron_tick skips execution */
    hu_daemon_cron_tick(&alloc);
}

void run_daemon_cron_tests(void) {
    HU_TEST_SUITE("daemon_cron");

    /* atom matching */
    HU_RUN_TEST(test_atom_matches_exact_value);
    HU_RUN_TEST(test_atom_matches_star);
    HU_RUN_TEST(test_atom_matches_step);
    HU_RUN_TEST(test_atom_matches_range);
    HU_RUN_TEST(test_atom_matches_range_step);
    HU_RUN_TEST(test_atom_matches_zero_len);
    HU_RUN_TEST(test_atom_matches_invalid_step);
    HU_RUN_TEST(test_atom_matches_overflow_len);

    /* field matching */
    HU_RUN_TEST(test_field_matches_star);
    HU_RUN_TEST(test_field_matches_exact);
    HU_RUN_TEST(test_field_matches_comma_list);
    HU_RUN_TEST(test_field_matches_null);

    /* schedule matching */
    HU_RUN_TEST(test_schedule_matches_all_star);
    HU_RUN_TEST(test_schedule_matches_exact_time);
    HU_RUN_TEST(test_schedule_matches_step_minutes);
    HU_RUN_TEST(test_schedule_matches_weekday_range);
    HU_RUN_TEST(test_schedule_matches_null_input);
    HU_RUN_TEST(test_schedule_matches_too_few_fields);
    HU_RUN_TEST(test_schedule_matches_specific_date);
    HU_RUN_TEST(test_schedule_matches_comma_minutes);
    HU_RUN_TEST(test_schedule_matches_range_step_combined);

    /* cron tick */
    HU_RUN_TEST(test_cron_tick_null_alloc);
    HU_RUN_TEST(test_cron_tick_runs_without_crash);
}
