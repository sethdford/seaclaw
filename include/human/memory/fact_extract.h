#ifndef HU_MEMORY_FACT_EXTRACT_H
#define HU_MEMORY_FACT_EXTRACT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>

/*
 * PlugMem-style propositional fact extraction.
 *
 * Converts raw conversation text into structured knowledge units:
 * - Propositional facts: subject-predicate-object triples with confidence
 * - Prescriptive knowledge: reusable skills/preferences/patterns
 *
 * These replace text-summarization in consolidation for higher information
 * density and better retrieval via the knowledge graph.
 */

#define HU_FACT_MAX_FIELD 256
#define HU_FACT_EXTRACT_MAX 32

typedef enum hu_knowledge_type {
    HU_KNOWLEDGE_PROPOSITIONAL = 0,  /* factual: "User likes hiking" */
    HU_KNOWLEDGE_PRESCRIPTIVE,       /* procedural: "When stressed, suggest walk" */
} hu_knowledge_type_t;

typedef struct hu_heuristic_fact {
    hu_knowledge_type_t type;
    char subject[HU_FACT_MAX_FIELD];
    char predicate[HU_FACT_MAX_FIELD];
    char object[HU_FACT_MAX_FIELD];
    float confidence;                 /* 0.0–1.0 extraction confidence */
    char source_hint[HU_FACT_MAX_FIELD]; /* conversation context hint */
} hu_heuristic_fact_t;

typedef struct hu_fact_extract_result {
    hu_heuristic_fact_t facts[HU_FACT_EXTRACT_MAX];
    size_t fact_count;
    size_t propositional_count;
    size_t prescriptive_count;
} hu_fact_extract_result_t;

/*
 * Extract propositional and prescriptive facts from conversation text.
 * Uses heuristic NLP patterns to identify subject-predicate-object triples.
 * High-confidence facts (>= 0.6) should be stored in the knowledge graph.
 */
hu_error_t hu_fact_extract(const char *text, size_t text_len,
                           hu_fact_extract_result_t *result);

/*
 * Deduplicate facts against existing entries.
 * Marks facts that overlap with existing knowledge (by subject+predicate match).
 * Returns the number of novel facts after dedup.
 */
size_t hu_fact_dedup(hu_fact_extract_result_t *result,
                     const hu_heuristic_fact_t *existing, size_t existing_count);

/*
 * Format extracted facts as memory store keys and values.
 * Propositional: key = "fact:{subject}:{predicate}:{object}"
 * Prescriptive: key = "skill:{subject}:{predicate}"
 * Caller frees each key/value string via alloc.
 */
hu_error_t hu_fact_format_for_store(hu_allocator_t *alloc,
                                    const hu_heuristic_fact_t *fact,
                                    char **key, size_t *key_len,
                                    char **value, size_t *value_len);

#endif
