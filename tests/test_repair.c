#include "human/context/repair.h"
#include "test_framework.h"
#include <string.h>

/* ── hu_repair_detect: correction patterns ────────────────────────── */

static void test_repair_detect_correction_explicit(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect("No, that's not right", 20, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CORRECTION);
    HU_ASSERT_TRUE(sig.confidence >= 0.6f);
    HU_ASSERT_TRUE(sig.should_acknowledge);
    HU_ASSERT_TRUE(strstr(sig.directive, "correcting") != NULL);
}

static void test_repair_detect_correction_bare_no(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect("No", 2, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CORRECTION);
}

static void test_repair_detect_correction_wrong(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect("That's wrong, I said something else", 35, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CORRECTION);
}

static void test_repair_detect_correction_incorrect(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect("Incorrect.", 10, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CORRECTION);
}

/* ── hu_repair_detect: redirect patterns ──────────────────────────── */

static void test_repair_detect_redirect_asking_about(void) {
    hu_repair_signal_t sig;
    const char *msg = "I was asking about the budget, not the schedule";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_REDIRECT);
    HU_ASSERT_TRUE(strstr(sig.directive, "off-topic") != NULL);
}

static void test_repair_detect_redirect_i_meant(void) {
    hu_repair_signal_t sig;
    const char *msg = "I meant the other feature";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_REDIRECT);
}

static void test_repair_detect_redirect_tangent(void) {
    hu_repair_signal_t sig;
    const char *msg = "You're going off on a tangent here";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_REDIRECT);
}

/* ── hu_repair_detect: confusion patterns ─────────────────────────── */

static void test_repair_detect_confusion_what(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect("What?", 5, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CONFUSION);
    HU_ASSERT_TRUE(!sig.should_acknowledge);
}

static void test_repair_detect_confusion_no_sense(void) {
    hu_repair_signal_t sig;
    const char *msg = "That doesn't make sense to me";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CONFUSION);
}

static void test_repair_detect_confusion_huh(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect("Huh?", 4, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CONFUSION);
}

/* ── hu_repair_detect: memory error patterns ──────────────────────── */

static void test_repair_detect_memory_error_confusing(void) {
    hu_repair_signal_t sig;
    const char *msg = "You're confusing me with someone else";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_MEMORY_ERROR);
    HU_ASSERT_TRUE(strstr(sig.directive, "personal detail") != NULL);
}

static void test_repair_detect_memory_error_never_told(void) {
    hu_repair_signal_t sig;
    const char *msg = "I never told you that";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_MEMORY_ERROR);
}

static void test_repair_detect_memory_error_wrong_person(void) {
    hu_repair_signal_t sig;
    const char *msg = "Wrong person, I don't have a dog";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_MEMORY_ERROR);
}

/* ── hu_repair_detect: no repair needed ───────────────────────────── */

static void test_repair_detect_none_for_normal_message(void) {
    hu_repair_signal_t sig;
    const char *msg = "Tell me more about quantum computing";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_NONE);
}

static void test_repair_detect_none_for_empty(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect("", 0, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_NONE);
}

static void test_repair_detect_none_for_null(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_detect(NULL, 0, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_NONE);
}

static void test_repair_detect_null_out_returns_error(void) {
    hu_error_t err = hu_repair_detect("hello", 5, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* ── hu_repair_detect: confidence scaling ─────────────────────────── */

static void test_repair_detect_higher_confidence_multiple_matches(void) {
    hu_repair_signal_t sig;
    const char *msg = "No, that's wrong. That's not right at all. Incorrect.";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CORRECTION);
    HU_ASSERT_TRUE(sig.confidence >= 0.9f);
}

/* ── hu_repair_from_metacognition ─────────────────────────────────── */

static void test_repair_metacog_not_degrading(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_from_metacognition(false, 0.8f, 0, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_NONE);
}

static void test_repair_metacog_single_degrading_suppressed(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_from_metacognition(true, 0.3f, 1, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_NONE);
}

static void test_repair_metacog_consecutive_degrading_triggers(void) {
    hu_repair_signal_t sig;
    hu_error_t err = hu_repair_from_metacognition(true, 0.3f, 2, &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_SELF_DETECTED);
    HU_ASSERT_TRUE(sig.confidence > 0.5f);
    HU_ASSERT_TRUE(sig.should_acknowledge);
    HU_ASSERT_TRUE(strstr(sig.directive, "understanding you correctly") != NULL);
}

static void test_repair_metacog_low_coherence_higher_confidence(void) {
    hu_repair_signal_t sig1, sig2;
    hu_repair_from_metacognition(true, 0.8f, 2, &sig1);
    hu_repair_from_metacognition(true, 0.1f, 2, &sig2);
    HU_ASSERT_TRUE(sig2.confidence > sig1.confidence);
}

static void test_repair_metacog_three_consecutive_boosts_confidence(void) {
    hu_repair_signal_t sig2, sig3;
    hu_repair_from_metacognition(true, 0.3f, 2, &sig2);
    hu_repair_from_metacognition(true, 0.3f, 3, &sig3);
    HU_ASSERT_TRUE(sig3.confidence > sig2.confidence);
}

static void test_repair_metacog_null_out_returns_error(void) {
    hu_error_t err = hu_repair_from_metacognition(true, 0.3f, 2, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

/* ── hu_repair_type_name ──────────────────────────────────────────── */

static void test_repair_type_name_covers_all(void) {
    HU_ASSERT_STR_EQ(hu_repair_type_name(HU_REPAIR_NONE), "none");
    HU_ASSERT_STR_EQ(hu_repair_type_name(HU_REPAIR_USER_CORRECTION), "user_correction");
    HU_ASSERT_STR_EQ(hu_repair_type_name(HU_REPAIR_USER_REDIRECT), "user_redirect");
    HU_ASSERT_STR_EQ(hu_repair_type_name(HU_REPAIR_USER_CONFUSION), "user_confusion");
    HU_ASSERT_STR_EQ(hu_repair_type_name(HU_REPAIR_MEMORY_ERROR), "memory_error");
    HU_ASSERT_STR_EQ(hu_repair_type_name(HU_REPAIR_SELF_DETECTED), "self_detected");
}

/* ── case insensitivity ───────────────────────────────────────────── */

static void test_repair_detect_case_insensitive(void) {
    hu_repair_signal_t sig;
    const char *msg = "THAT'S WRONG!";
    hu_error_t err = hu_repair_detect(msg, strlen(msg), &sig);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(sig.type, HU_REPAIR_USER_CORRECTION);
}

/* ── suite registration ───────────────────────────────────────────── */

void run_repair_tests(void) {
    HU_TEST_SUITE("repair");

    /* correction */
    HU_RUN_TEST(test_repair_detect_correction_explicit);
    HU_RUN_TEST(test_repair_detect_correction_bare_no);
    HU_RUN_TEST(test_repair_detect_correction_wrong);
    HU_RUN_TEST(test_repair_detect_correction_incorrect);

    /* redirect */
    HU_RUN_TEST(test_repair_detect_redirect_asking_about);
    HU_RUN_TEST(test_repair_detect_redirect_i_meant);
    HU_RUN_TEST(test_repair_detect_redirect_tangent);

    /* confusion */
    HU_RUN_TEST(test_repair_detect_confusion_what);
    HU_RUN_TEST(test_repair_detect_confusion_no_sense);
    HU_RUN_TEST(test_repair_detect_confusion_huh);

    /* memory error */
    HU_RUN_TEST(test_repair_detect_memory_error_confusing);
    HU_RUN_TEST(test_repair_detect_memory_error_never_told);
    HU_RUN_TEST(test_repair_detect_memory_error_wrong_person);

    /* no repair */
    HU_RUN_TEST(test_repair_detect_none_for_normal_message);
    HU_RUN_TEST(test_repair_detect_none_for_empty);
    HU_RUN_TEST(test_repair_detect_none_for_null);
    HU_RUN_TEST(test_repair_detect_null_out_returns_error);

    /* confidence */
    HU_RUN_TEST(test_repair_detect_higher_confidence_multiple_matches);

    /* metacognition integration */
    HU_RUN_TEST(test_repair_metacog_not_degrading);
    HU_RUN_TEST(test_repair_metacog_single_degrading_suppressed);
    HU_RUN_TEST(test_repair_metacog_consecutive_degrading_triggers);
    HU_RUN_TEST(test_repair_metacog_low_coherence_higher_confidence);
    HU_RUN_TEST(test_repair_metacog_three_consecutive_boosts_confidence);
    HU_RUN_TEST(test_repair_metacog_null_out_returns_error);

    /* type name */
    HU_RUN_TEST(test_repair_type_name_covers_all);

    /* case insensitivity */
    HU_RUN_TEST(test_repair_detect_case_insensitive);
}
