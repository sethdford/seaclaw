#ifndef HU_DAEMON_PROACTIVE_H
#define HU_DAEMON_PROACTIVE_H

#include "core/allocator.h"
#include "core/error.h"
#include "daemon.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct hu_agent;
struct hu_contact_profile;
struct hu_memory;
struct hu_memory_vtable;

/**
 * daemon_proactive.h — Proactive check-in subsystem extracted from daemon.c.
 *
 * Manages contact activity tracking (LRU cache), proactive route parsing,
 * prompt construction, and the main check-in orchestration loop.
 */

/* ── Contact activity LRU cache ─────────────────────────────────────── */

#define HU_DAEMON_CONTACT_ACTIVITY_CAP 256
#define HU_DAEMON_ACTIVITY_FRESH_SECS  (48 * 3600)

typedef struct hu_daemon_contact_activity {
    char contact_id[128];
    char last_channel[64];
    char last_session_key[128];
    time_t last_activity;
    uint64_t lru_seq;
} hu_daemon_contact_activity_t;

/** Record inbound activity for a contact on a given channel/session. */
void hu_daemon_contact_activity_record(const char *contact_id, const char *channel_name,
                                       const char *session_key);

/** Check if a channel name exists in the service channel list. */
bool hu_daemon_channel_list_has_name(const hu_service_channel_t *channels, size_t channel_count,
                                     const char *name);

/** Parse proactive_channel string into channel and target route buffers. */
void hu_daemon_proactive_parse_route(const struct hu_contact_profile *cp, char *ch_buf,
                                     char *target_buf);

/** Apply recent activity routing override if contact has fresh inbound activity. */
void hu_daemon_proactive_apply_route(const char *contact_id, time_t now,
                                     const hu_service_channel_t *channels, size_t channel_count,
                                     char *ch_buf, char *target_buf, size_t *target_len);

/** Build memory callback context for a contact (recalls + degradation + protective filter).
 *  Used by both proactive prompt builder and daemon main loop. */
char *hu_daemon_build_callback_context(hu_allocator_t *alloc, struct hu_memory *memory,
                                       const char *session_id, size_t session_id_len,
                                       const char *msg, size_t msg_len, size_t *out_len,
                                       struct hu_agent *agent);

/** Build proactive prompt for a contact with memory context, weather, feeds, calendar. */
char *hu_daemon_proactive_prompt_for_contact(hu_allocator_t *alloc, struct hu_agent *agent,
                                             struct hu_memory *memory,
                                             const struct hu_contact_profile *cp, size_t *out_len);

#ifdef HU_IS_TEST
/** Reset the contact activity LRU cache (for tests). */
void hu_daemon_contact_activity_reset(void);
/** Get current contact activity count (for tests). */
size_t hu_daemon_contact_activity_count(void);
#endif

#endif /* HU_DAEMON_PROACTIVE_H */
