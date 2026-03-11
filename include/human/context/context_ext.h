#ifndef HU_CONTEXT_EXT_H
#define HU_CONTEXT_EXT_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --- F47: Content Forwarding --- */
typedef struct hu_shareable_content {
    char *content;
    size_t content_len;
    char *source;
    size_t source_len; /* "contact:alice", "web", "rss" */
    char *topic;
    size_t topic_len;
    uint64_t received_at;
    double share_score;
} hu_shareable_content_t;

hu_error_t hu_forwarding_create_table_sql(char *buf, size_t cap, size_t *out_len);
hu_error_t hu_forwarding_insert_sql(const hu_shareable_content_t *c, char *buf, size_t cap,
                                   size_t *out_len);
hu_error_t hu_forwarding_query_for_contact_sql(const char *contact_id, size_t len, char *buf,
                                               size_t cap, size_t *out_len);
double hu_forwarding_score(bool topic_match, double contact_closeness,
                          uint32_t hours_since_received, bool already_shared);
void hu_shareable_content_deinit(hu_allocator_t *alloc, hu_shareable_content_t *c);

/* --- F51: Weather Context --- */
typedef struct hu_weather_state {
    char *condition;
    size_t condition_len; /* "sunny", "raining", "snowing" */
    double temp_f;
    char *location;
    size_t location_len;
    bool is_notable; /* extreme temp, storm, first nice day */
} hu_weather_state_t;

hu_error_t hu_weather_build_directive(hu_allocator_t *alloc, const hu_weather_state_t *w,
                                     char **out, size_t *out_len);
bool hu_weather_is_notable(const char *condition, size_t len, double temp_f);
void hu_weather_state_deinit(hu_allocator_t *alloc, hu_weather_state_t *w);

/* --- F52: Current Events --- */
typedef struct hu_current_event {
    char *topic;
    size_t topic_len;
    char *summary;
    size_t summary_len;
    char *source;
    size_t source_len;
    uint64_t published_at;
    double relevance;
} hu_current_event_t;

hu_error_t hu_events_create_table_sql(char *buf, size_t cap, size_t *out_len);
hu_error_t hu_events_insert_sql(const hu_current_event_t *e, char *buf, size_t cap,
                               size_t *out_len);
hu_error_t hu_events_build_prompt(hu_allocator_t *alloc, const hu_current_event_t *events,
                                 size_t count, char **out, size_t *out_len);
void hu_current_event_deinit(hu_allocator_t *alloc, hu_current_event_t *e);

/* --- F55-F57: Group Chat --- */
typedef struct hu_group_chat_state {
    uint32_t total_messages;     /* messages in last hour */
    uint32_t our_messages;       /* our messages in last hour */
    double response_rate;        /* configured from persona */
    bool was_mentioned;          /* @ mentioned */
    bool has_direct_question;    /* question directed at us */
    uint32_t active_participants;
} hu_group_chat_state_t;

bool hu_group_should_respond(const hu_group_chat_state_t *state, uint32_t seed);
bool hu_group_should_mention(const char *message, size_t msg_len, const char *contact_name,
                            size_t name_len);
hu_error_t hu_group_build_directive(hu_allocator_t *alloc,
                                    const hu_group_chat_state_t *state, char **out,
                                    size_t *out_len);

#endif /* HU_CONTEXT_EXT_H */
