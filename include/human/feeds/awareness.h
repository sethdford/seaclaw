#ifndef HU_FEEDS_AWARENESS_H
#define HU_FEEDS_AWARENESS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/feeds/processor.h"
#include "human/persona.h"

#include <stdbool.h>
#include <stddef.h>

/** Conversation-ready topic derived from feeds + persona fit. */
typedef struct hu_awareness_topic {
    char text[512];              /* what to say */
    char source[256];            /* e.g. "news: …", "twitter: …" */
    double relevance;            /* 0.0–1.0 */
    char suggested_contact[128]; /* who might care (name or id hint) */
} hu_awareness_topic_t;

/**
 * Turn recent feed rows into natural bring-up topics, filtered by persona traits,
 * context awareness (sports/news), and per-contact interest hints.
 * @param out_topics allocated array; free with hu_feed_awareness_topics_free.
 */
hu_error_t hu_feed_awareness_synthesize(hu_allocator_t *alloc,
                                        const hu_feed_item_t *feed_items, size_t feed_count,
                                        const hu_persona_t *persona,
                                        hu_awareness_topic_t **out_topics, size_t *out_topic_count);

bool hu_feed_awareness_should_share(const hu_awareness_topic_t *topic,
                                    const hu_contact_profile_t *contact_profile);

void hu_feed_awareness_topics_free(hu_allocator_t *alloc, hu_awareness_topic_t *topics,
                                   size_t topic_count);

#ifdef HU_ENABLE_SQLITE
#include <sqlite3.h>

/** True if any synthesized topic is both shareable with @p cp and at least @p min_relevance. */
bool hu_feed_awareness_contact_has_high_topics(hu_allocator_t *alloc, sqlite3 *db,
                                                const hu_persona_t *persona,
                                                const hu_contact_profile_t *cp, double min_relevance);

/** Map a stored feed row to an in-memory item (borrows strings from @p stored). */
void hu_feed_awareness_item_from_stored(const hu_feed_item_stored_t *stored, hu_feed_item_t *out);
#endif

#endif /* HU_FEEDS_AWARENESS_H */
