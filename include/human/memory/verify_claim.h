#ifndef HU_MEMORY_VERIFY_CLAIM_H
#define HU_MEMORY_VERIFY_CLAIM_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Memory claim verification result */
typedef struct hu_claim_result {
    double confidence;   /* 0.0-1.0: how confident the claim is grounded */
    bool has_provenance; /* source episode exists */
    bool contact_match;  /* episode contact_id matches current contact */
    bool timestamp_ok;   /* timestamps are plausible */
} hu_claim_result_t;

/* Verify a memory claim against episodic store.
 * Checks: (1) matching episode exists, (2) contact_id matches, (3) timestamps plausible.
 * db must be a sqlite3* handle with the episodes table. */
hu_error_t hu_memory_verify_claim(hu_allocator_t *alloc, void *db, const char *contact_id,
                                  size_t contact_id_len, const char *claim_text,
                                  size_t claim_text_len, hu_claim_result_t *out);

/* Scan response text for memory-claim language patterns.
 * Returns true if the text contains phrases like "I remember when you...",
 * "you told me...", "last time we talked...", etc. */
bool hu_memory_has_claim_language(const char *text, size_t text_len);

/* Rewrite an unverified claim with hedged language.
 * Allocates a new string in *out. Caller must free with alloc. */
hu_error_t hu_memory_hedge_claim(hu_allocator_t *alloc, const char *text, size_t text_len,
                                 char **out, size_t *out_len);

#endif /* HU_MEMORY_VERIFY_CLAIM_H */
