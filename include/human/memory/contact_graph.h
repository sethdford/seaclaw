#ifndef HU_CONTACT_GRAPH_H
#define HU_CONTACT_GRAPH_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

typedef struct hu_contact_identity {
    char contact_id[128];        /* canonical ID */
    char display_name[256];
    char platform[64];           /* "imessage", "discord", etc. */
    char platform_handle[256];   /* phone, user ID, etc. */
    double confidence;           /* 1.0 = manual, 0.5 = inferred */
} hu_contact_identity_t;

/* Initialize contact graph tables. Requires HU_ENABLE_SQLITE. */
hu_error_t hu_contact_graph_init(hu_allocator_t *alloc, void *db);

/* Link a platform handle to a canonical contact ID. */
hu_error_t hu_contact_graph_link(void *db, const char *contact_id, const char *platform,
    const char *platform_handle, const char *display_name, double confidence);

/* Resolve: given a platform + handle, find the canonical contact ID.
 * Returns HU_OK and fills out_contact_id, or HU_ERR_NOT_FOUND. */
hu_error_t hu_contact_graph_resolve(void *db, const char *platform, const char *platform_handle,
    char *out_contact_id, size_t out_size);

/* List all identities for a canonical contact ID.
 * Caller owns the allocated array. */
hu_error_t hu_contact_graph_list(hu_allocator_t *alloc, void *db, const char *contact_id,
    hu_contact_identity_t **out, size_t *out_count);

/* Merge two canonical contact IDs (when discovered to be the same person).
 * All identities under old_id are reassigned to new_id. */
hu_error_t hu_contact_graph_merge(void *db, const char *old_id, const char *new_id);

/* Fuzzy auto-resolve: find candidate contact_ids that might match a new identity.
 * Matches by: (1) exact platform_handle on any platform, (2) normalized phone number,
 * (3) display_name similarity (case-insensitive substring). Returns candidates sorted
 * by confidence descending. Caller owns allocated array. */
hu_error_t hu_contact_graph_auto_resolve(hu_allocator_t *alloc, void *db,
    const char *display_name, const char *platform_handle,
    hu_contact_identity_t **out_candidates, size_t *out_count);

/* Normalize a phone number by stripping non-digit characters (except leading +).
 * Writes into out_buf, returns length. */
size_t hu_contact_normalize_phone(const char *input, char *out_buf, size_t out_cap);

#endif
