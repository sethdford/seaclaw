#ifndef HU_VOICE_DUPLEX_H
#define HU_VOICE_DUPLEX_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * Micro-Turn Duplex FSM
 *
 * Replaces VAD-dependent silence timeouts with LLM-driven turn signals.
 * Audio is processed in small chunks (micro-turns) rather than complete
 * utterances, enabling lower latency and natural turn-taking.
 *
 * State transitions (signal-driven):
 *
 *   IDLE ──user──► USER_TURN ──yield──► AGENT_TURN ──yield──► IDLE
 *     │                │                     │
 *     └──agent──► AGENT_TURN          user(barge)──► OVERLAP
 *                      │                               │  │
 *                 agent(bc)──► BACKCHANNEL    agent(yield)─┘  │
 *                      │           │         user(yield)──────┘
 *                      └───────────┘
 *
 * Fallback mode: silence timeout replaces turn signals for callers
 * without an LLM turn predictor.
 */

typedef enum {
    HU_DUPLEX_IDLE = 0,
    HU_DUPLEX_USER_TURN,
    HU_DUPLEX_AGENT_TURN,
    HU_DUPLEX_OVERLAP,
    HU_DUPLEX_BACKCHANNEL
} hu_duplex_state_t;

/** Turn signals emitted by the LLM or a heuristic turn predictor. */
typedef enum {
    HU_TURN_SIGNAL_NONE = 0,
    HU_TURN_SIGNAL_YIELD,       /* speaker yields the floor */
    HU_TURN_SIGNAL_HOLD,        /* speaker holds the floor despite overlap */
    HU_TURN_SIGNAL_INTERRUPT,   /* barge-in: take the floor */
    HU_TURN_SIGNAL_BACKCHANNEL, /* non-interruptive acknowledgment */
    HU_TURN_SIGNAL_CONTINUE     /* micro-turn boundary, same speaker continues */
} hu_turn_signal_t;

/** Actions the FSM recommends to callers after processing a chunk. */
typedef enum {
    HU_TURN_ACTION_NONE = 0,
    HU_TURN_ACTION_START_GENERATION,  /* begin (or restart) agent response */
    HU_TURN_ACTION_CANCEL_GENERATION, /* stop current agent output */
    HU_TURN_ACTION_SEND_BACKCHANNEL,  /* emit short ack without claiming floor */
    HU_TURN_ACTION_FLUSH_AUDIO,       /* send pending TTS audio to client */
    HU_TURN_ACTION_YIELD_FLOOR        /* agent done, floor is open */
} hu_turn_action_t;

typedef struct hu_duplex_session {
    hu_duplex_state_t state;

    /* Micro-turn tracking */
    uint32_t user_chunks;
    uint32_t agent_chunks;
    uint32_t turn_id;
    int64_t turn_start_ms;
    int64_t last_chunk_ms;
    hu_turn_signal_t last_signal;
    uint32_t total_micro_turns;

    /* Interrupt tracking */
    bool interrupt_detected;
    int64_t total_interrupts;

    /* Streaming output: first-byte latency tracking */
    int64_t output_request_ms;
    int64_t first_byte_ms;
    bool streaming_active;
    bool output_cancelled;

    /* Backchannel */
    bool backchannel_active;

    /* Fallback: silence-timeout mode (no LLM turn predictor) */
    bool fallback_mode;
    bool vad_active;

    /* Metrics */
    int64_t total_outputs;
    double avg_first_byte_latency_ms;
} hu_duplex_session_t;

hu_error_t hu_duplex_session_init(hu_duplex_session_t *session);

/**
 * Process a user audio/text micro-turn chunk with an optional turn signal.
 * Writes the recommended caller action to *out_action.
 */
hu_error_t hu_duplex_user_chunk(hu_duplex_session_t *session, int64_t now_ms,
                                hu_turn_signal_t signal, hu_turn_action_t *out_action);

/**
 * Process an agent audio/text micro-turn chunk with an optional turn signal.
 * Writes the recommended caller action to *out_action.
 */
hu_error_t hu_duplex_agent_chunk(hu_duplex_session_t *session, int64_t now_ms,
                                 hu_turn_signal_t signal, hu_turn_action_t *out_action);

/**
 * Silence timeout expired — fallback turn boundary.
 * Use when no LLM turn predictor is available.
 */
hu_error_t hu_duplex_silence_timeout(hu_duplex_session_t *session, int64_t now_ms,
                                     hu_turn_action_t *out_action);

/* Streaming latency tracking */
hu_error_t hu_duplex_start_streaming(hu_duplex_session_t *session, int64_t now_ms);
hu_error_t hu_duplex_first_byte_sent(hu_duplex_session_t *session, int64_t now_ms);
hu_error_t hu_duplex_cancel_output(hu_duplex_session_t *session);
hu_error_t hu_duplex_end_output(hu_duplex_session_t *session);

/* Configuration */
hu_error_t hu_duplex_set_fallback(hu_duplex_session_t *session, bool fallback);

/* Query */
hu_error_t hu_duplex_check_interrupt(hu_duplex_session_t *session, bool *interrupted);
hu_error_t hu_duplex_update_vad(hu_duplex_session_t *session, bool speech_detected);
hu_error_t hu_duplex_get_latency(const hu_duplex_session_t *session, double *latency_ms);

#endif
