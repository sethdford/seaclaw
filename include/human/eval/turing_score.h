#ifndef HU_EVAL_TURING_SCORE_H
#define HU_EVAL_TURING_SCORE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/provider.h"
#include <stddef.h>
#include <stdint.h>

#define HU_TURING_DIM_COUNT 18

typedef enum {
    /* Original 12 text-based dimensions */
    HU_TURING_NATURAL_LANGUAGE = 0,
    HU_TURING_EMOTIONAL_INTELLIGENCE,
    HU_TURING_APPROPRIATE_LENGTH,
    HU_TURING_PERSONALITY_CONSISTENCY,
    HU_TURING_VULNERABILITY_WILLINGNESS,
    HU_TURING_HUMOR_NATURALNESS,
    HU_TURING_IMPERFECTION,
    HU_TURING_OPINION_HAVING,
    HU_TURING_ENERGY_MATCHING,
    HU_TURING_CONTEXT_AWARENESS,
    HU_TURING_NON_ROBOTIC,
    HU_TURING_GENUINE_WARMTH,
    /* S2S voice dimensions (arXiv:2602.24080 taxonomy) */
    HU_TURING_PROSODY_NATURALNESS,    /* intonation, rhythm, stress patterns */
    HU_TURING_TURN_TIMING,            /* response latency, turn-taking fluency */
    HU_TURING_FILLER_USAGE,           /* natural hesitations (um, uh, like) */
    HU_TURING_EMOTIONAL_PROSODY,      /* vocal emotion expression (not just word choice) */
    HU_TURING_CONVERSATIONAL_REPAIR,  /* self-correction, rephrasing mid-utterance */
    HU_TURING_PARALINGUISTIC_CUES,    /* laughter, sighs, breath, vocal quality */
} hu_turing_dimension_t;

typedef enum {
    HU_TURING_HUMAN = 0,
    HU_TURING_BORDERLINE,
    HU_TURING_AI_DETECTED,
} hu_turing_verdict_t;

typedef struct hu_turing_score {
    int dimensions[HU_TURING_DIM_COUNT]; /* 1-10 per dimension */
    int overall;                         /* average across dimensions */
    hu_turing_verdict_t verdict;
    char ai_tells[256];
    char human_signals[256];
} hu_turing_score_t;

/* Evaluate a response for human-likeness using heuristic scoring.
 * conversation_context: recent messages for context (may be NULL).
 * response: the message being scored.
 * Returns HU_OK on success, fills out. */
hu_error_t hu_turing_score_heuristic(const char *response, size_t response_len,
                                     const char *conversation_context, size_t context_len,
                                     hu_turing_score_t *out);

/* Evaluate using an LLM judge for semantic scoring.
 * provider: the LLM to use as judge (should be cheap/fast model).
 * Returns HU_OK on success. Falls back to heuristic on LLM failure. */
hu_error_t hu_turing_score_llm(hu_allocator_t *alloc, hu_provider_t *provider,
                               const char *model, size_t model_len,
                               const char *response, size_t response_len,
                               const char *conversation_context, size_t context_len,
                               hu_turing_score_t *out);

/* Dimension name for display/logging. */
const char *hu_turing_dimension_name(hu_turing_dimension_t dim);

/* Verdict name for display/logging. */
const char *hu_turing_verdict_name(hu_turing_verdict_t verdict);

/* Format a score as a summary string. Caller owns result. */
char *hu_turing_score_summary(hu_allocator_t *alloc, const hu_turing_score_t *score,
                              size_t *out_len);

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

hu_error_t hu_turing_init_tables(sqlite3 *db);

hu_error_t hu_turing_store_score(sqlite3 *db, const char *contact_id, size_t contact_id_len,
                                 int64_t timestamp, const hu_turing_score_t *score);

#define HU_TURING_CONTACT_ID_MAX 128

hu_error_t hu_turing_get_trend(hu_allocator_t *alloc, sqlite3 *db, const char *contact_id,
                               size_t contact_id_len, size_t max_entries,
                               hu_turing_score_t *scores, int64_t *timestamps,
                               char (*out_contact_ids)[HU_TURING_CONTACT_ID_MAX],
                               size_t *out_count);

hu_error_t hu_turing_get_weakest_dimensions(sqlite3 *db, int *dimension_averages);
#endif

#endif
