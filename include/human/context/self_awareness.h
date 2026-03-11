#ifndef HU_CONTEXT_SELF_AWARENESS_H
#define HU_CONTEXT_SELF_AWARENESS_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_self_stats {
    char *contact_id;
    size_t contact_id_len;
    uint32_t messages_sent_week;
    uint32_t messages_received_week;
    uint32_t initiations_week;       /* times we started conversations */
    uint32_t their_initiations_week;
    char *last_topic;
    size_t last_topic_len;
    uint32_t topic_repeat_count;     /* consecutive times same topic */
    uint32_t days_since_contact;
} hu_self_stats_t;

typedef struct hu_reciprocity_metrics {
    double initiation_ratio;    /* our initiations / total (0-1) */
    double question_balance;    /* our questions / total questions (0-1) */
    double share_balance;      /* our shares / total shares (0-1) */
    double response_ratio;      /* % of their messages we responded to */
} hu_reciprocity_metrics_t;

/* Build SQL to create self_awareness_stats and reciprocity_scores tables */
hu_error_t hu_self_awareness_create_table_sql(char *buf, size_t cap, size_t *out_len);

/* Build SQL to upsert stats after sending a message */
hu_error_t hu_self_awareness_record_send_sql(const char *contact_id, size_t contact_id_len,
                                            bool we_initiated,
                                            const char *topic, size_t topic_len,
                                            char *buf, size_t cap, size_t *out_len);

/* Build SQL to query stats for a contact */
hu_error_t hu_self_awareness_query_sql(const char *contact_id, size_t contact_id_len,
                                      char *buf, size_t cap, size_t *out_len);

/* Compute initiation ratio from stats */
double hu_self_awareness_initiation_ratio(const hu_self_stats_t *stats);

/* Detect if we're repeating topics */
bool hu_self_awareness_topic_repeating(const hu_self_stats_t *stats, uint32_t threshold);

/* Build self-aware directive.
   "I've been kind of quiet lately" if initiation_ratio < 0.3
   "I know I keep talking about work" if topic repeating
   "haven't texted you in forever" if days_since > 7
   Returns NULL if nothing notable. Allocates *out. */
hu_error_t hu_self_awareness_build_directive(hu_allocator_t *alloc,
                                            const hu_self_stats_t *stats,
                                            char **out, size_t *out_len);

/* Compute reciprocity metrics from raw counts */
hu_reciprocity_metrics_t hu_reciprocity_compute(uint32_t our_initiations, uint32_t their_initiations,
                                                uint32_t our_questions, uint32_t their_questions,
                                                uint32_t our_shares, uint32_t their_shares,
                                                uint32_t our_responses, uint32_t their_messages);

/* Build SQL to store/update reciprocity metrics */
hu_error_t hu_reciprocity_upsert_sql(const char *contact_id, size_t contact_id_len,
                                     const hu_reciprocity_metrics_t *metrics,
                                     char *buf, size_t cap, size_t *out_len);

/* Build reciprocity adjustment directive.
   "You've been receiving more than initiating" if initiation_ratio < 0.35
   "They've asked more about you" if question_balance < 0.4
   Returns NULL if balanced. Allocates *out. */
hu_error_t hu_reciprocity_build_directive(hu_allocator_t *alloc,
                                          const hu_reciprocity_metrics_t *metrics,
                                          char **out, size_t *out_len);

void hu_self_stats_deinit(hu_allocator_t *alloc, hu_self_stats_t *stats);

#endif /* HU_CONTEXT_SELF_AWARENESS_H */
