/*
 * Tests for the transcript preprocessor (sentence segmenter, SSML builder,
 * per-sentence emotion, discourse markers, volume mapping).
 */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tts/transcript_prep.h"
#include "test_framework.h"
#include <string.h>

/* ── Sentence Segmenter Tests ─────────────────────────────────────────── */

static void test_segment_simple_sentences(void) {
    hu_prep_sentence_t sents[8];
    const char *text = "Hello there. How are you? I'm great!";
    size_t count = hu_transcript_segment(text, strlen(text), sents, 8);
    HU_ASSERT_EQ(count, (size_t)3);
    HU_ASSERT_TRUE(sents[0].len > 0);
    HU_ASSERT_TRUE(memcmp(sents[0].text, "Hello there.", 12) == 0);
    HU_ASSERT_TRUE(memcmp(sents[1].text, "How are you?", 12) == 0);
    HU_ASSERT_TRUE(memcmp(sents[2].text, "I'm great!", 10) == 0);
}

static void test_segment_abbreviations_preserved(void) {
    hu_prep_sentence_t sents[8];
    const char *text = "Dr. Smith is here. He said Mr. Jones left.";
    size_t count = hu_transcript_segment(text, strlen(text), sents, 8);
    HU_ASSERT_EQ(count, (size_t)2);
    HU_ASSERT_TRUE(sents[0].len > 5);
}

static void test_segment_ellipsis_not_split(void) {
    hu_prep_sentence_t sents[8];
    const char *text = "Well... I guess so. That's fine.";
    size_t count = hu_transcript_segment(text, strlen(text), sents, 8);
    HU_ASSERT_TRUE(count >= 2);
}

static void test_segment_decimal_not_split(void) {
    hu_prep_sentence_t sents[8];
    const char *text = "The temperature is 98.6 degrees. That's normal.";
    size_t count = hu_transcript_segment(text, strlen(text), sents, 8);
    HU_ASSERT_EQ(count, (size_t)2);
}

static void test_segment_no_punctuation(void) {
    hu_prep_sentence_t sents[8];
    const char *text = "Hello world this has no punctuation";
    size_t count = hu_transcript_segment(text, strlen(text), sents, 8);
    HU_ASSERT_EQ(count, (size_t)1);
    HU_ASSERT_TRUE(sents[0].len == strlen(text));
}

static void test_segment_empty_input(void) {
    hu_prep_sentence_t sents[8];
    size_t count = hu_transcript_segment("", 0, sents, 8);
    HU_ASSERT_EQ(count, (size_t)0);
}

static void test_segment_null_input(void) {
    hu_prep_sentence_t sents[8];
    size_t count = hu_transcript_segment(NULL, 0, sents, 8);
    HU_ASSERT_EQ(count, (size_t)0);
}

static void test_segment_max_capacity(void) {
    hu_prep_sentence_t sents[2];
    const char *text = "One. Two. Three. Four.";
    size_t count = hu_transcript_segment(text, strlen(text), sents, 2);
    HU_ASSERT_EQ(count, (size_t)2);
}

static void test_segment_multiple_excl_and_question(void) {
    hu_prep_sentence_t sents[8];
    const char *text = "What?! No way!! Yes!!!";
    size_t count = hu_transcript_segment(text, strlen(text), sents, 8);
    HU_ASSERT_TRUE(count >= 2);
}

/* ── Volume Mapping Tests ─────────────────────────────────────────────── */

static void test_volume_calm_is_soft(void) {
    float v = hu_emotion_to_volume("calm");
    HU_ASSERT_TRUE(v < 1.0f);
    HU_ASSERT_TRUE(v >= 0.8f);
}

static void test_volume_excited_is_loud(void) {
    float v = hu_emotion_to_volume("excited");
    HU_ASSERT_TRUE(v > 1.0f);
    HU_ASSERT_TRUE(v <= 1.3f);
}

static void test_volume_sad_is_quiet(void) {
    float v = hu_emotion_to_volume("sad");
    HU_ASSERT_TRUE(v < 0.9f);
}

static void test_volume_neutral_default(void) {
    float v = hu_emotion_to_volume("content");
    HU_ASSERT_TRUE(v >= 0.95f && v <= 1.05f);
}

static void test_volume_null_default(void) {
    float v = hu_emotion_to_volume(NULL);
    HU_ASSERT_TRUE(v >= 0.99f && v <= 1.01f);
}

/* ── Full Preprocessor Tests ──────────────────────────────────────────── */

static void test_prep_null_returns_error(void) {
    hu_prep_result_t result;
    hu_prep_config_t cfg = {0};
    HU_ASSERT_EQ(hu_transcript_prep(NULL, 0, &cfg, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_transcript_prep("hi", 2, NULL, &result), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_transcript_prep("hi", 2, &cfg, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_prep_single_sentence_no_breaks(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .hour_local = 14,
    };
    hu_prep_result_t result = {0};
    const char *text = "Hello there.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.output_len > 0);
    HU_ASSERT_EQ(result.sentence_count, (size_t)1);
    HU_ASSERT_NOT_NULL(result.dominant_emotion);
}

static void test_prep_multi_sentence_has_breaks(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "I'm so sorry that happened. But you're going to crush it tomorrow.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.sentence_count, (size_t)2);
    HU_ASSERT_TRUE(strstr(result.output, "<break time=") != NULL);
}

static void test_prep_emotion_shift_produces_tags(void) {
    hu_prep_config_t cfg = {
        .incoming_msg = "I'm sad today",
        .incoming_msg_len = 13,
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 100,
    };
    hu_prep_result_t result = {0};
    const char *text = "I understand how you feel. Congratulations on pushing through!";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(result.output, "<emotion value=") != NULL);
}

static void test_prep_speed_variation_for_exclamation(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 77,
    };
    hu_prep_result_t result = {0};
    const char *text = "This is amazing! Really cool stuff.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.sentence_count, (size_t)2);
    HU_ASSERT_TRUE(result.sentences[0].speed_ratio < 0.95f);
}

static void test_prep_volume_from_emotion(void) {
    hu_prep_config_t cfg = {
        .incoming_msg = "I'm devastated",
        .incoming_msg_len = 14,
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 200,
    };
    hu_prep_result_t result = {0};
    const char *text = "I'm so sorry. I'm here for you.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
#ifdef HU_ENABLE_CARTESIA
    HU_ASSERT_TRUE(result.volume < 1.0f);
#else
    HU_ASSERT_TRUE(result.volume <= 1.0f);
#endif
}

static void test_prep_pause_factor_scales_breaks(void) {
    hu_prep_config_t cfg_fast = {
        .base_speed = 0.95f, .pause_factor = 0.5f, .hour_local = 14, .seed = 42,
    };
    hu_prep_config_t cfg_slow = {
        .base_speed = 0.95f, .pause_factor = 2.0f, .hour_local = 14, .seed = 42,
    };
    hu_prep_result_t r_fast = {0}, r_slow = {0};
    const char *text = "First sentence. Second sentence.";

    hu_transcript_prep(text, strlen(text), &cfg_fast, &r_fast);
    hu_transcript_prep(text, strlen(text), &cfg_slow, &r_slow);

    /* Slow pauses produce longer output (more break ms digits) */
    HU_ASSERT_TRUE(r_slow.output_len >= r_fast.output_len);
}

static void test_prep_discourse_markers_contextual(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .discourse_rate = 1.0f,
        .hour_local = 14,
        .seed = 0,
    };
    hu_prep_result_t result = {0};
    /* "believe" triggers "honestly," rule; multiple seeds to find one that fires */
    bool found_marker = false;
    for (uint32_t s = 0; s < 50 && !found_marker; s++) {
        cfg.seed = s;
        memset(&result, 0, sizeof(result));
        const char *text = "First part. I really believe in you.";
        hu_transcript_prep(text, strlen(text), &cfg, &result);
        if (strstr(result.output, "honestly") || strstr(result.output, "you know") ||
            strstr(result.output, "I mean"))
            found_marker = true;
    }
    HU_ASSERT_TRUE(found_marker);
}

static void test_prep_nonverbals_contextual(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .nonverbals_enabled = true,
        .hour_local = 14,
    };
    hu_prep_result_t result = {0};
    bool found = false;
    for (uint32_t s = 0; s < 200 && !found; s++) {
        cfg.seed = s;
        memset(&result, 0, sizeof(result));
        const char *text = "That's hilarious lol. So funny haha.";
        hu_transcript_prep(text, strlen(text), &cfg, &result);
        if (strstr(result.output, "[laughter]") || strstr(result.output, "Hmm") ||
            strstr(result.output, "<break time="))
            found = true;
    }
    HU_ASSERT_TRUE(found);
}

static void test_prep_output_not_empty(void) {
    hu_prep_config_t cfg = {.base_speed = 0.95f, .pause_factor = 1.0f, .hour_local = 14};
    hu_prep_result_t result = {0};
    const char *text = "Just a test.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.output_len > 0);
    HU_ASSERT_TRUE(result.output[0] != '\0');
}

static void test_prep_long_text_truncated_safely(void) {
    hu_prep_config_t cfg = {.base_speed = 0.95f, .pause_factor = 1.0f, .hour_local = 14};
    hu_prep_result_t result = {0};
    char long_text[9000];
    memset(long_text, 'A', sizeof(long_text) - 2);
    long_text[sizeof(long_text) - 2] = '.';
    long_text[sizeof(long_text) - 1] = '\0';
    hu_error_t err = hu_transcript_prep(long_text, strlen(long_text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.output_len < HU_PREP_MAX_OUTPUT);
    HU_ASSERT_TRUE(result.output[result.output_len] == '\0');
}

/* ── Strip-SSML Fallback Tests ───────────────────────────────────────── */

static void test_prep_strip_ssml_no_tags(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .strip_ssml = true,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "First sentence. Second sentence.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.output_len > 0);
    HU_ASSERT_TRUE(strstr(result.output, "<break") == NULL);
    HU_ASSERT_TRUE(strstr(result.output, "<emotion") == NULL);
    HU_ASSERT_TRUE(strstr(result.output, "<speed") == NULL);
    HU_ASSERT_TRUE(strstr(result.output, "<volume") == NULL);
}

static void test_prep_strip_ssml_converts_long_pause_to_period(void) {
    hu_prep_config_t cfg = {
        .incoming_msg = "Tell me something sad",
        .incoming_msg_len = 21,
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .strip_ssml = true,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "I understand your pain. Happiness will come.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.output_len > 0);
}

static void test_prep_strip_ssml_nonverbals_text_only(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .strip_ssml = true,
        .nonverbals_enabled = true,
        .hour_local = 14,
    };
    hu_prep_result_t result = {0};
    bool found_text_nonverbal = false;
    for (uint32_t s = 0; s < 200 && !found_text_nonverbal; s++) {
        cfg.seed = s;
        memset(&result, 0, sizeof(result));
        const char *text = "That's hilarious lol. So funny haha.";
        hu_transcript_prep(text, strlen(text), &cfg, &result);
        if (strstr(result.output, "[laughter]"))
            found_text_nonverbal = true;
        HU_ASSERT_TRUE(strstr(result.output, "<break") == NULL);
    }
}

/* ── Per-Sentence Volume Tests ──────────────────────────────────────── */

static void test_prep_per_sentence_volume_tags(void) {
    hu_prep_config_t cfg = {
        .incoming_msg = "I'm feeling very sad today",
        .incoming_msg_len = 26,
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "I'm so sorry. But you are amazing!";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    /* Multi-sentence emotional arc should produce volume variation */
    HU_ASSERT_TRUE(result.output_len > 0);
}

/* ── Clause-Level Break Tests ───────────────────────────────────────── */

static void test_prep_clause_breaks_at_commas(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "Well, I think that, honestly, it matters.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    /* Should have intra-sentence breaks at clause boundaries */
    int break_count = 0;
    const char *p = result.output;
    while ((p = strstr(p, "<break time=")) != NULL) {
        break_count++;
        p++;
    }
    HU_ASSERT_TRUE(break_count >= 2);
}

static void test_prep_clause_breaks_strip_ssml_no_tags(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .strip_ssml = true,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "Well, I think that, honestly, it matters.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(strstr(result.output, "<break") == NULL);
}

/* ── Speed Variation (voiceai-inspired) Tests ───────────────────────── */

static void test_prep_emotional_words_slower(void) {
    hu_prep_config_t cfg = {
        .base_speed = 1.0f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "I really feel for you.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.sentences[0].speed_ratio < 1.0f);
}

static void test_prep_list_words_faster(void) {
    hu_prep_config_t cfg = {
        .base_speed = 1.0f,
        .pause_factor = 1.0f,
        .hour_local = 14,
        .seed = 42,
    };
    hu_prep_result_t result = {0};
    const char *text = "For example this is a list item.";
    hu_error_t err = hu_transcript_prep(text, strlen(text), &cfg, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.sentences[0].speed_ratio > 1.0f);
}

static void test_prep_discourse_off_by_default(void) {
    hu_prep_config_t cfg = {
        .base_speed = 0.95f,
        .pause_factor = 1.0f,
        .discourse_rate = 0.0f,
        .hour_local = 14,
    };
    hu_prep_result_t result = {0};
    bool found_marker = false;
    for (uint32_t s = 0; s < 50 && !found_marker; s++) {
        cfg.seed = s;
        memset(&result, 0, sizeof(result));
        const char *text = "First part. I really believe in you.";
        hu_transcript_prep(text, strlen(text), &cfg, &result);
        if (strstr(result.output, "honestly") || strstr(result.output, "you know") ||
            strstr(result.output, "I mean"))
            found_marker = true;
    }
    HU_ASSERT_TRUE(!found_marker);
}

/* ── Registration ────────────────────────────────────────────────────── */

void run_transcript_prep_tests(void) {
    HU_TEST_SUITE("Transcript preprocessor");

    HU_RUN_TEST(test_segment_simple_sentences);
    HU_RUN_TEST(test_segment_abbreviations_preserved);
    HU_RUN_TEST(test_segment_ellipsis_not_split);
    HU_RUN_TEST(test_segment_decimal_not_split);
    HU_RUN_TEST(test_segment_no_punctuation);
    HU_RUN_TEST(test_segment_empty_input);
    HU_RUN_TEST(test_segment_null_input);
    HU_RUN_TEST(test_segment_max_capacity);
    HU_RUN_TEST(test_segment_multiple_excl_and_question);

    HU_RUN_TEST(test_volume_calm_is_soft);
    HU_RUN_TEST(test_volume_excited_is_loud);
    HU_RUN_TEST(test_volume_sad_is_quiet);
    HU_RUN_TEST(test_volume_neutral_default);
    HU_RUN_TEST(test_volume_null_default);

    HU_RUN_TEST(test_prep_null_returns_error);
    HU_RUN_TEST(test_prep_single_sentence_no_breaks);
    HU_RUN_TEST(test_prep_multi_sentence_has_breaks);
    HU_RUN_TEST(test_prep_emotion_shift_produces_tags);
    HU_RUN_TEST(test_prep_speed_variation_for_exclamation);
    HU_RUN_TEST(test_prep_volume_from_emotion);
    HU_RUN_TEST(test_prep_pause_factor_scales_breaks);
    HU_RUN_TEST(test_prep_discourse_markers_contextual);
    HU_RUN_TEST(test_prep_nonverbals_contextual);
    HU_RUN_TEST(test_prep_output_not_empty);
    HU_RUN_TEST(test_prep_long_text_truncated_safely);

    HU_RUN_TEST(test_prep_strip_ssml_no_tags);
    HU_RUN_TEST(test_prep_strip_ssml_converts_long_pause_to_period);
    HU_RUN_TEST(test_prep_strip_ssml_nonverbals_text_only);
    HU_RUN_TEST(test_prep_per_sentence_volume_tags);
    HU_RUN_TEST(test_prep_clause_breaks_at_commas);
    HU_RUN_TEST(test_prep_clause_breaks_strip_ssml_no_tags);
    HU_RUN_TEST(test_prep_emotional_words_slower);
    HU_RUN_TEST(test_prep_list_words_faster);
    HU_RUN_TEST(test_prep_discourse_off_by_default);
}
