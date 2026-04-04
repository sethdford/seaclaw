#ifndef HU_MEMORY_HALLUCINATION_GUARD_H
#define HU_MEMORY_HALLUCINATION_GUARD_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Memory hallucination grounding — MARCH-style claim verification.
 *
 * Before presenting "I remember when..." claims, decompose the response into
 * atomic memory propositions, verify each against stored memory, and strip or
 * hedge unverifiable claims. Prevents the #1 trust-destroying failure mode
 * for a digital twin: fabricating shared experiences.
 *
 * Pipeline: response text → extract claims → verify each → rewrite unverified.
 */

#define HU_HALLUCINATION_MAX_CLAIMS 16
#define HU_HALLUCINATION_CLAIM_MAX_LEN 512

typedef enum hu_claim_status {
    HU_CLAIM_VERIFIED = 0,
    HU_CLAIM_UNVERIFIED,
    HU_CLAIM_CONTRADICTED,
} hu_claim_status_t;

typedef struct hu_memory_claim {
    char text[HU_HALLUCINATION_CLAIM_MAX_LEN];
    size_t text_len;
    hu_claim_status_t status;
    float confidence;
} hu_memory_claim_t;

typedef struct hu_hallucination_result {
    hu_memory_claim_t claims[HU_HALLUCINATION_MAX_CLAIMS];
    size_t claim_count;
    size_t verified_count;
    size_t unverified_count;
    size_t contradicted_count;
    bool needs_rewrite;
} hu_hallucination_result_t;

/*
 * Extract memory claims from a response.
 * Scans for patterns like "I remember", "you told me", "we talked about",
 * "last time you", "you mentioned", etc. and isolates the claim text.
 */
hu_error_t hu_hallucination_extract_claims(const char *response, size_t response_len,
                                           hu_hallucination_result_t *result);

/*
 * Verify extracted claims against stored memory.
 * For each claim, attempts recall from the memory backend.
 * Sets claim status to VERIFIED if a matching memory exists,
 * CONTRADICTED if memory contradicts, UNVERIFIED if no evidence.
 */
hu_error_t hu_hallucination_verify_claims(hu_hallucination_result_t *result,
                                          hu_memory_t *memory,
                                          hu_allocator_t *alloc);

/*
 * Rewrite unverified claims in a response with hedging language.
 * Replaces "I remember when you..." with "I think you might have mentioned..."
 * for UNVERIFIED claims, and removes CONTRADICTED claims entirely.
 * Returns a new allocated string via out; caller must free.
 */
hu_error_t hu_hallucination_rewrite(hu_allocator_t *alloc,
                                    const char *response, size_t response_len,
                                    const hu_hallucination_result_t *result,
                                    char **out, size_t *out_len);

/*
 * Full pipeline: extract → verify → rewrite if needed.
 * If no claims found or all verified, *out == NULL (use original response).
 * Caller frees *out when non-NULL.
 */
hu_error_t hu_hallucination_guard(hu_allocator_t *alloc,
                                  const char *response, size_t response_len,
                                  hu_memory_t *memory,
                                  char **out, size_t *out_len);

#endif
