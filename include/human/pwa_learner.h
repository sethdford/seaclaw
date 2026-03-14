#ifndef HU_PWA_LEARNER_H
#define HU_PWA_LEARNER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/memory.h"
#include "human/pwa.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* PWA learner — periodically reads all PWA tabs and stores content in memory */
typedef struct hu_pwa_learner {
    hu_allocator_t *alloc;
    hu_memory_t *memory;
    hu_pwa_browser_t browser;
    bool browser_ok;
    uint32_t *content_hashes; /* per-app hash of last content */
    size_t app_count;
    size_t ingest_count; /* total items ingested */
} hu_pwa_learner_t;

hu_error_t hu_pwa_learner_init(hu_allocator_t *alloc, hu_pwa_learner_t *out,
                               hu_memory_t *memory);
void hu_pwa_learner_destroy(hu_pwa_learner_t *learner);

/* Run one scan cycle — read all PWA tabs, store new content in memory.
 * Returns number of new items ingested via *ingested_count */
hu_error_t hu_pwa_learner_scan(hu_pwa_learner_t *learner, size_t *ingested_count);

/* Store a single piece of PWA content in memory.
 * key format: "pwa:<app>:<timestamp>" */
hu_error_t hu_pwa_learner_store(hu_pwa_learner_t *learner, const char *app_name,
                                const char *content, size_t content_len);

#endif /* HU_PWA_LEARNER_H */
