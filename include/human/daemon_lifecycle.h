#ifndef HU_DAEMON_LIFECYCLE_H
#define HU_DAEMON_LIFECYCLE_H

#include "core/allocator.h"
#include "core/error.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Daemon lifecycle management extracted from daemon.c.
 *
 * Public API (hu_daemon_start/stop/status, hu_daemon_write_pid/remove_pid,
 * hu_daemon_install/uninstall/logs) is declared in daemon.h.
 *
 * This header exposes internal helpers needed across daemon modules.
 */

/* Validate HOME path contains only safe characters. */
hu_error_t hu_daemon_validate_home(const char *home);

/* Build PID file path into buf. Returns snprintf result (<0 on error). */
int hu_daemon_get_pid_path(char *buf, size_t buf_size);

#endif /* HU_DAEMON_LIFECYCLE_H */
