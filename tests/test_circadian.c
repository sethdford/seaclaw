#include "human/core/allocator.h"
#include "human/persona.h"
#include "human/persona/circadian.h"
#include "test_framework.h"
#include <string.h>

static void circadian_5am_is_early_morning(void) {
    hu_time_phase_t p = hu_circadian_phase(5);
    HU_ASSERT_EQ(p, HU_PHASE_EARLY_MORNING);
}

static void circadian_10am_is_morning(void) {
    hu_time_phase_t p = hu_circadian_phase(10);
    HU_ASSERT_EQ(p, HU_PHASE_MORNING);
}

static void circadian_14pm_is_afternoon(void) {
    hu_time_phase_t p = hu_circadian_phase(14);
    HU_ASSERT_EQ(p, HU_PHASE_AFTERNOON);
}

static void circadian_19pm_is_evening(void) {
    hu_time_phase_t p = hu_circadian_phase(19);
    HU_ASSERT_EQ(p, HU_PHASE_EVENING);
}

static void circadian_22pm_is_night(void) {
    hu_time_phase_t p = hu_circadian_phase(22);
    HU_ASSERT_EQ(p, HU_PHASE_NIGHT);
}

static void circadian_2am_is_late_night(void) {
    hu_time_phase_t p = hu_circadian_phase(2);
    HU_ASSERT_EQ(p, HU_PHASE_LATE_NIGHT);
}

static void circadian_build_prompt_contains_phase(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_circadian_build_prompt(&alloc, 10, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Time Awareness") != NULL);
    HU_ASSERT_TRUE(strstr(out, "morning") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void circadian_boundary_hours(void) {
    HU_ASSERT_EQ(hu_circadian_phase(0), HU_PHASE_LATE_NIGHT);
    HU_ASSERT_EQ(hu_circadian_phase(5), HU_PHASE_EARLY_MORNING);
    HU_ASSERT_EQ(hu_circadian_phase(8), HU_PHASE_MORNING);
    HU_ASSERT_EQ(hu_circadian_phase(12), HU_PHASE_AFTERNOON);
    HU_ASSERT_EQ(hu_circadian_phase(17), HU_PHASE_EVENING);
    HU_ASSERT_EQ(hu_circadian_phase(21), HU_PHASE_NIGHT);
}

/* --- Persona-aware circadian tests --- */

static hu_persona_t make_test_persona_with_overlays(void) {
    hu_persona_t p = {0};
    p.time_overlay_late_night = (char *)"Be vulnerable and contemplative";
    p.time_overlay_early_morning = (char *)"Be gentle and slow to start";
    p.time_overlay_afternoon = (char *)"Be focused and productive";
    p.time_overlay_evening = (char *)"Be warm and reflective";
    return p;
}

static void circadian_persona_overlay_late_night(void) {
    hu_persona_t p = make_test_persona_with_overlays();
    const char *overlay =
        hu_circadian_persona_overlay((const struct hu_persona *)&p, HU_PHASE_LATE_NIGHT);
    HU_ASSERT_NOT_NULL(overlay);
    HU_ASSERT_TRUE(strstr(overlay, "vulnerable") != NULL);
}

static void circadian_persona_overlay_early_morning(void) {
    hu_persona_t p = make_test_persona_with_overlays();
    const char *overlay =
        hu_circadian_persona_overlay((const struct hu_persona *)&p, HU_PHASE_EARLY_MORNING);
    HU_ASSERT_NOT_NULL(overlay);
    HU_ASSERT_TRUE(strstr(overlay, "gentle") != NULL);
}

static void circadian_persona_overlay_morning_returns_null(void) {
    hu_persona_t p = make_test_persona_with_overlays();
    const char *overlay =
        hu_circadian_persona_overlay((const struct hu_persona *)&p, HU_PHASE_MORNING);
    HU_ASSERT_NULL(overlay);
}

static void circadian_persona_overlay_evening(void) {
    hu_persona_t p = make_test_persona_with_overlays();
    const char *overlay =
        hu_circadian_persona_overlay((const struct hu_persona *)&p, HU_PHASE_EVENING);
    HU_ASSERT_NOT_NULL(overlay);
    HU_ASSERT_TRUE(strstr(overlay, "warm") != NULL);
}

static void circadian_persona_overlay_null_persona(void) {
    const char *overlay = hu_circadian_persona_overlay(NULL, HU_PHASE_LATE_NIGHT);
    HU_ASSERT_NULL(overlay);
}

static void circadian_persona_prompt_includes_overlay(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = make_test_persona_with_overlays();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_circadian_build_persona_prompt(&alloc, 2, (const struct hu_persona *)&p, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Persona guidance") != NULL);
    HU_ASSERT_TRUE(strstr(out, "vulnerable") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void circadian_persona_prompt_with_routine(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_persona_t p = make_test_persona_with_overlays();
    /* Set up a routine block at 2am */
    hu_routine_block_t block = {0};
    snprintf(block.time, sizeof(block.time), "02:00");
    snprintf(block.activity, sizeof(block.activity), "sleeping");
    snprintf(block.mood_modifier, sizeof(block.mood_modifier), "drowsy");
    p.daily_routine.weekday[0] = block;
    p.daily_routine.weekday_count = 1;

    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_circadian_build_persona_prompt(&alloc, 2, (const struct hu_persona *)&p, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "drowsy") != NULL);
    HU_ASSERT_TRUE(strstr(out, "Persona guidance") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void circadian_persona_prompt_null_persona_falls_back(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_circadian_build_persona_prompt(&alloc, 10, NULL, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(strstr(out, "Time Awareness") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

void run_circadian_tests(void) {
    HU_TEST_SUITE("circadian");
    HU_RUN_TEST(circadian_5am_is_early_morning);
    HU_RUN_TEST(circadian_10am_is_morning);
    HU_RUN_TEST(circadian_14pm_is_afternoon);
    HU_RUN_TEST(circadian_19pm_is_evening);
    HU_RUN_TEST(circadian_22pm_is_night);
    HU_RUN_TEST(circadian_2am_is_late_night);
    HU_RUN_TEST(circadian_build_prompt_contains_phase);
    HU_RUN_TEST(circadian_boundary_hours);
    /* Persona-aware circadian */
    HU_RUN_TEST(circadian_persona_overlay_late_night);
    HU_RUN_TEST(circadian_persona_overlay_early_morning);
    HU_RUN_TEST(circadian_persona_overlay_morning_returns_null);
    HU_RUN_TEST(circadian_persona_overlay_evening);
    HU_RUN_TEST(circadian_persona_overlay_null_persona);
    HU_RUN_TEST(circadian_persona_prompt_includes_overlay);
    HU_RUN_TEST(circadian_persona_prompt_with_routine);
    HU_RUN_TEST(circadian_persona_prompt_null_persona_falls_back);
}
