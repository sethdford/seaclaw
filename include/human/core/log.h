#ifndef HU_CORE_LOG_H
#define HU_CORE_LOG_H

#include "human/observer.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 * Structured logging that routes through hu_observer_t when available,
 * falling back to fprintf(stderr) when no observer is set.
 *
 * Usage:
 *   hu_log_error("daemon", obs, "failed: %s", detail);
 *   hu_log_info("human", obs, "check-in sent to %s", name);
 *
 * The observer pointer may be NULL (triggers fallback).
 */

static inline void hu_log_impl_(const char *component, hu_observer_t *obs, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (obs && obs->vtable && obs->vtable->record_event) {
        hu_observer_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.tag = HU_OBSERVER_EVENT_ERR;
        ev.data.err.component = component;
        ev.data.err.message = buf;
        obs->vtable->record_event(obs->ctx, &ev);
    } else {
        fprintf(stderr, "[%s] %s\n", component, buf);
    }
}

#define hu_log_error(component, obs_ptr, ...) hu_log_impl_((component), (obs_ptr), __VA_ARGS__)
#define hu_log_warn(component, obs_ptr, ...)  hu_log_impl_((component), (obs_ptr), __VA_ARGS__)
#define hu_log_info(component, obs_ptr, ...)  hu_log_impl_((component), (obs_ptr), __VA_ARGS__)

#endif /* HU_CORE_LOG_H */
