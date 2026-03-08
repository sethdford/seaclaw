#ifndef SC_SUPERHUMAN_SILENCE_H
#define SC_SUPERHUMAN_SILENCE_H

#include "seaclaw/agent/superhuman.h"
#include "seaclaw/core/error.h"
#include <stddef.h>
#include <stdint.h>

#define SC_SILENCE_GAP_MS 5000

typedef struct sc_superhuman_silence_ctx {
    uint64_t last_ts_ms;
    uint64_t prev_ts_ms;
    bool has_prev;
} sc_superhuman_silence_ctx_t;

sc_error_t sc_superhuman_silence_service(sc_superhuman_silence_ctx_t *ctx,
                                          sc_superhuman_service_t *out);

#endif /* SC_SUPERHUMAN_SILENCE_H */
