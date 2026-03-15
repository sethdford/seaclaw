#ifndef HU_VOICE_DUPLEX_H
#define HU_VOICE_DUPLEX_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HU_DUPLEX_IDLE = 0,
    HU_DUPLEX_LISTENING,
    HU_DUPLEX_SPEAKING,
    HU_DUPLEX_BOTH /* full duplex */
} hu_duplex_state_t;

typedef struct hu_duplex_session {
    hu_duplex_state_t state;
    bool interrupt_detected;
    int64_t last_input_ms;
    int64_t last_output_ms;
    bool vad_active; /* voice activity detected */
} hu_duplex_session_t;

hu_error_t hu_duplex_session_init(hu_duplex_session_t *session);
hu_error_t hu_duplex_handle_input(hu_duplex_session_t *session, int64_t now_ms);
hu_error_t hu_duplex_handle_output(hu_duplex_session_t *session, int64_t now_ms);
hu_error_t hu_duplex_check_interrupt(hu_duplex_session_t *session, bool *interrupted);
hu_error_t hu_duplex_update_vad(hu_duplex_session_t *session, bool speech_detected);

#endif
