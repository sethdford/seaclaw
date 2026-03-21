#include "human/core/allocator.h"
#include "human/config.h"
#include "human/voice/session.h"
#include "test_framework.h"
#include <stdint.h>
#include <string.h>

static void voice_session_start_stop_lifecycle_ok_under_test(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t vs = {0};
    hu_config_t cfg = {0};
    cfg.voice.tts_provider = NULL;

    hu_error_t err =
        hu_voice_session_start(&alloc, &vs, "telegram", strlen("telegram"), &cfg);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(vs.active);
    HU_ASSERT_EQ(vs.duplex.state, HU_DUPLEX_IDLE);

    err = hu_voice_session_stop(&vs);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(vs.active);
}

static void voice_session_send_audio_inactive_returns_error(void) {
    hu_voice_session_t vs = {0};
    uint8_t pcm[] = {0, 1};
    hu_error_t err = hu_voice_session_send_audio(&vs, pcm, sizeof(pcm));
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_session_interrupt_active_no_crash(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t vs = {0};
    hu_config_t cfg = {0};

    HU_ASSERT_EQ(hu_voice_session_start(&alloc, &vs, "cli", 3, &cfg), HU_OK);
    HU_ASSERT_EQ(hu_voice_session_on_interrupt(&vs), HU_OK);
    HU_ASSERT_EQ(hu_voice_session_stop(&vs), HU_OK);
}

static void voice_session_start_null_config_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t vs = {0};
    hu_error_t err = hu_voice_session_start(&alloc, &vs, "cli", 3, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void voice_session_latency_simulated_under_test_matches_expected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t vs = {0};
    hu_config_t cfg = {0};
    HU_ASSERT_EQ(hu_voice_session_start(&alloc, &vs, "cli", 3, &cfg), HU_OK);
    uint8_t pcm[] = {0, 1};
    HU_ASSERT_EQ(hu_voice_session_send_audio(&vs, pcm, sizeof(pcm)), HU_OK);
    int64_t fb = 0, rt = 0, irq = 0;
    HU_ASSERT_EQ(hu_voice_session_get_latency(&vs, &fb, &rt, &irq), HU_OK);
    HU_ASSERT_EQ(fb, 150);
    HU_ASSERT_EQ(rt, 400);
    HU_ASSERT_EQ(irq, 50);
    HU_ASSERT_EQ(hu_voice_session_stop(&vs), HU_OK);
}

static void voice_session_get_latency_inactive_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_session_t vs = {0};
    hu_config_t cfg = {0};
    HU_ASSERT_EQ(hu_voice_session_start(&alloc, &vs, "cli", 3, &cfg), HU_OK);
    uint8_t pcm[] = {0, 1};
    HU_ASSERT_EQ(hu_voice_session_send_audio(&vs, pcm, sizeof(pcm)), HU_OK);
    HU_ASSERT_EQ(hu_voice_session_stop(&vs), HU_OK);
    int64_t fb = 0, rt = 0, irq = 0;
    HU_ASSERT_EQ(hu_voice_session_get_latency(&vs, &fb, &rt, &irq), HU_ERR_INVALID_ARGUMENT);
}

static void voice_latency_budget_targets_are_defined(void) {
    HU_ASSERT_EQ(HU_VOICE_TARGET_FIRST_BYTE_MS, 200);
    HU_ASSERT_EQ(HU_VOICE_TARGET_ROUND_TRIP_MS, 500);
    HU_ASSERT_EQ(HU_VOICE_TARGET_INTERRUPT_MS, 100);
}

void run_voice_session_tests(void) {
    HU_TEST_SUITE("voice_session");
    HU_RUN_TEST(voice_session_start_stop_lifecycle_ok_under_test);
    HU_RUN_TEST(voice_session_send_audio_inactive_returns_error);
    HU_RUN_TEST(voice_session_interrupt_active_no_crash);
    HU_RUN_TEST(voice_session_start_null_config_returns_error);
    HU_RUN_TEST(voice_session_latency_simulated_under_test_matches_expected);
    HU_RUN_TEST(voice_session_get_latency_inactive_returns_error);
    HU_RUN_TEST(voice_latency_budget_targets_are_defined);
}
