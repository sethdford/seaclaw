#ifndef HU_MEMORY_COMPRESSION_H
#define HU_MEMORY_COMPRESSION_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_compression_stage {
    HU_COMPRESS_FULL = 1,        /* full story referenced */
    HU_COMPRESS_ABBREVIATED = 2, /* "remember when X" */
    HU_COMPRESS_COMPRESSED = 3,  /* "like that time" */
    HU_COMPRESS_SINGLE_WORD = 4  /* just "mango" */
} hu_compression_stage_t;

typedef struct hu_shared_ref {
    int64_t id;
    char *contact_id;
    size_t contact_id_len;
    char *compressed_form; /* "the incident", "your nemesis", "mango" */
    size_t compressed_form_len;
    char *expanded_meaning; /* full shared memory this refers to */
    size_t expanded_meaning_len;
    uint32_t usage_count;
    uint64_t created_at;
    uint64_t last_used_at;
    double strength; /* 0.0-1.0: how established */
    hu_compression_stage_t stage;
} hu_shared_ref_t;

typedef struct hu_compression_config {
    uint32_t min_uses_to_compress; /* default 2 */
    uint32_t max_refs_per_contact; /* default 20 */
    double deployment_probability; /* default 0.3 */
    bool never_during_conflict;    /* default true */
    double strength_decay_rate;     /* default 0.02 per week */
    double min_strength_to_deploy; /* default 0.4 */
} hu_compression_config_t;

/* Build SQL to create the shared_references table */
hu_error_t hu_compression_create_table_sql(char *buf, size_t cap, size_t *out_len);

/* Build SQL to insert/update a shared reference */
hu_error_t hu_compression_insert_sql(const hu_shared_ref_t *ref, char *buf, size_t cap,
                                    size_t *out_len);

/* Build SQL to query references for a contact */
hu_error_t hu_compression_query_sql(const char *contact_id, size_t contact_id_len, char *buf,
                                    size_t cap, size_t *out_len);

/* Build SQL to record a usage (increment count, update last_used_at) */
hu_error_t hu_compression_record_usage_sql(int64_t ref_id, uint64_t now_ms, char *buf, size_t cap,
                                           size_t *out_len);

/* Check if a message contains any known compressed reference for a contact.
   Scans message for substrings matching known compressed_forms.
   Returns indices of matching refs via match_indices (caller provides array). */
size_t hu_compression_find_in_message(const hu_shared_ref_t *refs, size_t ref_count,
                                      const char *message, size_t msg_len,
                                      size_t *match_indices, size_t max_matches);

/* Decide whether to deploy a reference: checks strength, conflict, probability.
   Returns true if the ref should be used. seed for deterministic randomness. */
bool hu_compression_should_deploy(const hu_shared_ref_t *ref,
                                  const hu_compression_config_t *config, bool in_conflict,
                                  uint32_t seed);

/* Apply strength decay based on time since last use */
double hu_compression_decay_strength(double current_strength, uint64_t last_used_ms,
                                    uint64_t now_ms, double decay_rate_per_week);

/* Determine if a reference should advance to next compression stage */
bool hu_compression_should_advance(const hu_shared_ref_t *ref, uint32_t min_uses);

/* Build prompt context listing shared references for a contact.
   Allocates *out. Caller frees. */
hu_error_t hu_compression_build_prompt(hu_allocator_t *alloc, const hu_shared_ref_t *refs,
                                       size_t ref_count, char **out, size_t *out_len);

void hu_shared_ref_deinit(hu_allocator_t *alloc, hu_shared_ref_t *ref);

#endif
