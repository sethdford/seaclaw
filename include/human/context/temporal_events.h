#ifndef HU_CONTEXT_TEMPORAL_EVENTS_H
#define HU_CONTEXT_TEMPORAL_EVENTS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>
#endif

struct hu_deep_extract_result;

typedef struct hu_temporal_event {
    int64_t id;
    char contact_id[128];
    char description[256];
    int64_t event_time;
    int followed_up;
} hu_temporal_event_t;

#ifdef HU_ENABLE_SQLITE
hu_error_t hu_temporal_events_init_table(sqlite3 *db);
hu_error_t hu_temporal_events_store_batch(sqlite3 *db, const char *contact_id,
                                          size_t contact_id_len,
                                          const struct hu_deep_extract_result *result,
                                          int64_t now);
hu_error_t hu_temporal_events_get_upcoming(sqlite3 *db, hu_allocator_t *alloc,
                                           int64_t now, int64_t window_secs,
                                           hu_temporal_event_t *out, size_t max_count,
                                           size_t *out_count);
hu_error_t hu_temporal_events_mark_followed_up(sqlite3 *db, int64_t event_id);
#endif

int64_t hu_temporal_resolve_reference(const char *text, size_t text_len, int64_t now);

#endif
