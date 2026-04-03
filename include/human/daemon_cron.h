#ifndef HU_DAEMON_CRON_H
#define HU_DAEMON_CRON_H

#include "core/allocator.h"
#include "core/error.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Internal cron helpers extracted from daemon.c.
 * Public API (hu_cron_schedule_matches, hu_service_run_agent_cron) is in daemon.h.
 */

/* Match a single cron atom (e.g. "5", "1-10", "star/5", "1-10/3") against a value. */
bool hu_cron_atom_matches(const char *atom, size_t len, int value);

/* Match a cron field (comma-separated atoms, or "*") against a value. */
bool hu_cron_field_matches(const char *field, int value);

/* Run system crontab tick: load crontab, execute matching jobs.
 * Called once per minute from the service loop. */
void hu_daemon_cron_tick(hu_allocator_t *alloc);

#endif /* HU_DAEMON_CRON_H */
