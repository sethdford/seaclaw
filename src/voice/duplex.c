#include "human/voice/duplex.h"
#include <string.h>

static void advance_turn(hu_duplex_session_t *s, int64_t now_ms) {
    s->turn_id++;
    s->turn_start_ms = now_ms;
    s->user_chunks = 0;
    s->agent_chunks = 0;
}

hu_error_t hu_duplex_session_init(hu_duplex_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    memset(session, 0, sizeof(*session));
    session->state = HU_DUPLEX_IDLE;
    return HU_OK;
}

hu_error_t hu_duplex_user_chunk(hu_duplex_session_t *session, int64_t now_ms,
                                hu_turn_signal_t signal, hu_turn_action_t *out_action) {
    if (!session || !out_action)
        return HU_ERR_INVALID_ARGUMENT;

    *out_action = HU_TURN_ACTION_NONE;
    session->last_chunk_ms = now_ms;
    session->user_chunks++;
    session->last_signal = signal;
    session->total_micro_turns++;

    if (session->fallback_mode) {
        if (session->state == HU_DUPLEX_AGENT_TURN || session->state == HU_DUPLEX_OVERLAP) {
            session->interrupt_detected = true;
            session->total_interrupts++;
            if (session->streaming_active) {
                session->output_cancelled = true;
                session->streaming_active = false;
            }
            *out_action = HU_TURN_ACTION_CANCEL_GENERATION;
        }
        session->state = HU_DUPLEX_USER_TURN;
        session->turn_start_ms = now_ms;
        return HU_OK;
    }

    switch (session->state) {
    case HU_DUPLEX_IDLE:
        session->state = HU_DUPLEX_USER_TURN;
        advance_turn(session, now_ms);
        session->user_chunks = 1;
        if (signal == HU_TURN_SIGNAL_YIELD) {
            session->state = HU_DUPLEX_AGENT_TURN;
            *out_action = HU_TURN_ACTION_START_GENERATION;
        }
        break;

    case HU_DUPLEX_USER_TURN:
        if (signal == HU_TURN_SIGNAL_YIELD) {
            session->state = HU_DUPLEX_AGENT_TURN;
            advance_turn(session, now_ms);
            *out_action = HU_TURN_ACTION_START_GENERATION;
        }
        break;

    case HU_DUPLEX_AGENT_TURN:
        if (signal == HU_TURN_SIGNAL_BACKCHANNEL)
            break;
        session->state = HU_DUPLEX_OVERLAP;
        session->interrupt_detected = true;
        session->total_interrupts++;
        if (session->streaming_active) {
            session->output_cancelled = true;
            session->streaming_active = false;
        }
        *out_action = HU_TURN_ACTION_CANCEL_GENERATION;
        break;

    case HU_DUPLEX_OVERLAP:
        if (signal == HU_TURN_SIGNAL_YIELD) {
            session->state = HU_DUPLEX_AGENT_TURN;
            advance_turn(session, now_ms);
            *out_action = HU_TURN_ACTION_START_GENERATION;
        }
        break;

    case HU_DUPLEX_BACKCHANNEL:
        if (signal == HU_TURN_SIGNAL_YIELD) {
            session->state = HU_DUPLEX_AGENT_TURN;
            advance_turn(session, now_ms);
            *out_action = HU_TURN_ACTION_START_GENERATION;
        }
        break;
    }
    return HU_OK;
}

hu_error_t hu_duplex_agent_chunk(hu_duplex_session_t *session, int64_t now_ms,
                                 hu_turn_signal_t signal, hu_turn_action_t *out_action) {
    if (!session || !out_action)
        return HU_ERR_INVALID_ARGUMENT;

    *out_action = HU_TURN_ACTION_NONE;
    session->last_chunk_ms = now_ms;
    session->agent_chunks++;
    session->last_signal = signal;
    session->total_micro_turns++;

    switch (session->state) {
    case HU_DUPLEX_IDLE:
        session->state = HU_DUPLEX_AGENT_TURN;
        advance_turn(session, now_ms);
        session->agent_chunks = 1;
        *out_action = HU_TURN_ACTION_FLUSH_AUDIO;
        break;

    case HU_DUPLEX_USER_TURN:
        if (signal == HU_TURN_SIGNAL_BACKCHANNEL) {
            session->state = HU_DUPLEX_BACKCHANNEL;
            session->backchannel_active = true;
            *out_action = HU_TURN_ACTION_SEND_BACKCHANNEL;
        } else {
            session->state = HU_DUPLEX_OVERLAP;
            *out_action = HU_TURN_ACTION_FLUSH_AUDIO;
        }
        break;

    case HU_DUPLEX_AGENT_TURN:
        if (signal == HU_TURN_SIGNAL_YIELD) {
            session->state = HU_DUPLEX_IDLE;
            session->streaming_active = false;
            *out_action = HU_TURN_ACTION_YIELD_FLOOR;
        } else {
            *out_action = HU_TURN_ACTION_FLUSH_AUDIO;
        }
        break;

    case HU_DUPLEX_OVERLAP:
        if (signal == HU_TURN_SIGNAL_YIELD) {
            session->state = HU_DUPLEX_USER_TURN;
            advance_turn(session, now_ms);
            session->streaming_active = false;
            session->interrupt_detected = false;
            *out_action = HU_TURN_ACTION_CANCEL_GENERATION;
        } else {
            *out_action = HU_TURN_ACTION_FLUSH_AUDIO;
        }
        break;

    case HU_DUPLEX_BACKCHANNEL:
        if (signal == HU_TURN_SIGNAL_YIELD) {
            session->state = HU_DUPLEX_USER_TURN;
            session->backchannel_active = false;
            *out_action = HU_TURN_ACTION_YIELD_FLOOR;
        } else {
            *out_action = HU_TURN_ACTION_FLUSH_AUDIO;
        }
        break;
    }
    return HU_OK;
}

hu_error_t hu_duplex_silence_timeout(hu_duplex_session_t *session, int64_t now_ms,
                                     hu_turn_action_t *out_action) {
    if (!session || !out_action)
        return HU_ERR_INVALID_ARGUMENT;

    *out_action = HU_TURN_ACTION_NONE;

    switch (session->state) {
    case HU_DUPLEX_USER_TURN:
        session->state = HU_DUPLEX_AGENT_TURN;
        advance_turn(session, now_ms);
        *out_action = HU_TURN_ACTION_START_GENERATION;
        break;

    case HU_DUPLEX_OVERLAP:
        session->state = HU_DUPLEX_USER_TURN;
        advance_turn(session, now_ms);
        session->streaming_active = false;
        *out_action = HU_TURN_ACTION_CANCEL_GENERATION;
        break;

    default:
        break;
    }
    return HU_OK;
}

hu_error_t hu_duplex_start_streaming(hu_duplex_session_t *session, int64_t now_ms) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->output_request_ms = now_ms;
    session->first_byte_ms = 0;
    session->streaming_active = true;
    session->output_cancelled = false;
    session->total_outputs++;
    return HU_OK;
}

hu_error_t hu_duplex_first_byte_sent(hu_duplex_session_t *session, int64_t now_ms) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    if (!session->streaming_active)
        return HU_ERR_INVALID_ARGUMENT;
    session->first_byte_ms = now_ms;
    double latency = (double)(now_ms - session->output_request_ms);
    if (session->total_outputs <= 1) {
        session->avg_first_byte_latency_ms = latency;
    } else {
        double n = (double)session->total_outputs;
        session->avg_first_byte_latency_ms =
            session->avg_first_byte_latency_ms * ((n - 1.0) / n) + latency / n;
    }
    return HU_OK;
}

hu_error_t hu_duplex_cancel_output(hu_duplex_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->output_cancelled = true;
    session->streaming_active = false;
    if (session->state == HU_DUPLEX_AGENT_TURN)
        session->state = HU_DUPLEX_IDLE;
    else if (session->state == HU_DUPLEX_OVERLAP)
        session->state = HU_DUPLEX_USER_TURN;
    return HU_OK;
}

hu_error_t hu_duplex_end_output(hu_duplex_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->streaming_active = false;
    if (session->state == HU_DUPLEX_AGENT_TURN)
        session->state = HU_DUPLEX_IDLE;
    else if (session->state == HU_DUPLEX_OVERLAP)
        session->state = HU_DUPLEX_USER_TURN;
    return HU_OK;
}

hu_error_t hu_duplex_set_fallback(hu_duplex_session_t *session, bool fallback) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->fallback_mode = fallback;
    if (fallback && session->state == HU_DUPLEX_OVERLAP)
        session->state = HU_DUPLEX_USER_TURN;
    if (fallback)
        session->backchannel_active = false;
    return HU_OK;
}

hu_error_t hu_duplex_check_interrupt(hu_duplex_session_t *session, bool *interrupted) {
    if (!session || !interrupted)
        return HU_ERR_INVALID_ARGUMENT;
    *interrupted = (session->state == HU_DUPLEX_OVERLAP) || session->interrupt_detected;
    return HU_OK;
}

hu_error_t hu_duplex_update_vad(hu_duplex_session_t *session, bool speech_detected) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->vad_active = speech_detected;
    return HU_OK;
}

hu_error_t hu_duplex_get_latency(const hu_duplex_session_t *session, double *latency_ms) {
    if (!session || !latency_ms)
        return HU_ERR_INVALID_ARGUMENT;
    *latency_ms = session->avg_first_byte_latency_ms;
    return HU_OK;
}
