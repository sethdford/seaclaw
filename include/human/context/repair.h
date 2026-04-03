#ifndef HU_CONTEXT_REPAIR_H
#define HU_CONTEXT_REPAIR_H

#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum hu_repair_type {
    HU_REPAIR_NONE = 0,
    HU_REPAIR_USER_CORRECTION,  /* "no, that's not right" */
    HU_REPAIR_USER_REDIRECT,    /* "I was asking about X, not Y" */
    HU_REPAIR_USER_CONFUSION,   /* "what? that doesn't make sense" */
    HU_REPAIR_MEMORY_ERROR,     /* "you're confusing me with someone" */
    HU_REPAIR_SELF_DETECTED,    /* metacognition detected degradation */
} hu_repair_type_t;

#define HU_REPAIR_DIRECTIVE_CAP 512

typedef struct hu_repair_signal {
    hu_repair_type_t type;
    float confidence;                         /* how sure repair is needed [0,1] */
    char directive[HU_REPAIR_DIRECTIVE_CAP];  /* what to inject into next response */
    bool should_acknowledge;                  /* should we say "I got confused"? */
} hu_repair_signal_t;

/* Detect if user message is a repair attempt via pattern matching. */
hu_error_t hu_repair_detect(const char *user_message, size_t msg_len,
                            hu_repair_signal_t *out);

/* Build repair directive from metacognition degradation signal.
 * consecutive_degrading: how many consecutive calls had is_degrading=true. */
hu_error_t hu_repair_from_metacognition(bool is_degrading, float coherence,
                                        unsigned consecutive_degrading,
                                        hu_repair_signal_t *out);

const char *hu_repair_type_name(hu_repair_type_t type);

#endif /* HU_CONTEXT_REPAIR_H */
