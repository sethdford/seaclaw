#ifndef HU_MEMORY_DEGRADATION_H
#define HU_MEMORY_DEGRADATION_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --- Memory Degradation (F61) — imperfect recall simulation --- */

typedef enum hu_degradation_type {
    HU_DEGRADE_NONE = 0,        /* no degradation, perfect recall */
    HU_DEGRADE_FUZZ,            /* slightly wrong: "Tuesday"->"Wednesday" */
    HU_DEGRADE_ASK_REMIND       /* "remind me, what was the...?" */
} hu_degradation_type_t;

typedef struct hu_degradation_config {
    double perfect_rate; /* default 0.90 (90% perfect) */
    double fuzz_rate;    /* default 0.05 (5% slightly wrong) */
    double ask_rate;     /* default 0.05 (5% ask to be reminded) */
} hu_degradation_config_t;

/* Check if content is protected from degradation.
   Protected: contains commitment words ("promised", "will", "commitment"),
   emotional keywords ("love", "died", "hospital"), names (capitalized words). */
bool hu_degradation_is_protected(const char *content, size_t content_len);

/* Determine degradation type based on config and seed. */
hu_degradation_type_t hu_degradation_roll(const hu_degradation_config_t *config,
                                          uint32_t seed);

/* Apply degradation to a memory content string.
   NONE: returns copy of content.
   FUZZ: replaces day names, small numbers, time references with adjacent values.
   ASK_REMIND: returns "remind me, what was [first_few_words]?"
   Allocates result. Caller frees. */
char *hu_degradation_apply(hu_allocator_t *alloc, const char *content,
                           size_t content_len, hu_degradation_type_t type,
                           uint32_t seed, size_t *out_len);

/* Full pipeline: check protection, roll, apply.
   If protected → returns copy unchanged.
   Otherwise → rolls and applies. */
char *hu_degradation_process(hu_allocator_t *alloc, const char *content,
                            size_t content_len,
                            const hu_degradation_config_t *config, uint32_t seed,
                            size_t *out_len);

/* --- Forgetting Curve (F73) — Ebbinghaus-style memory decay --- */

typedef struct hu_forgetting_config {
    double initial_retention; /* default 1.0 */
    double stability_factor;  /* default 0.7 — higher = slower forgetting */
    double min_retention;     /* default 0.2 — never fully forget */
    uint32_t rehearsal_boost; /* how much each recall boosts stability, default 2 */
} hu_forgetting_config_t;

/* Compute retention probability using Ebbinghaus curve.
   retention = max(min_retention, e^(-t / (stability * rehearsals^boost_factor)))
   where t = hours since last recall, stability = stability_factor,
   rehearsals = number of times recalled */
double hu_forgetting_retention(double hours_since_recall, uint32_t rehearsal_count,
                               const hu_forgetting_config_t *config);

/* Determine if a memory should be recalled (passes retention check).
   Uses seed for deterministic randomness. */
bool hu_forgetting_should_recall(double hours_since_recall, uint32_t rehearsal_count,
                                const hu_forgetting_config_t *config, uint32_t seed);

/* Build SQL to create the memory_recall_log table */
hu_error_t hu_forgetting_create_table_sql(char *buf, size_t cap, size_t *out_len);

/* Build SQL to log a recall event */
hu_error_t hu_forgetting_log_recall_sql(int64_t memory_id, uint64_t recalled_at,
                                        char *buf, size_t cap, size_t *out_len);

/* Build SQL to query recall history for a memory */
hu_error_t hu_forgetting_query_recalls_sql(int64_t memory_id, char *buf, size_t cap,
                                           size_t *out_len);

/* --- F61 Memory Degradation API (seed+rate based) --- */

/* Check if content is protected from degradation. Same semantics as hu_degradation_is_protected. */
bool hu_memory_degradation_is_protected(const char *content, size_t len);

/* Apply degradation: roll = (seed % 100) / 100.0f.
   If protected → always return unchanged copy.
   If roll < (1.0 - rate) → unchanged.
   If 0.90 <= roll < 0.95 → fuzz minor details.
   If 0.95 <= roll < 1.0 → "remind me, what was that about [first 3 words]?"
   Returns allocated copy. Caller frees. */
char *hu_memory_degradation_apply(hu_allocator_t *alloc, const char *content, size_t content_len,
                                  uint32_t seed, float rate, size_t *out_len);

#endif
