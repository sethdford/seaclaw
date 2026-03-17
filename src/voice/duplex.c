#include "human/voice/duplex.h"
#include <string.h>

hu_error_t hu_duplex_session_init(hu_duplex_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    memset(session, 0, sizeof(*session));
    session->state = HU_DUPLEX_IDLE;
    session->duplex_supported = true;
    return HU_OK;
}

hu_error_t hu_duplex_handle_input(hu_duplex_session_t *session, int64_t now_ms) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->last_input_ms = now_ms;

    if (session->fallback_active) {
        session->state = HU_DUPLEX_LISTENING;
        return HU_OK;
    }

    switch (session->state) {
    case HU_DUPLEX_IDLE:
        session->state = HU_DUPLEX_LISTENING;
        break;
    case HU_DUPLEX_SPEAKING:
        session->state = HU_DUPLEX_BOTH;
        session->interrupt_detected = true;
        session->total_interrupts++;
        if (session->streaming_active) {
            session->output_cancelled = true;
            session->streaming_active = false;
        }
        break;
    case HU_DUPLEX_LISTENING:
    case HU_DUPLEX_BOTH:
        break;
    }
    return HU_OK;
}

hu_error_t hu_duplex_handle_output(hu_duplex_session_t *session, int64_t now_ms) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->last_output_ms = now_ms;
    switch (session->state) {
    case HU_DUPLEX_IDLE:
    case HU_DUPLEX_LISTENING:
        session->state = HU_DUPLEX_SPEAKING;
        break;
    case HU_DUPLEX_SPEAKING:
    case HU_DUPLEX_BOTH:
        break;
    }
    return HU_OK;
}

hu_error_t hu_duplex_check_interrupt(hu_duplex_session_t *session, bool *interrupted) {
    if (!session || !interrupted)
        return HU_ERR_INVALID_ARGUMENT;
    *interrupted = (session->state == HU_DUPLEX_BOTH) || session->interrupt_detected;
    return HU_OK;
}

hu_error_t hu_duplex_update_vad(hu_duplex_session_t *session, bool speech_detected) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->vad_active = speech_detected;
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
    if (session->state == HU_DUPLEX_SPEAKING)
        session->state = HU_DUPLEX_IDLE;
    else if (session->state == HU_DUPLEX_BOTH)
        session->state = HU_DUPLEX_LISTENING;
    return HU_OK;
}

hu_error_t hu_duplex_end_output(hu_duplex_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->streaming_active = false;
    if (session->state == HU_DUPLEX_SPEAKING)
        session->state = HU_DUPLEX_IDLE;
    else if (session->state == HU_DUPLEX_BOTH)
        session->state = HU_DUPLEX_LISTENING;
    return HU_OK;
}

hu_error_t hu_duplex_set_fallback(hu_duplex_session_t *session, bool fallback) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->fallback_active = fallback;
    if (fallback && session->state == HU_DUPLEX_BOTH)
        session->state = HU_DUPLEX_LISTENING;
    return HU_OK;
}

hu_error_t hu_duplex_get_latency(const hu_duplex_session_t *session, double *latency_ms) {
    if (!session || !latency_ms)
        return HU_ERR_INVALID_ARGUMENT;
    *latency_ms = session->avg_first_byte_latency_ms;
    return HU_OK;
}
