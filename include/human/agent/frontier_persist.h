#ifndef HU_AGENT_FRONTIER_PERSIST_H
#define HU_AGENT_FRONTIER_PERSIST_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

struct hu_frontier_state;
struct sqlite3;

hu_error_t hu_frontier_persist_ensure_table(struct sqlite3 *db);

hu_error_t hu_frontier_persist_save(hu_allocator_t *alloc, struct sqlite3 *db,
                                    const char *contact_id, size_t contact_id_len,
                                    const struct hu_frontier_state *state);

hu_error_t hu_frontier_persist_load(hu_allocator_t *alloc, struct sqlite3 *db,
                                    const char *contact_id, size_t contact_id_len,
                                    struct hu_frontier_state *state);

hu_error_t hu_frontier_persist_save_growth(hu_allocator_t *alloc, struct sqlite3 *db,
                                           const char *contact_id, size_t contact_id_len,
                                           const struct hu_frontier_state *state);

hu_error_t hu_frontier_persist_load_growth(hu_allocator_t *alloc, struct sqlite3 *db,
                                           const char *contact_id, size_t contact_id_len,
                                           struct hu_frontier_state *state);

hu_error_t hu_frontier_persist_save_relationship(struct sqlite3 *db, const char *contact_id,
                                                  size_t contact_id_len, int stage,
                                                  int session_count, int total_turns);
hu_error_t hu_frontier_persist_load_relationship(struct sqlite3 *db, const char *contact_id,
                                                  size_t contact_id_len, int *stage,
                                                  int *session_count, int *total_turns);

#endif
