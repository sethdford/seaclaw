#ifndef HU_MEMORY_RELATIONAL_EPISODE_H
#define HU_MEMORY_RELATIONAL_EPISODE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>

#define HU_RELATIONAL_EPISODE_MAX_TAGS 4

typedef struct hu_relational_episode {
    char *contact_id;
    char *summary;
    char *felt_sense;
    char *relational_meaning;
    float significance;
    float warmth;
    uint64_t timestamp;
    char *tags[HU_RELATIONAL_EPISODE_MAX_TAGS];
    size_t tag_count;
} hu_relational_episode_t;

void hu_relational_episode_init(hu_relational_episode_t *ep);
void hu_relational_episode_free(hu_allocator_t *alloc, hu_relational_episode_t *ep);

hu_error_t hu_relational_episode_set(hu_allocator_t *alloc, hu_relational_episode_t *ep,
                                     const char *contact_id, const char *summary,
                                     const char *felt_sense, const char *relational_meaning,
                                     float significance, float warmth, uint64_t timestamp);

hu_error_t hu_relational_episode_add_tag(hu_allocator_t *alloc, hu_relational_episode_t *ep,
                                         const char *tag);

hu_error_t hu_relational_episode_build_context(hu_allocator_t *alloc,
                                               const hu_relational_episode_t *episodes,
                                               size_t count, char **out, size_t *out_len);

hu_error_t hu_relational_episode_create_table_sql(char *buf, size_t cap, size_t *out_len);

hu_error_t hu_relational_episode_insert_sql(const hu_relational_episode_t *ep,
                                            char *buf, size_t cap, size_t *out_len);

#endif
