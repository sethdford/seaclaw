#include "human/voice/duplex.h"
#include "test_framework.h"

static void test_duplex_init_idle(void) {
    hu_duplex_session_t session;
    hu_error_t err = hu_duplex_session_init(&session);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_IDLE);
}

static void test_duplex_input_transitions_to_listening(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_error_t err = hu_duplex_handle_input(&session, 1000);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_LISTENING);
}

static void test_duplex_output_transitions_to_speaking(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_error_t err = hu_duplex_handle_output(&session, 2000);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_SPEAKING);
}

static void test_duplex_interrupt_detected(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_duplex_handle_output(&session, 1000);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_SPEAKING);
    hu_duplex_handle_input(&session, 1500);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_BOTH);
    HU_ASSERT_TRUE(session.interrupt_detected);

    bool interrupted = false;
    hu_duplex_check_interrupt(&session, &interrupted);
    HU_ASSERT_TRUE(interrupted);
}

static void test_duplex_vad_updates(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    HU_ASSERT_FALSE(session.vad_active);
    hu_duplex_update_vad(&session, true);
    HU_ASSERT_TRUE(session.vad_active);
    hu_duplex_update_vad(&session, false);
    HU_ASSERT_FALSE(session.vad_active);
}

static void test_duplex_null_session_returns_error(void) {
    HU_ASSERT_EQ(hu_duplex_session_init(NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_handle_input(NULL, 0), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_handle_output(NULL, 0), HU_ERR_INVALID_ARGUMENT);
    bool x = false;
    HU_ASSERT_EQ(hu_duplex_check_interrupt(NULL, &x), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_check_interrupt((hu_duplex_session_t *)1, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_update_vad(NULL, true), HU_ERR_INVALID_ARGUMENT);
}

static void test_duplex_streaming_latency(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_duplex_start_streaming(&session, 1000);
    HU_ASSERT_TRUE(session.streaming_active);
    HU_ASSERT_EQ(session.total_outputs, 1);

    hu_duplex_first_byte_sent(&session, 1150);
    double latency = 0;
    hu_duplex_get_latency(&session, &latency);
    HU_ASSERT_TRUE(latency >= 149.0 && latency <= 151.0);
}

static void test_duplex_interrupt_cancels_streaming(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_duplex_handle_output(&session, 1000);
    hu_duplex_start_streaming(&session, 1000);
    HU_ASSERT_TRUE(session.streaming_active);

    hu_duplex_handle_input(&session, 1500);
    HU_ASSERT_TRUE(session.output_cancelled);
    HU_ASSERT_FALSE(session.streaming_active);
    HU_ASSERT_EQ(session.total_interrupts, 1);
}

static void test_duplex_cancel_output(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_duplex_handle_output(&session, 1000);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_SPEAKING);
    hu_duplex_cancel_output(&session);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_IDLE);
    HU_ASSERT_TRUE(session.output_cancelled);
}

static void test_duplex_end_output(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_duplex_handle_output(&session, 1000);
    hu_duplex_start_streaming(&session, 1000);
    hu_duplex_end_output(&session);
    HU_ASSERT_EQ(session.state, HU_DUPLEX_IDLE);
    HU_ASSERT_FALSE(session.streaming_active);
}

static void test_duplex_fallback_mode(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);
    hu_duplex_set_fallback(&session, true);
    HU_ASSERT_TRUE(session.fallback_active);

    hu_duplex_handle_output(&session, 1000);
    hu_duplex_handle_input(&session, 1500);
    /* In fallback mode, input forces LISTENING (no BOTH state) */
    HU_ASSERT_EQ(session.state, HU_DUPLEX_LISTENING);
}

static void test_duplex_latency_averaging(void) {
    hu_duplex_session_t session;
    hu_duplex_session_init(&session);

    hu_duplex_start_streaming(&session, 0);
    hu_duplex_first_byte_sent(&session, 100);
    hu_duplex_end_output(&session);

    hu_duplex_start_streaming(&session, 200);
    hu_duplex_first_byte_sent(&session, 500);
    hu_duplex_end_output(&session);

    double latency = 0;
    hu_duplex_get_latency(&session, &latency);
    /* avg of 100ms and 300ms = 200ms */
    HU_ASSERT_TRUE(latency >= 195.0 && latency <= 205.0);
}

static void test_duplex_streaming_null_args(void) {
    HU_ASSERT_EQ(hu_duplex_start_streaming(NULL, 0), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_first_byte_sent(NULL, 0), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_cancel_output(NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_end_output(NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_set_fallback(NULL, true), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_get_latency(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

void run_voice_duplex_tests(void) {
    HU_TEST_SUITE("VoiceDuplex");
    HU_RUN_TEST(test_duplex_init_idle);
    HU_RUN_TEST(test_duplex_input_transitions_to_listening);
    HU_RUN_TEST(test_duplex_output_transitions_to_speaking);
    HU_RUN_TEST(test_duplex_interrupt_detected);
    HU_RUN_TEST(test_duplex_vad_updates);
    HU_RUN_TEST(test_duplex_null_session_returns_error);
    HU_RUN_TEST(test_duplex_streaming_latency);
    HU_RUN_TEST(test_duplex_interrupt_cancels_streaming);
    HU_RUN_TEST(test_duplex_cancel_output);
    HU_RUN_TEST(test_duplex_end_output);
    HU_RUN_TEST(test_duplex_fallback_mode);
    HU_RUN_TEST(test_duplex_latency_averaging);
    HU_RUN_TEST(test_duplex_streaming_null_args);
}
