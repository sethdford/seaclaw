#include "human/voice/turn_signal.h"
#include "test_framework.h"
#include <string.h>

/* ── Extract: single tokens ───────────────────────────────────── */

static void test_extract_yield_token(void) {
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract("<|yield|>", 9, &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_YIELD);
    HU_ASSERT_TRUE(r.had_token);
}

static void test_extract_hold_token(void) {
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract("<|hold|>", 8, &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_HOLD);
    HU_ASSERT_TRUE(r.had_token);
}

static void test_extract_backchannel_token(void) {
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract("<|bc|>", 6, &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_BACKCHANNEL);
    HU_ASSERT_TRUE(r.had_token);
}

static void test_extract_continue_token(void) {
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract("<|continue|>", 12, &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_CONTINUE);
    HU_ASSERT_TRUE(r.had_token);
}

/* ── Extract: no token ────────────────────────────────────────── */

static void test_extract_plain_text_no_token(void) {
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract("hello world", 11, &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_NONE);
    HU_ASSERT_FALSE(r.had_token);
}

static void test_extract_empty_text(void) {
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract("", 0, &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_NONE);
    HU_ASSERT_FALSE(r.had_token);
}

static void test_extract_null_text(void) {
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract(NULL, 0, &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_NONE);
}

static void test_extract_null_out(void) {
    HU_ASSERT_EQ(hu_turn_signal_extract("text", 4, NULL), HU_ERR_INVALID_ARGUMENT);
}

/* ── Extract: token in context ────────────────────────────────── */

static void test_extract_token_at_end(void) {
    const char *t = "That's all.<|yield|>";
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract(t, strlen(t), &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_YIELD);
    HU_ASSERT_TRUE(r.had_token);
}

static void test_extract_token_at_start(void) {
    const char *t = "<|hold|>Let me continue.";
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract(t, strlen(t), &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_HOLD);
    HU_ASSERT_TRUE(r.had_token);
}

static void test_extract_token_in_middle(void) {
    const char *t = "Sure, <|bc|> got it.";
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract(t, strlen(t), &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_BACKCHANNEL);
    HU_ASSERT_TRUE(r.had_token);
}

static void test_extract_multiple_tokens_last_wins(void) {
    const char *t = "<|hold|>text<|yield|>";
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract(t, strlen(t), &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_YIELD);
    HU_ASSERT_TRUE(r.had_token);
}

static void test_extract_partial_token_not_matched(void) {
    const char *t = "<|yiel";
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract(t, strlen(t), &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_NONE);
    HU_ASSERT_FALSE(r.had_token);
}

static void test_extract_angle_bracket_not_token(void) {
    const char *t = "x < 5 and y > 3";
    hu_turn_signal_result_t r;
    HU_ASSERT_EQ(hu_turn_signal_extract(t, strlen(t), &r), HU_OK);
    HU_ASSERT_EQ(r.signal, HU_TURN_SIGNAL_NONE);
    HU_ASSERT_FALSE(r.had_token);
}

/* ── Strip: basic ─────────────────────────────────────────────── */

static void test_strip_removes_yield(void) {
    const char *t = "Done.<|yield|>";
    char dst[64];
    size_t len = hu_turn_signal_strip(t, strlen(t), dst, sizeof(dst));
    HU_ASSERT_EQ(len, 5u);
    HU_ASSERT_STR_EQ(dst, "Done.");
}

static void test_strip_removes_hold_at_start(void) {
    const char *t = "<|hold|>Keep going.";
    char dst[64];
    size_t len = hu_turn_signal_strip(t, strlen(t), dst, sizeof(dst));
    HU_ASSERT_STR_EQ(dst, "Keep going.");
    HU_ASSERT_EQ(len, 11u);
}

static void test_strip_removes_multiple(void) {
    const char *t = "<|hold|>A<|bc|>B<|yield|>";
    char dst[64];
    size_t len = hu_turn_signal_strip(t, strlen(t), dst, sizeof(dst));
    HU_ASSERT_STR_EQ(dst, "AB");
    HU_ASSERT_EQ(len, 2u);
}

static void test_strip_plain_text_unchanged(void) {
    const char *t = "Hello world";
    char dst[64];
    size_t len = hu_turn_signal_strip(t, strlen(t), dst, sizeof(dst));
    HU_ASSERT_STR_EQ(dst, "Hello world");
    HU_ASSERT_EQ(len, 11u);
}

static void test_strip_only_token_returns_empty(void) {
    const char *t = "<|yield|>";
    char dst[64];
    size_t len = hu_turn_signal_strip(t, strlen(t), dst, sizeof(dst));
    HU_ASSERT_STR_EQ(dst, "");
    HU_ASSERT_EQ(len, 0u);
}

static void test_strip_empty_text(void) {
    char dst[8];
    size_t len = hu_turn_signal_strip("", 0, dst, sizeof(dst));
    HU_ASSERT_EQ(len, 0u);
    HU_ASSERT_STR_EQ(dst, "");
}

static void test_strip_null_dst(void) {
    size_t len = hu_turn_signal_strip("text", 4, NULL, 0);
    HU_ASSERT_EQ(len, 0u);
}

static void test_strip_dst_cap_one(void) {
    char dst[1];
    size_t len = hu_turn_signal_strip("abc", 3, dst, sizeof(dst));
    HU_ASSERT_EQ(len, 0u);
    HU_ASSERT_EQ(dst[0], '\0');
}

/* ── Registration ─────────────────────────────────────────────── */

void run_turn_signal_tests(void) {
    HU_TEST_SUITE("TurnSignal");

    HU_RUN_TEST(test_extract_yield_token);
    HU_RUN_TEST(test_extract_hold_token);
    HU_RUN_TEST(test_extract_backchannel_token);
    HU_RUN_TEST(test_extract_continue_token);

    HU_RUN_TEST(test_extract_plain_text_no_token);
    HU_RUN_TEST(test_extract_empty_text);
    HU_RUN_TEST(test_extract_null_text);
    HU_RUN_TEST(test_extract_null_out);

    HU_RUN_TEST(test_extract_token_at_end);
    HU_RUN_TEST(test_extract_token_at_start);
    HU_RUN_TEST(test_extract_token_in_middle);
    HU_RUN_TEST(test_extract_multiple_tokens_last_wins);
    HU_RUN_TEST(test_extract_partial_token_not_matched);
    HU_RUN_TEST(test_extract_angle_bracket_not_token);

    HU_RUN_TEST(test_strip_removes_yield);
    HU_RUN_TEST(test_strip_removes_hold_at_start);
    HU_RUN_TEST(test_strip_removes_multiple);
    HU_RUN_TEST(test_strip_plain_text_unchanged);
    HU_RUN_TEST(test_strip_only_token_returns_empty);
    HU_RUN_TEST(test_strip_empty_text);
    HU_RUN_TEST(test_strip_null_dst);
    HU_RUN_TEST(test_strip_dst_cap_one);
}
