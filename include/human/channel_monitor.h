#ifndef HU_CHANNEL_MONITOR_H
#define HU_CHANNEL_MONITOR_H

#include "human/channel.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/health.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Channel health monitor — periodic health checks with automatic
 * reconnection, exponential backoff, and per-channel error tracking.
 *
 * Inspired by OpenClaw's configurable health monitor with stale-event
 * thresholds, restart limits, and per-channel overrides.
 */

#define HU_CHANNEL_MONITOR_MAX 64

typedef struct hu_channel_monitor_config {
    int check_interval_sec;    /* seconds between health checks (default 30) */
    int max_restart_count;     /* max restarts before giving up (default 5) */
    int backoff_initial_sec;   /* initial backoff for reconnect (default 2) */
    int backoff_max_sec;       /* max backoff cap (default 120) */
    int stale_event_threshold; /* seconds without events before warning (default 300) */
} hu_channel_monitor_config_t;

typedef struct hu_channel_status {
    const char *channel_name;
    bool healthy;
    int64_t last_healthy_ts;
    int64_t last_check_ts;
    int64_t last_event_ts;
    uint32_t restart_count;
    uint32_t consecutive_failures;
    int current_backoff_sec;
    char last_error[256];
} hu_channel_status_t;

typedef struct hu_channel_monitor hu_channel_monitor_t;

/* Create a channel monitor with the given config. */
hu_error_t hu_channel_monitor_create(hu_allocator_t *alloc,
                                     const hu_channel_monitor_config_t *config,
                                     hu_channel_monitor_t **out);

void hu_channel_monitor_destroy(hu_channel_monitor_t *mon);

/* Register a channel for monitoring. */
hu_error_t hu_channel_monitor_add(hu_channel_monitor_t *mon, hu_channel_t *channel);

/* Run one health check cycle across all registered channels.
 * Checks health, triggers restart on failure, applies backoff. */
hu_error_t hu_channel_monitor_tick(hu_channel_monitor_t *mon, int64_t now_ts);

/* Record that an event was received on a channel (resets stale timer). */
void hu_channel_monitor_record_event(hu_channel_monitor_t *mon, const char *channel_name);

/* Get status for all monitored channels. Caller does NOT own the array. */
hu_error_t hu_channel_monitor_get_status(hu_channel_monitor_t *mon, const hu_channel_status_t **out,
                                         size_t *count);

hu_channel_monitor_config_t hu_channel_monitor_config_default(void);

#endif /* HU_CHANNEL_MONITOR_H */
