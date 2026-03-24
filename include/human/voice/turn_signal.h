#ifndef HU_VOICE_TURN_SIGNAL_H
#define HU_VOICE_TURN_SIGNAL_H

#include "human/core/error.h"
#include "human/voice/duplex.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Control tokens the LLM can emit to steer turn-taking.
 * These are stripped from the text before TTS synthesis.
 */
#define HU_TURN_TOKEN_YIELD    "<|yield|>"
#define HU_TURN_TOKEN_HOLD     "<|hold|>"
#define HU_TURN_TOKEN_BC       "<|bc|>"
#define HU_TURN_TOKEN_CONTINUE "<|continue|>"

typedef struct hu_turn_signal_result {
    hu_turn_signal_t signal;
    bool had_token;
} hu_turn_signal_result_t;

/**
 * Scan text for control tokens and return the detected turn signal.
 * If multiple tokens are present, the last one wins.
 * Text without control tokens yields HU_TURN_SIGNAL_NONE.
 */
hu_error_t hu_turn_signal_extract(const char *text, size_t text_len, hu_turn_signal_result_t *out);

/**
 * Copy text to dst with all control tokens removed.
 * Returns the length of the stripped output (excluding NUL).
 * Always NUL-terminates dst when dst_cap > 0.
 */
size_t hu_turn_signal_strip(const char *text, size_t text_len, char *dst, size_t dst_cap);

#endif
