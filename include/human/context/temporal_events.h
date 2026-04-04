#ifndef HU_CONTEXT_TEMPORAL_EVENTS_H
#define HU_CONTEXT_TEMPORAL_EVENTS_H

#include "human/context/event_extract.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

typedef struct hu_temporal_event {
    int64_t id;
    char contact_id[128];
    char description[512];
    char temporal_ref[128];
    int64_t resolved_timestamp;
    int64_t extracted_at;
    double confidence;
    bool followed_up;
} hu_temporal_event_t;

hu_error_t hu_temporal_events_init_table(sqlite3 *db);

hu_error_t hu_temporal_events_store(sqlite3 *db, const char *contact_id,
                                    size_t contact_id_len,
                                    const hu_extracted_event_t *event,
                                    int64_t resolved_ts, int64_t now_ts);

hu_error_t hu_temporal_events_store_batch(sqlite3 *db, const char *contact_id,
                                          size_t contact_id_len,
                                          const hu_event_extract_result_t *result,
                                          int64_t now_ts);

hu_error_t hu_temporal_events_get_upcoming(sqlite3 *db, hu_allocator_t *alloc,
                                           int64_t now_ts, int64_t window_secs,
                                           hu_temporal_event_t *out,
                                           size_t max_events, size_t *out_count);

hu_error_t hu_temporal_events_mark_followed_up(sqlite3 *db, int64_t event_id);

#endif /* HU_ENABLE_SQLITE */

int64_t hu_temporal_resolve_reference(const char *temporal_ref, size_t ref_len,
                                      int64_t now_ts);

#endif /* HU_CONTEXT_TEMPORAL_EVENTS_H */
