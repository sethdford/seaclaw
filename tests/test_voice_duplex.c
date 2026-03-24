#include "human/voice/duplex.h"
#include "test_framework.h"

/* ── Init ─────────────────────────────────────────────────────── */

static void test_duplex_init_idle(void) {
    hu_duplex_session_t s;
    HU_ASSERT_EQ(hu_duplex_session_init(&s), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_IDLE);
    HU_ASSERT_EQ(s.turn_id, 0u);
    HU_ASSERT_EQ(s.total_micro_turns, 0u);
}

static void test_duplex_init_null(void) {
    HU_ASSERT_EQ(hu_duplex_session_init(NULL), HU_ERR_INVALID_ARGUMENT);
}

/* ── User chunk: IDLE transitions ─────────────────────────────── */

static void test_user_chunk_idle_to_user_turn(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_NONE);
    HU_ASSERT_EQ(s.turn_id, 1u);
    HU_ASSERT_EQ(s.user_chunks, 1u);
}

static void test_user_chunk_idle_yield_starts_generation(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_START_GENERATION);
}

/* ── User chunk: USER_TURN transitions ────────────────────────── */

static void test_user_chunk_continue_stays_user_turn(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_CONTINUE, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_NONE);
    HU_ASSERT_EQ(s.user_chunks, 2u);
}

static void test_user_chunk_yield_starts_agent(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_CONTINUE, &action);
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_START_GENERATION);
    HU_ASSERT_EQ(s.turn_id, 2u);
}

/* ── User chunk: AGENT_TURN transitions (barge-in) ────────────── */

static void test_user_chunk_agent_turn_interrupt(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);

    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_CANCEL_GENERATION);
    HU_ASSERT_TRUE(s.interrupt_detected);
    HU_ASSERT_EQ(s.total_interrupts, 1);
}

static void test_user_chunk_agent_turn_cancels_streaming(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_start_streaming(&s, 100);
    HU_ASSERT_TRUE(s.streaming_active);

    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_TRUE(s.output_cancelled);
    HU_ASSERT_FALSE(s.streaming_active);
}

static void test_user_backchannel_during_agent_no_interrupt(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);

    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_BACKCHANNEL, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_NONE);
    HU_ASSERT_FALSE(s.interrupt_detected);
    HU_ASSERT_EQ(s.total_interrupts, 0);
}

/* ── Agent chunk: IDLE transitions ────────────────────────────── */

static void test_agent_chunk_idle_to_agent_turn(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_FLUSH_AUDIO);
    HU_ASSERT_EQ(s.agent_chunks, 1u);
}

/* ── Agent chunk: AGENT_TURN transitions ──────────────────────── */

static void test_agent_chunk_yield_to_idle(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_CONTINUE, &action);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_FLUSH_AUDIO);

    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_IDLE);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_YIELD_FLOOR);
}

static void test_agent_chunk_hold_stays_agent_turn(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_HOLD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_FLUSH_AUDIO);
}

/* ── Agent chunk: USER_TURN transitions ───────────────────────── */

static void test_agent_backchannel_during_user_turn(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);

    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_BACKCHANNEL, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_BACKCHANNEL);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_SEND_BACKCHANNEL);
    HU_ASSERT_TRUE(s.backchannel_active);
}

static void test_agent_chunk_user_turn_no_signal_overlaps(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_FLUSH_AUDIO);
}

/* ── Overlap resolution ───────────────────────────────────────── */

static void test_overlap_agent_yields_to_user(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);

    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_CANCEL_GENERATION);
    HU_ASSERT_FALSE(s.interrupt_detected);
}

static void test_overlap_user_yields_to_agent(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);

    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_START_GENERATION);
}

static void test_overlap_stays_without_yield(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);

    hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_NONE);
}

/* ── Backchannel resolution ───────────────────────────────────── */

static void test_backchannel_agent_yields_back_to_user(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_BACKCHANNEL, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_BACKCHANNEL);

    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_YIELD_FLOOR);
    HU_ASSERT_FALSE(s.backchannel_active);
}

static void test_backchannel_user_yields_starts_agent(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_BACKCHANNEL, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_BACKCHANNEL);

    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_START_GENERATION);
}

static void test_backchannel_agent_continues_flushing(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_BACKCHANNEL, &action);

    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 250, HU_TURN_SIGNAL_CONTINUE, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_BACKCHANNEL);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_FLUSH_AUDIO);
}

/* ── Silence timeout ──────────────────────────────────────────── */

static void test_silence_timeout_user_turn_starts_generation(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);

    HU_ASSERT_EQ(hu_duplex_silence_timeout(&s, 600, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_START_GENERATION);
}

static void test_silence_timeout_overlap_user_wins(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);

    HU_ASSERT_EQ(hu_duplex_silence_timeout(&s, 700, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_CANCEL_GENERATION);
}

static void test_silence_timeout_idle_noop(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_silence_timeout(&s, 500, &action), HU_OK);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_IDLE);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_NONE);
}

/* ── Fallback mode ────────────────────────────────────────────── */

static void test_fallback_user_during_agent_interrupts(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_duplex_set_fallback(&s, true);
    hu_turn_action_t action;

    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);

    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_CANCEL_GENERATION);
    HU_ASSERT_TRUE(s.interrupt_detected);
}

static void test_fallback_no_backchannel_state(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_duplex_set_fallback(&s, true);
    hu_turn_action_t action;

    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_agent_chunk(&s, 200, HU_TURN_SIGNAL_BACKCHANNEL, &action);
    /* Fallback doesn't honor backchannel at the user_chunk level;
     * agent_chunk still works normally but fallback user input
     * always forces USER_TURN. */
    HU_ASSERT_EQ(s.state, HU_DUPLEX_BACKCHANNEL);

    hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
}

static void test_fallback_overlap_collapses_to_user(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;

    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);

    hu_duplex_set_fallback(&s, true);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
}

/* ── Streaming latency tracking ───────────────────────────────── */

static void test_streaming_latency_single(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_duplex_start_streaming(&s, 1000);
    HU_ASSERT_TRUE(s.streaming_active);
    HU_ASSERT_EQ(s.total_outputs, 1);

    hu_duplex_first_byte_sent(&s, 1150);
    double latency = 0;
    hu_duplex_get_latency(&s, &latency);
    HU_ASSERT_TRUE(latency >= 149.0 && latency <= 151.0);
}

static void test_streaming_latency_averaging(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);

    hu_duplex_start_streaming(&s, 0);
    hu_duplex_first_byte_sent(&s, 100);
    hu_duplex_end_output(&s);

    hu_duplex_start_streaming(&s, 200);
    hu_duplex_first_byte_sent(&s, 500);
    hu_duplex_end_output(&s);

    double latency = 0;
    hu_duplex_get_latency(&s, &latency);
    HU_ASSERT_TRUE(latency >= 195.0 && latency <= 205.0);
}

static void test_streaming_latency_under_target(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);

    hu_duplex_start_streaming(&s, 1000);
    hu_duplex_first_byte_sent(&s, 1120);
    hu_duplex_end_output(&s);

    hu_duplex_start_streaming(&s, 2000);
    hu_duplex_first_byte_sent(&s, 2180);
    hu_duplex_end_output(&s);

    hu_duplex_start_streaming(&s, 3000);
    hu_duplex_first_byte_sent(&s, 3090);
    hu_duplex_end_output(&s);

    double latency = 0;
    hu_duplex_get_latency(&s, &latency);
    HU_ASSERT_TRUE(latency < 200.0);
    HU_ASSERT_TRUE(latency > 0.0);
}

/* ── Cancel / end output ──────────────────────────────────────── */

static void test_cancel_output_agent_to_idle(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    hu_duplex_cancel_output(&s);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_IDLE);
    HU_ASSERT_TRUE(s.output_cancelled);
}

static void test_end_output_agent_to_idle(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_start_streaming(&s, 100);
    hu_duplex_end_output(&s);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_IDLE);
    HU_ASSERT_FALSE(s.streaming_active);
}

static void test_cancel_output_overlap_to_user(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_OVERLAP);
    hu_duplex_cancel_output(&s);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
}

/* ── VAD ──────────────────────────────────────────────────────── */

static void test_vad_updates(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    HU_ASSERT_FALSE(s.vad_active);
    hu_duplex_update_vad(&s, true);
    HU_ASSERT_TRUE(s.vad_active);
    hu_duplex_update_vad(&s, false);
    HU_ASSERT_FALSE(s.vad_active);
}

/* ── Micro-turn counting ──────────────────────────────────────── */

static void test_micro_turn_counter(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_CONTINUE, &action);
    hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action);
    hu_duplex_agent_chunk(&s, 400, HU_TURN_SIGNAL_CONTINUE, &action);
    hu_duplex_agent_chunk(&s, 500, HU_TURN_SIGNAL_YIELD, &action);
    HU_ASSERT_EQ(s.total_micro_turns, 5u);
}

/* ── Turn ID tracking ─────────────────────────────────────────── */

static void test_turn_id_increments_on_transitions(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;

    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.turn_id, 1u);

    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_YIELD, &action);
    HU_ASSERT_EQ(s.turn_id, 2u);

    hu_duplex_agent_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_IDLE);
}

/* ── Full conversation flow ───────────────────────────────────── */

static void test_full_conversation_flow(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;

    /* User speaks 3 micro-turns, then yields */
    hu_duplex_user_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_USER_TURN);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_CONTINUE, &action);
    hu_duplex_user_chunk(&s, 300, HU_TURN_SIGNAL_YIELD, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_START_GENERATION);

    /* Agent generates 2 chunks, sends backchannel-worthy "mm-hmm" from user */
    hu_duplex_agent_chunk(&s, 400, HU_TURN_SIGNAL_CONTINUE, &action);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_FLUSH_AUDIO);
    hu_duplex_user_chunk(&s, 450, HU_TURN_SIGNAL_BACKCHANNEL, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_AGENT_TURN);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_NONE);

    /* Agent finishes */
    hu_duplex_agent_chunk(&s, 500, HU_TURN_SIGNAL_YIELD, &action);
    HU_ASSERT_EQ(s.state, HU_DUPLEX_IDLE);
    HU_ASSERT_EQ(action, HU_TURN_ACTION_YIELD_FLOOR);
}

/* ── Null argument handling ───────────────────────────────────── */

static void test_null_args_user_chunk(void) {
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_user_chunk(NULL, 0, HU_TURN_SIGNAL_NONE, &action),
                 HU_ERR_INVALID_ARGUMENT);
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    HU_ASSERT_EQ(hu_duplex_user_chunk(&s, 0, HU_TURN_SIGNAL_NONE, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_null_args_agent_chunk(void) {
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_agent_chunk(NULL, 0, HU_TURN_SIGNAL_NONE, &action),
                 HU_ERR_INVALID_ARGUMENT);
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    HU_ASSERT_EQ(hu_duplex_agent_chunk(&s, 0, HU_TURN_SIGNAL_NONE, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_null_args_silence_timeout(void) {
    hu_turn_action_t action;
    HU_ASSERT_EQ(hu_duplex_silence_timeout(NULL, 0, &action), HU_ERR_INVALID_ARGUMENT);
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    HU_ASSERT_EQ(hu_duplex_silence_timeout(&s, 0, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_null_args_streaming(void) {
    HU_ASSERT_EQ(hu_duplex_start_streaming(NULL, 0), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_first_byte_sent(NULL, 0), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_cancel_output(NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_end_output(NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_set_fallback(NULL, true), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_get_latency(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_check_interrupt(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_duplex_update_vad(NULL, true), HU_ERR_INVALID_ARGUMENT);
}

/* ── Check interrupt query ────────────────────────────────────── */

static void test_check_interrupt_overlap(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    hu_turn_action_t action;
    hu_duplex_agent_chunk(&s, 100, HU_TURN_SIGNAL_NONE, &action);
    hu_duplex_user_chunk(&s, 200, HU_TURN_SIGNAL_NONE, &action);

    bool interrupted = false;
    hu_duplex_check_interrupt(&s, &interrupted);
    HU_ASSERT_TRUE(interrupted);
}

static void test_check_interrupt_idle_false(void) {
    hu_duplex_session_t s;
    hu_duplex_session_init(&s);
    bool interrupted = true;
    hu_duplex_check_interrupt(&s, &interrupted);
    HU_ASSERT_FALSE(interrupted);
}

/* ── Registration ─────────────────────────────────────────────── */

void run_voice_duplex_tests(void) {
    HU_TEST_SUITE("VoiceDuplex");

    HU_RUN_TEST(test_duplex_init_idle);
    HU_RUN_TEST(test_duplex_init_null);

    HU_RUN_TEST(test_user_chunk_idle_to_user_turn);
    HU_RUN_TEST(test_user_chunk_idle_yield_starts_generation);
    HU_RUN_TEST(test_user_chunk_continue_stays_user_turn);
    HU_RUN_TEST(test_user_chunk_yield_starts_agent);
    HU_RUN_TEST(test_user_chunk_agent_turn_interrupt);
    HU_RUN_TEST(test_user_chunk_agent_turn_cancels_streaming);
    HU_RUN_TEST(test_user_backchannel_during_agent_no_interrupt);

    HU_RUN_TEST(test_agent_chunk_idle_to_agent_turn);
    HU_RUN_TEST(test_agent_chunk_yield_to_idle);
    HU_RUN_TEST(test_agent_chunk_hold_stays_agent_turn);
    HU_RUN_TEST(test_agent_backchannel_during_user_turn);
    HU_RUN_TEST(test_agent_chunk_user_turn_no_signal_overlaps);

    HU_RUN_TEST(test_overlap_agent_yields_to_user);
    HU_RUN_TEST(test_overlap_user_yields_to_agent);
    HU_RUN_TEST(test_overlap_stays_without_yield);

    HU_RUN_TEST(test_backchannel_agent_yields_back_to_user);
    HU_RUN_TEST(test_backchannel_user_yields_starts_agent);
    HU_RUN_TEST(test_backchannel_agent_continues_flushing);

    HU_RUN_TEST(test_silence_timeout_user_turn_starts_generation);
    HU_RUN_TEST(test_silence_timeout_overlap_user_wins);
    HU_RUN_TEST(test_silence_timeout_idle_noop);

    HU_RUN_TEST(test_fallback_user_during_agent_interrupts);
    HU_RUN_TEST(test_fallback_no_backchannel_state);
    HU_RUN_TEST(test_fallback_overlap_collapses_to_user);

    HU_RUN_TEST(test_streaming_latency_single);
    HU_RUN_TEST(test_streaming_latency_averaging);
    HU_RUN_TEST(test_streaming_latency_under_target);

    HU_RUN_TEST(test_cancel_output_agent_to_idle);
    HU_RUN_TEST(test_end_output_agent_to_idle);
    HU_RUN_TEST(test_cancel_output_overlap_to_user);

    HU_RUN_TEST(test_vad_updates);
    HU_RUN_TEST(test_micro_turn_counter);
    HU_RUN_TEST(test_turn_id_increments_on_transitions);
    HU_RUN_TEST(test_full_conversation_flow);

    HU_RUN_TEST(test_null_args_user_chunk);
    HU_RUN_TEST(test_null_args_agent_chunk);
    HU_RUN_TEST(test_null_args_silence_timeout);
    HU_RUN_TEST(test_null_args_streaming);

    HU_RUN_TEST(test_check_interrupt_overlap);
    HU_RUN_TEST(test_check_interrupt_idle_false);
}
