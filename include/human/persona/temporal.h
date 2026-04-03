#ifndef HU_TEMPORAL_H
#define HU_TEMPORAL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_season {
    HU_SEASON_SPRING,
    HU_SEASON_SUMMER,
    HU_SEASON_AUTUMN,
    HU_SEASON_WINTER,
} hu_season_t;

typedef struct hu_anniversary {
    const char *label; /* borrowed; e.g. "birthday", "work anniversary" */
    size_t label_len;
    int month;
    int day;
    int days_away; /* 0 = today, 1 = tomorrow, etc. */
} hu_anniversary_t;

typedef enum hu_life_transition {
    HU_TRANSITION_NONE = 0,
    HU_TRANSITION_JOB_CHANGE,
    HU_TRANSITION_MOVE,
    HU_TRANSITION_BREAKUP,
    HU_TRANSITION_NEW_BABY,
    HU_TRANSITION_GRADUATION,
    HU_TRANSITION_RETIREMENT,
    HU_TRANSITION_HEALTH_EVENT,
    HU_TRANSITION_LOSS,
} hu_life_transition_t;

/* Simple message struct for scanning */
typedef struct hu_temporal_message {
    const char *text;
    size_t text_len;
} hu_temporal_message_t;

/* Season from month (1-12) */
hu_season_t hu_temporal_season(int month);
const char *hu_temporal_season_name(hu_season_t season);

/* Check for upcoming anniversaries within window_days of (month, day).
 * dates is array of (label, month, day) pairs; returns matching entries. */
typedef struct hu_date_entry {
    const char *label;
    size_t label_len;
    int month;
    int day;
} hu_date_entry_t;

size_t hu_temporal_check_anniversaries(const hu_date_entry_t *dates, size_t date_count,
                                       int current_year, int current_month, int current_day,
                                       int window_days, hu_anniversary_t *out, size_t out_cap);

/* Detect life transitions from recent messages */
hu_life_transition_t hu_temporal_detect_life_transition(const hu_temporal_message_t *messages,
                                                        size_t count);

/* Build a temporal context directive string */
hu_error_t hu_temporal_build_context(hu_allocator_t *alloc, int month, int day,
                                     const hu_anniversary_t *anniversaries, size_t ann_count,
                                     hu_life_transition_t transition, char **out, size_t *out_len);

#endif /* HU_TEMPORAL_H */
