#ifndef SC_AGENT_AWARENESS_H
#define SC_AGENT_AWARENESS_H

#include "seaclaw/bus.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Proactive awareness tracks bus events and builds situational context
 * that the agent can inject into its prompt — e.g. recent errors,
 * health state changes, active channels, tool usage patterns. */

#define SC_AWARENESS_MAX_RECENT_ERRORS   5
#define SC_AWARENESS_MAX_ACTIVE_CHANNELS 8
#define SC_AWARENESS_CHANNEL_NAME_LEN    32

typedef struct sc_awareness_state {
    /* Recent errors (circular buffer) */
    struct {
        char text[SC_BUS_MSG_LEN];
        uint64_t timestamp_ms;
    } recent_errors[SC_AWARENESS_MAX_RECENT_ERRORS];
    size_t error_write_idx;
    size_t total_errors;

    /* Active channels seen */
    char active_channels[SC_AWARENESS_MAX_ACTIVE_CHANNELS][SC_AWARENESS_CHANNEL_NAME_LEN];
    size_t active_channel_count;

    /* Counters */
    uint64_t messages_received;
    uint64_t messages_sent;
    uint64_t tool_calls;
    bool health_degraded;
} sc_awareness_state_t;

typedef struct sc_awareness {
    sc_awareness_state_t state;
    sc_bus_t *bus;
} sc_awareness_t;

/* Initialize and subscribe to bus events. */
sc_error_t sc_awareness_init(sc_awareness_t *aw, sc_bus_t *bus);

/* Unsubscribe from bus. */
void sc_awareness_deinit(sc_awareness_t *aw);

/* Build a formatted context string from current awareness state.
 * Caller owns the returned string. Returns NULL if nothing notable. */
char *sc_awareness_context(const sc_awareness_t *aw, sc_allocator_t *alloc, size_t *out_len);

#endif /* SC_AGENT_AWARENESS_H */
