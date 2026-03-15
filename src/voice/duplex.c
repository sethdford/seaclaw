#include "human/voice/duplex.h"
#include <string.h>

hu_error_t hu_duplex_session_init(hu_duplex_session_t *session) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    memset(session, 0, sizeof(*session));
    session->state = HU_DUPLEX_IDLE;
    return HU_OK;
}

hu_error_t hu_duplex_handle_input(hu_duplex_session_t *session, int64_t now_ms) {
    if (!session)
        return HU_ERR_INVALID_ARGUMENT;
    session->last_input_ms = now_ms;
    switch (session->state) {
    case HU_DUPLEX_IDLE:
        session->state = HU_DUPLEX_LISTENING;
        break;
    case HU_DUPLEX_SPEAKING:
        session->state = HU_DUPLEX_BOTH;
        session->interrupt_detected = true;
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
