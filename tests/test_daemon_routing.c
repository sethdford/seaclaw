#include "human/daemon.h"
#include "human/daemon_routing.h"
#include "human/channel_loop.h"
#include "test_framework.h"
#include <string.h>

/* ── hu_daemon_is_tapback_worthy ─────────────────────────────────────── */

static void test_tapback_null_returns_false(void) {
    HU_ASSERT_FALSE(hu_daemon_is_tapback_worthy(NULL, 0));
    HU_ASSERT_FALSE(hu_daemon_is_tapback_worthy("hi", 0));
}

static void test_tapback_question_not_worthy(void) {
    HU_ASSERT_FALSE(hu_daemon_is_tapback_worthy("how are you?", 12));
}

static void test_tapback_short_ack(void) {
    HU_ASSERT_TRUE(hu_daemon_is_tapback_worthy("ok", 2));
    HU_ASSERT_TRUE(hu_daemon_is_tapback_worthy("lol", 3));
    HU_ASSERT_TRUE(hu_daemon_is_tapback_worthy("k", 1));
}

static void test_tapback_single_token_under_12(void) {
    HU_ASSERT_TRUE(hu_daemon_is_tapback_worthy("hahahahaha", 10));
}

static void test_tapback_multi_word_short_not_worthy(void) {
    HU_ASSERT_FALSE(hu_daemon_is_tapback_worthy("oh nice one there", 17));
}

static void test_tapback_long_message_not_worthy(void) {
    HU_ASSERT_FALSE(hu_daemon_is_tapback_worthy("that sounds like a great plan", 29));
}

/* ── hu_daemon_compute_photo_delay ───────────────────────────────────── */

static void test_photo_delay_no_attachment(void) {
    hu_channel_loop_msg_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    uint32_t delay = hu_daemon_compute_photo_delay(msgs, 0, 1, 123);
    HU_ASSERT_EQ(delay, 0);
}

static void test_photo_delay_with_attachment(void) {
    hu_channel_loop_msg_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[1].has_attachment = true;
    uint32_t delay = hu_daemon_compute_photo_delay(msgs, 0, 1, 0);
    HU_ASSERT(delay >= 3000 && delay <= 8000);
}

/* ── hu_daemon_compute_video_delay ───────────────────────────────────── */

static void test_video_delay_no_video(void) {
    hu_channel_loop_msg_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    uint32_t delay = hu_daemon_compute_video_delay(msgs, 0, 1, 123);
    HU_ASSERT_EQ(delay, 0);
}

static void test_video_delay_with_video(void) {
    hu_channel_loop_msg_t msgs[2];
    memset(msgs, 0, sizeof(msgs));
    msgs[0].has_video = true;
    uint32_t delay = hu_daemon_compute_video_delay(msgs, 0, 1, 0);
    HU_ASSERT(delay >= 2000 && delay <= 10000);
}

/* ── hu_missed_message_acknowledgment ────────────────────────────────── */

static void test_missed_msg_short_delay_null(void) {
    const char *phrase = hu_missed_message_acknowledgment(60, 14, 14, 0);
    HU_ASSERT_NULL(phrase);
}

static void test_missed_msg_long_delay_returns_phrase(void) {
    const char *phrase = hu_missed_message_acknowledgment(7200, 14, 14, 0);
    HU_ASSERT_NOT_NULL(phrase);
}

static void test_missed_msg_overnight_woke_phrase(void) {
    const char *phrase = hu_missed_message_acknowledgment(7200, 2, 8, 0);
    HU_ASSERT_NOT_NULL(phrase);
    HU_ASSERT_NOT_NULL(strstr(phrase, "woke up"));
}

static void test_missed_msg_deterministic(void) {
    const char *a = hu_missed_message_acknowledgment(7200, 14, 14, 42);
    const char *b = hu_missed_message_acknowledgment(7200, 14, 14, 42);
    HU_ASSERT_STR_EQ(a, b);
}

/* ── hu_daemon_set_missed_msg_threshold ──────────────────────────────── */

static void test_set_threshold_changes_behavior(void) {
    /* Default 1800s: 1900s delay should return a phrase */
    const char *before = hu_missed_message_acknowledgment(1900, 12, 12, 0);
    HU_ASSERT_NOT_NULL(before);

    /* Raise to 3600s: 1900s should now be under threshold */
    hu_daemon_set_missed_msg_threshold(3600);
    const char *after = hu_missed_message_acknowledgment(1900, 12, 12, 0);
    HU_ASSERT_NULL(after);

    /* Restore default */
    hu_daemon_set_missed_msg_threshold(1800);
}

static void test_set_threshold_rejects_too_small(void) {
    hu_daemon_set_missed_msg_threshold(1800); /* reset */
    hu_daemon_set_missed_msg_threshold(30);   /* too small, rejected */
    /* 1900s still returns phrase with 1800s threshold */
    const char *phrase = hu_missed_message_acknowledgment(1900, 12, 12, 0);
    HU_ASSERT_NOT_NULL(phrase);
}

void run_daemon_routing_tests(void) {
    HU_TEST_SUITE("daemon_routing");

    /* tapback */
    HU_RUN_TEST(test_tapback_null_returns_false);
    HU_RUN_TEST(test_tapback_question_not_worthy);
    HU_RUN_TEST(test_tapback_short_ack);
    HU_RUN_TEST(test_tapback_single_token_under_12);
    HU_RUN_TEST(test_tapback_multi_word_short_not_worthy);
    HU_RUN_TEST(test_tapback_long_message_not_worthy);

    /* photo delay */
    HU_RUN_TEST(test_photo_delay_no_attachment);
    HU_RUN_TEST(test_photo_delay_with_attachment);

    /* video delay */
    HU_RUN_TEST(test_video_delay_no_video);
    HU_RUN_TEST(test_video_delay_with_video);

    /* missed message */
    HU_RUN_TEST(test_missed_msg_short_delay_null);
    HU_RUN_TEST(test_missed_msg_long_delay_returns_phrase);
    HU_RUN_TEST(test_missed_msg_overnight_woke_phrase);
    HU_RUN_TEST(test_missed_msg_deterministic);
    HU_RUN_TEST(test_set_threshold_changes_behavior);
    HU_RUN_TEST(test_set_threshold_rejects_too_small);
}
