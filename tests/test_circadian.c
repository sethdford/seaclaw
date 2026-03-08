#include "seaclaw/core/allocator.h"
#include "seaclaw/persona/circadian.h"
#include "test_framework.h"
#include <string.h>

static void circadian_5am_is_early_morning(void) {
    sc_time_phase_t p = sc_circadian_phase(5);
    SC_ASSERT_EQ(p, SC_PHASE_EARLY_MORNING);
}

static void circadian_10am_is_morning(void) {
    sc_time_phase_t p = sc_circadian_phase(10);
    SC_ASSERT_EQ(p, SC_PHASE_MORNING);
}

static void circadian_14pm_is_afternoon(void) {
    sc_time_phase_t p = sc_circadian_phase(14);
    SC_ASSERT_EQ(p, SC_PHASE_AFTERNOON);
}

static void circadian_19pm_is_evening(void) {
    sc_time_phase_t p = sc_circadian_phase(19);
    SC_ASSERT_EQ(p, SC_PHASE_EVENING);
}

static void circadian_22pm_is_night(void) {
    sc_time_phase_t p = sc_circadian_phase(22);
    SC_ASSERT_EQ(p, SC_PHASE_NIGHT);
}

static void circadian_2am_is_late_night(void) {
    sc_time_phase_t p = sc_circadian_phase(2);
    SC_ASSERT_EQ(p, SC_PHASE_LATE_NIGHT);
}

static void circadian_build_prompt_contains_phase(void) {
    sc_allocator_t alloc = sc_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    sc_error_t err = sc_circadian_build_prompt(&alloc, 10, &out, &out_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(out);
    SC_ASSERT_TRUE(strstr(out, "Time Awareness") != NULL);
    SC_ASSERT_TRUE(strstr(out, "morning") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

void run_circadian_tests(void) {
    SC_TEST_SUITE("circadian");
    SC_RUN_TEST(circadian_5am_is_early_morning);
    SC_RUN_TEST(circadian_10am_is_morning);
    SC_RUN_TEST(circadian_14pm_is_afternoon);
    SC_RUN_TEST(circadian_19pm_is_evening);
    SC_RUN_TEST(circadian_22pm_is_night);
    SC_RUN_TEST(circadian_2am_is_late_night);
    SC_RUN_TEST(circadian_build_prompt_contains_phase);
}
