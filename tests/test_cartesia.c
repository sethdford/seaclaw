/*
 * Tests for Cartesia TTS integration.
 * Channel format mapping always runs; synthesize tests need HU_ENABLE_CARTESIA=ON.
 */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tts/cartesia.h"
#include "test_framework.h"
#include <stddef.h>
#include <string.h>

static void test_tts_format_for_channel_imessage_returns_caf(void) {
    HU_ASSERT_STR_EQ(hu_tts_format_for_channel("imessage"), "caf");
}

static void test_tts_format_for_channel_telegram_discord_return_ogg(void) {
    HU_ASSERT_STR_EQ(hu_tts_format_for_channel("telegram"), "ogg");
    HU_ASSERT_STR_EQ(hu_tts_format_for_channel("discord"), "ogg");
}

static void test_tts_format_for_channel_null_and_slack_default_mp3(void) {
    HU_ASSERT_STR_EQ(hu_tts_format_for_channel(NULL), "mp3");
    HU_ASSERT_STR_EQ(hu_tts_format_for_channel("slack"), "mp3");
}

#if HU_ENABLE_CARTESIA

static void test_cartesia_null_api_key_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    unsigned char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_cartesia_tts_synthesize(&alloc, NULL, 0, "hello", 5, NULL, NULL, &out, &out_len);
    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(out);
    HU_ASSERT_EQ(out_len, 0);
}

static void test_cartesia_empty_transcript_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    unsigned char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_cartesia_tts_synthesize(&alloc, "test-key", 8, "", 0, NULL, NULL, &out, &out_len);
    HU_ASSERT_NEQ(err, HU_OK);
    HU_ASSERT_NULL(out);
}

static void test_cartesia_null_config_uses_defaults(void) {
    /* In HU_IS_TEST, synthesize returns mock audio bytes without network */
    hu_allocator_t alloc = hu_system_allocator();
    unsigned char *out = NULL;
    size_t out_len = 0;
    hu_error_t err =
        hu_cartesia_tts_synthesize(&alloc, "test-key", 8, "Hello", 5, NULL, NULL, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_EQ(out_len, 400u);
    HU_ASSERT_EQ(out[0], (unsigned char)0xFF);
    HU_ASSERT_EQ(out[1], (unsigned char)0xFB);
    HU_ASSERT_EQ(out[2], (unsigned char)0x90);
    HU_ASSERT_EQ(out[3], (unsigned char)0x00);
    hu_cartesia_tts_free_bytes(&alloc, out, out_len);
}

static void test_cartesia_free_bytes_handles_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_cartesia_tts_free_bytes(&alloc, NULL, 0);
}

static void test_cartesia_synthesize_with_config(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_cartesia_tts_config_t cfg = {
        .model_id = "sonic-3-2026-01-12",
        .voice_id = "voice-uuid",
        .emotion = "content",
        .speed = 0.95f,
        .volume = 1.0f,
        .nonverbals = false,
    };
    unsigned char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_cartesia_tts_synthesize(&alloc, "test-key", 8, "Hello world", 11, &cfg,
        NULL, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    hu_cartesia_tts_free_bytes(&alloc, out, out_len);
}

static void test_cartesia_synthesize_wav_format_returns_mock_bytes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    unsigned char *out = NULL;
    size_t out_len = 0;
    hu_error_t err = hu_cartesia_tts_synthesize(&alloc, "test-key", 8, "Hi", 2, NULL, "wav", &out,
        &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_EQ(out_len, 400u);
    hu_cartesia_tts_free_bytes(&alloc, out, out_len);
}

#endif /* HU_ENABLE_CARTESIA */

void run_cartesia_tests(void) {
    HU_TEST_SUITE("Cartesia TTS");
    HU_RUN_TEST(test_tts_format_for_channel_imessage_returns_caf);
    HU_RUN_TEST(test_tts_format_for_channel_telegram_discord_return_ogg);
    HU_RUN_TEST(test_tts_format_for_channel_null_and_slack_default_mp3);
#if HU_ENABLE_CARTESIA
    HU_RUN_TEST(test_cartesia_null_api_key_returns_error);
    HU_RUN_TEST(test_cartesia_empty_transcript_returns_error);
    HU_RUN_TEST(test_cartesia_null_config_uses_defaults);
    HU_RUN_TEST(test_cartesia_free_bytes_handles_null);
    HU_RUN_TEST(test_cartesia_synthesize_with_config);
    HU_RUN_TEST(test_cartesia_synthesize_wav_format_returns_mock_bytes);
#endif
}
