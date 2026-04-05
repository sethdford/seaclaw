#include "human/cognition/rupture_repair.h"
#include "human/core/string.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void rupture_clear(hu_allocator_t *alloc, hu_rupture_state_t *state) {
    if (!alloc || !state)
        return;
    hu_str_free(alloc, state->trigger_summary);
    hu_str_free(alloc, state->last_assistant_msg);
    hu_str_free(alloc, state->repair_directive);
    state->trigger_summary = NULL;
    state->last_assistant_msg = NULL;
    state->repair_directive = NULL;
    state->severity = 0.0f;
    state->turns_since = 0;
    state->stage = HU_RUPTURE_NONE;
}

void hu_rupture_init(hu_rupture_state_t *state) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    state->stage = HU_RUPTURE_NONE;
}

void hu_rupture_deinit(hu_allocator_t *alloc, hu_rupture_state_t *state) {
    if (!alloc || !state)
        return;
    rupture_clear(alloc, state);
}

const char *hu_rupture_stage_name(hu_rupture_stage_t stage) {
    switch (stage) {
    case HU_RUPTURE_NONE:
        return "none";
    case HU_RUPTURE_DETECTED:
        return "detected";
    case HU_RUPTURE_ACKNOWLEDGED:
        return "acknowledged";
    case HU_RUPTURE_REPAIRING:
        return "repairing";
    case HU_RUPTURE_REPAIRED:
        return "repaired";
    case HU_RUPTURE_UNRESOLVED:
        return "unresolved";
    default:
        return "unknown";
    }
}

hu_error_t hu_rupture_evaluate(hu_allocator_t *alloc, hu_rupture_state_t *state,
                               const hu_rupture_signals_t *signals,
                               const char *last_assistant_msg, size_t msg_len) {
    if (!alloc || !state || !signals)
        return HU_ERR_INVALID_ARGUMENT;

    if (state->stage == HU_RUPTURE_REPAIRED || state->stage == HU_RUPTURE_UNRESOLVED) {
        state->turns_since++;
        if (state->turns_since >= 3u)
            rupture_clear(alloc, state);
        return HU_OK;
    }

    if (state->stage != HU_RUPTURE_NONE) {
        state->turns_since++;
        if (state->turns_since > 8u && state->stage != HU_RUPTURE_REPAIRED) {
            state->stage = HU_RUPTURE_UNRESOLVED;
            state->turns_since = 0;
        }
    }

    if (state->stage != HU_RUPTURE_NONE)
        return HU_OK;

    bool rupture = signals->tone_delta < -0.3f || signals->explicit_correction ||
                   (signals->length_delta < -0.5f && signals->energy_drop > 0.3f) ||
                   signals->user_withdrawal;
    if (!rupture)
        return HU_OK;

    state->stage = HU_RUPTURE_DETECTED;
    state->severity = fmaxf(fabsf(signals->tone_delta), signals->energy_drop);
    if (state->severity > 1.0f)
        state->severity = 1.0f;
    state->turns_since = 0;

    hu_str_free(alloc, state->trigger_summary);
    state->trigger_summary = NULL;
    if (signals->explicit_correction) {
        static const char msg[] = "User corrected something";
        state->trigger_summary = hu_strndup(alloc, msg, strlen(msg));
    } else if (signals->user_withdrawal) {
        static const char msg[] = "User may have withdrawn or cooled off";
        state->trigger_summary = hu_strndup(alloc, msg, strlen(msg));
    } else {
        static const char msg[] = "Tone shift detected after your last message";
        state->trigger_summary = hu_strndup(alloc, msg, strlen(msg));
    }
    if (!state->trigger_summary) {
        rupture_clear(alloc, state);
        return HU_ERR_OUT_OF_MEMORY;
    }

    hu_str_free(alloc, state->last_assistant_msg);
    state->last_assistant_msg = NULL;
    if (last_assistant_msg && msg_len > 0) {
        state->last_assistant_msg = hu_strndup(alloc, last_assistant_msg, msg_len);
        if (!state->last_assistant_msg) {
            hu_str_free(alloc, state->trigger_summary);
            state->trigger_summary = NULL;
            state->stage = HU_RUPTURE_NONE;
            state->severity = 0.0f;
            return HU_ERR_OUT_OF_MEMORY;
        }
    }

    return HU_OK;
}

hu_error_t hu_rupture_advance(hu_allocator_t *alloc, hu_rupture_state_t *state,
                              bool user_reengaged) {
    if (!alloc || !state)
        return HU_ERR_INVALID_ARGUMENT;

    switch (state->stage) {
    case HU_RUPTURE_DETECTED:
        state->stage = HU_RUPTURE_ACKNOWLEDGED;
        break;
    case HU_RUPTURE_ACKNOWLEDGED:
        state->stage = HU_RUPTURE_REPAIRING;
        state->turns_since = 0;
        break;
    case HU_RUPTURE_REPAIRING:
        if (user_reengaged) {
            state->stage = HU_RUPTURE_REPAIRED;
            state->turns_since = 0;
        } else if (state->turns_since > 5u) {
            state->stage = HU_RUPTURE_UNRESOLVED;
            state->turns_since = 0;
        }
        break;
    default:
        break;
    }
    (void)alloc;
    return HU_OK;
}

hu_error_t hu_rupture_build_context(hu_allocator_t *alloc, const hu_rupture_state_t *state,
                                    char **out, size_t *out_len) {
    if (!alloc || !state || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    if (state->stage == HU_RUPTURE_NONE)
        return HU_OK;

    char buf[640];
    int n = 0;

    switch (state->stage) {
    case HU_RUPTURE_DETECTED: {
        const char *trig = state->trigger_summary ? state->trigger_summary : "Signals suggest a mismatch";
        float pct = state->severity * 100.0f;
        if (pct > 100.0f)
            pct = 100.0f;
        n = snprintf(buf, sizeof(buf),
                     "[RUPTURE DETECTED: Something you said may not have landed well. %s. "
                     "Acknowledge it gently — don't assume what went wrong, ask. Severity: %.0f%%]",
                     trig, (double)pct);
        break;
    }
    case HU_RUPTURE_ACKNOWLEDGED:
    case HU_RUPTURE_REPAIRING:
        n = snprintf(buf, sizeof(buf),
                     "[RUPTURE REPAIR: Stay with the repair. Be specific about what you might have "
                     "gotten wrong. Don't rush past it or change the subject.]");
        break;
    case HU_RUPTURE_REPAIRED:
        n = snprintf(buf, sizeof(buf),
                     "[RUPTURE REPAIRED: The repair worked — the user re-engaged. Let it settle. "
                     "Don't over-process.]");
        break;
    case HU_RUPTURE_UNRESOLVED:
        n = snprintf(buf, sizeof(buf),
                     "[RUPTURE UNRESOLVED: The user may still be processing. Give space. Don't "
                     "pretend it didn't happen.]");
        break;
    default:
        return HU_OK;
    }

    if (n <= 0 || (size_t)n >= sizeof(buf))
        return HU_ERR_INVALID_ARGUMENT;

    char *result = hu_strndup(alloc, buf, (size_t)n);
    if (!result)
        return HU_ERR_OUT_OF_MEMORY;
    *out = result;
    *out_len = (size_t)n;
    return HU_OK;
}
