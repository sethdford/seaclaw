#ifndef HU_AGENT_CONV_GOALS_H
#define HU_AGENT_CONV_GOALS_H
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_goal_status {
    HU_GOAL_PENDING = 0,
    HU_GOAL_IN_PROGRESS,
    HU_GOAL_ACHIEVED,
    HU_GOAL_ABANDONED,
    HU_GOAL_DEFERRED
} hu_goal_status_t;

typedef enum hu_goal_priority {
    HU_GOAL_LOW = 0,
    HU_GOAL_MEDIUM,
    HU_GOAL_HIGH,
    HU_GOAL_CRITICAL
} hu_goal_priority_t;

typedef struct hu_conv_goal {
    int64_t id;
    char *contact_id;
    size_t contact_id_len;
    char *description; /* "check on them after the breakup" */
    size_t description_len;
    char *success_signal; /* "they share how they're feeling" */
    size_t success_signal_len;
    hu_goal_status_t status;
    hu_goal_priority_t priority;
    uint64_t created_at;
    uint64_t target_by;   /* deadline: 0 = no deadline */
    uint64_t achieved_at; /* 0 if not achieved */
    uint8_t attempts;    /* how many turns we've tried */
    uint8_t max_attempts; /* give up after this many, default 5 */
} hu_conv_goal_t;

/* Build SQL to create the conversation_goals table */
hu_error_t hu_conv_goals_create_table_sql(char *buf, size_t cap, size_t *out_len);

/* Build SQL to insert a goal */
hu_error_t hu_conv_goals_insert_sql(const hu_conv_goal_t *goal, char *buf, size_t cap,
                                   size_t *out_len);

/* Build SQL to update goal status */
hu_error_t hu_conv_goals_update_status_sql(int64_t goal_id, hu_goal_status_t new_status,
                                           char *buf, size_t cap, size_t *out_len);

/* Build SQL to query active goals for a contact */
hu_error_t hu_conv_goals_query_active_sql(const char *contact_id, size_t contact_id_len,
                                          char *buf, size_t cap, size_t *out_len);

/* Check if a goal should be abandoned (exceeded max attempts or past deadline) */
bool hu_conv_goal_should_abandon(const hu_conv_goal_t *goal, uint64_t now_ms);

/* Increment attempt counter and check if we should defer (reached half of max) */
hu_error_t hu_conv_goal_record_attempt(hu_conv_goal_t *goal);

/* Score goal urgency for prioritization: combines priority, deadline proximity, attempts */
double hu_conv_goal_urgency(const hu_conv_goal_t *goal, uint64_t now_ms);

/* Build prompt context listing active goals for a conversation.
   Allocates *out. Caller frees. */
hu_error_t hu_conv_goals_build_prompt(hu_allocator_t *alloc, const hu_conv_goal_t *goals,
                                      size_t goal_count, char **out, size_t *out_len);

/* Goal status to string */
const char *hu_goal_status_str(hu_goal_status_t status);

/* String to goal status */
bool hu_goal_status_from_str(const char *str, hu_goal_status_t *out);

/* Goal priority to string */
const char *hu_goal_priority_str(hu_goal_priority_t priority);

void hu_conv_goal_deinit(hu_allocator_t *alloc, hu_conv_goal_t *goal);

#endif
