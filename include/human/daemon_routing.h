#ifndef HU_DAEMON_ROUTING_H
#define HU_DAEMON_ROUTING_H

#include "channel_loop.h"
#include "core/allocator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Daemon message routing utilities extracted from daemon.c.
 *
 * Public API (hu_daemon_set_missed_msg_threshold, hu_missed_message_acknowledgment,
 * hu_daemon_photo_viewing_delay_ms, hu_daemon_video_viewing_delay_ms) is in daemon.h.
 */

/* Tapback-worthy: message properties suggest acknowledgment, not response.
 * Uses structural checks (length, question mark, word count). */
bool hu_daemon_is_tapback_worthy(const char *msg, size_t len);

/* Photo viewing delay: returns 3-8 s (ms) if batch has attachment, else 0. */
uint32_t hu_daemon_compute_photo_delay(const hu_channel_loop_msg_t *msgs, size_t batch_start,
                                       size_t batch_end, uint32_t seed);

/* Video viewing delay: returns 2-10 s (ms) if batch has video, else 0. */
uint32_t hu_daemon_compute_video_delay(const hu_channel_loop_msg_t *msgs, size_t batch_start,
                                       size_t batch_end, uint32_t seed);

#endif /* HU_DAEMON_ROUTING_H */
