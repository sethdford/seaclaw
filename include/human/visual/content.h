#ifndef HU_VISUAL_CONTENT_H
#define HU_VISUAL_CONTENT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_visual_type {
    HU_VISUAL_NONE = 0,
    HU_VISUAL_PHOTO,
    HU_VISUAL_LINK,
    HU_VISUAL_SCREENSHOT,
    HU_VISUAL_PHOTO_WITH_TEXT,
    HU_VISUAL_LINK_WITH_TEXT
} hu_visual_type_t;

typedef struct hu_visual_candidate {
    char *path;
    size_t path_len;
    char *description;
    size_t description_len;
    double relevance_score; /* 0.0-1.0 */
    char *sharing_context; /* "I took this yesterday" / "reminded me of..." */
    size_t sharing_context_len;
    hu_visual_type_t type;
    uint64_t captured_at;
} hu_visual_candidate_t;

typedef struct hu_visual_config {
    uint32_t scan_interval_hours;         /* default 6 */
    uint32_t max_shares_per_contact_day;  /* default 2 */
    double share_probability;             /* default 0.15 */
    uint32_t link_frequency_per_week;     /* default 3 */
} hu_visual_config_t;

/* Build SQL to create the visual_content table */
hu_error_t hu_visual_create_table_sql(char *buf, size_t cap, size_t *out_len);

/* Build SQL to insert a visual content entry */
hu_error_t hu_visual_insert_sql(const hu_visual_candidate_t *candidate,
                                const char *source, size_t source_len, char *buf, size_t cap,
                                size_t *out_len);

/* Build SQL to query recent visual content */
hu_error_t hu_visual_query_recent_sql(uint64_t since_ms, char *buf, size_t cap, size_t *out_len);

/* Build SQL to record that content was shared with a contact */
hu_error_t hu_visual_record_share_sql(int64_t content_id, const char *contact_id,
                                     size_t contact_id_len, char *buf, size_t cap,
                                     size_t *out_len);

/* Build SQL to count shares to a contact today */
hu_error_t hu_visual_count_shares_today_sql(const char *contact_id, size_t contact_id_len,
                                            uint64_t today_start_ms, char *buf, size_t cap,
                                            size_t *out_len);

/* Decide whether to include visual content in a message.
   Factors: closeness, time of day, recent shares, contact sends photos.
   Returns HU_VISUAL_NONE if no visual content should be included. */
hu_visual_type_t hu_visual_decide(double closeness, uint32_t hour_of_day, uint32_t shares_today,
                                  uint32_t max_shares_day, bool contact_sends_photos,
                                  double share_probability, uint32_t seed);

/* Score visual content relevance to a topic/conversation.
   Simple keyword overlap matching. */
double hu_visual_score_relevance(const char *description, size_t desc_len, const char *topic,
                                 size_t topic_len);

/* Build sharing context string — natural framing for the content.
   Allocates *out. Caller frees. */
hu_error_t hu_visual_build_sharing_context(hu_allocator_t *alloc, hu_visual_type_t type,
                                          const char *description, size_t desc_len, char **out,
                                          size_t *out_len);

/* Build prompt directive about available visual content.
   Allocates *out. Caller frees. */
hu_error_t hu_visual_build_prompt(hu_allocator_t *alloc,
                                  const hu_visual_candidate_t *candidates, size_t count, char **out,
                                  size_t *out_len);

const char *hu_visual_type_str(hu_visual_type_t type);

void hu_visual_candidate_deinit(hu_allocator_t *alloc, hu_visual_candidate_t *c);

#endif
