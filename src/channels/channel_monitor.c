/*
 * Channel health monitor — periodic checks with reconnection and backoff.
 */

#include "human/channel_monitor.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

struct hu_channel_monitor {
    hu_allocator_t *alloc;
    hu_channel_monitor_config_t config;
    struct {
        hu_channel_t *channel;
        hu_channel_status_t status;
    } entries[HU_CHANNEL_MONITOR_MAX];
    size_t count;
};

hu_channel_monitor_config_t hu_channel_monitor_config_default(void) {
    return (hu_channel_monitor_config_t){
        .check_interval_sec = 30,
        .max_restart_count = 5,
        .backoff_initial_sec = 2,
        .backoff_max_sec = 120,
        .stale_event_threshold = 300,
    };
}

hu_error_t hu_channel_monitor_create(hu_allocator_t *alloc,
                                     const hu_channel_monitor_config_t *config,
                                     hu_channel_monitor_t **out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_channel_monitor_t *mon =
        (hu_channel_monitor_t *)alloc->alloc(alloc->ctx, sizeof(hu_channel_monitor_t));
    if (!mon)
        return HU_ERR_OUT_OF_MEMORY;
    memset(mon, 0, sizeof(*mon));
    mon->alloc = alloc;
    mon->config = config ? *config : hu_channel_monitor_config_default();
    *out = mon;
    return HU_OK;
}

void hu_channel_monitor_destroy(hu_channel_monitor_t *mon) {
    if (!mon)
        return;
    hu_allocator_t *alloc = mon->alloc;
    alloc->free(alloc->ctx, mon, sizeof(*mon));
}

hu_error_t hu_channel_monitor_add(hu_channel_monitor_t *mon, hu_channel_t *channel) {
    if (!mon || !channel || !channel->vtable || !channel->vtable->name)
        return HU_ERR_INVALID_ARGUMENT;
    if (mon->count >= HU_CHANNEL_MONITOR_MAX)
        return HU_ERR_OUT_OF_MEMORY;

    size_t idx = mon->count;
    mon->entries[idx].channel = channel;
    memset(&mon->entries[idx].status, 0, sizeof(hu_channel_status_t));
    mon->entries[idx].status.channel_name = channel->vtable->name(channel->ctx);
    mon->entries[idx].status.healthy = true;
    mon->entries[idx].status.current_backoff_sec = mon->config.backoff_initial_sec;
    mon->count++;
    return HU_OK;
}

hu_error_t hu_channel_monitor_tick(hu_channel_monitor_t *mon, int64_t now_ts) {
    if (!mon)
        return HU_ERR_INVALID_ARGUMENT;

    for (size_t i = 0; i < mon->count; i++) {
        hu_channel_t *ch = mon->entries[i].channel;
        hu_channel_status_t *st = &mon->entries[i].status;

        /* Skip if check interval hasn't elapsed */
        if (st->last_check_ts > 0 && (now_ts - st->last_check_ts) < mon->config.check_interval_sec)
            continue;

        /* Skip if in backoff after restart failure */
        if (st->consecutive_failures > 0 && st->last_check_ts > 0) {
            int64_t backoff_remaining = st->current_backoff_sec - (now_ts - st->last_check_ts);
            if (backoff_remaining > 0)
                continue;
        }

        st->last_check_ts = now_ts;

        bool ok = false;
        if (ch->vtable->health_check)
            ok = ch->vtable->health_check(ch->ctx);
        else
            ok = true;

        if (ok) {
            st->healthy = true;
            st->last_healthy_ts = now_ts;
            st->consecutive_failures = 0;
            st->current_backoff_sec = mon->config.backoff_initial_sec;
            st->last_error[0] = '\0';
            hu_health_mark_ok(st->channel_name);
        } else {
            st->healthy = false;
            st->consecutive_failures++;

            snprintf(st->last_error, sizeof(st->last_error), "health check failed (attempt %u)",
                     st->consecutive_failures);
            hu_health_mark_error(st->channel_name, st->last_error);

            /* Attempt restart if under the limit */
            if (st->restart_count < (uint32_t)mon->config.max_restart_count) {
#if !HU_IS_TEST
                if (ch->vtable->stop)
                    ch->vtable->stop(ch->ctx);
                hu_error_t start_err = HU_ERR_NOT_SUPPORTED;
                if (ch->vtable->start)
                    start_err = ch->vtable->start(ch->ctx);

                if (start_err == HU_OK) {
                    st->healthy = true;
                    st->last_healthy_ts = now_ts;
                    st->consecutive_failures = 0;
                    st->current_backoff_sec = mon->config.backoff_initial_sec;
                    st->last_error[0] = '\0';
                    hu_health_mark_ok(st->channel_name);
                }
#endif
                st->restart_count++;
                hu_health_bump_restart(st->channel_name);
            }

            /* Exponential backoff: double up to max */
            st->current_backoff_sec *= 2;
            if (st->current_backoff_sec > mon->config.backoff_max_sec)
                st->current_backoff_sec = mon->config.backoff_max_sec;
        }

        /* Stale event detection */
        if (mon->config.stale_event_threshold > 0 && st->last_event_ts > 0 &&
            (now_ts - st->last_event_ts) > mon->config.stale_event_threshold && st->healthy) {
            snprintf(st->last_error, sizeof(st->last_error), "no events for %lld seconds",
                     (long long)(now_ts - st->last_event_ts));
        }
    }
    return HU_OK;
}

void hu_channel_monitor_record_event(hu_channel_monitor_t *mon, const char *channel_name) {
    if (!mon || !channel_name)
        return;
    for (size_t i = 0; i < mon->count; i++) {
        if (mon->entries[i].status.channel_name &&
            strcmp(mon->entries[i].status.channel_name, channel_name) == 0) {
            mon->entries[i].status.last_event_ts = mon->entries[i].status.last_check_ts;
            return;
        }
    }
}

hu_error_t hu_channel_monitor_get_status(hu_channel_monitor_t *mon, const hu_channel_status_t **out,
                                         size_t *count) {
    if (!mon || !out || !count)
        return HU_ERR_INVALID_ARGUMENT;

    static hu_channel_status_t status_buf[HU_CHANNEL_MONITOR_MAX];
    for (size_t i = 0; i < mon->count; i++)
        status_buf[i] = mon->entries[i].status;
    *out = status_buf;
    *count = mon->count;
    return HU_OK;
}
