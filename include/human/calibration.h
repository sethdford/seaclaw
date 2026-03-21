#ifndef HU_CALIBRATION_H
#define HU_CALIBRATION_H

#include "human/core/allocator.h"
#include "human/core/error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hu_calibration_tod_bucket {
    HU_CALIB_TOD_MORNING = 0,   /* local 06:00–12:00 */
    HU_CALIB_TOD_AFTERNOON = 1, /* 12:00–17:00 */
    HU_CALIB_TOD_EVENING = 2,   /* 17:00–22:00 */
    HU_CALIB_TOD_NIGHT = 3,     /* 22:00–06:00 */
    HU_CALIB_TOD_BUCKET_COUNT
} hu_calibration_tod_bucket_t;

typedef struct hu_calibration_latency_percentiles {
    double p25_sec;
    double p50_sec;
    double p75_sec;
    double p95_sec;
    uint32_t sample_count;
} hu_calibration_latency_percentiles_t;

typedef struct hu_calibration_contact_latency {
    char *handle_id;
    double median_reply_sec;
    uint32_t sample_count;
} hu_calibration_contact_latency_t;

typedef struct hu_timing_report {
    hu_calibration_latency_percentiles_t by_tod[HU_CALIB_TOD_BUCKET_COUNT];
    hu_calibration_contact_latency_t *contacts;
    size_t contacts_count;
    uint32_t active_hours[24];
    /* Percentile fields are message counts per calendar day (not seconds). */
    hu_calibration_latency_percentiles_t messages_per_day;
} hu_timing_report_t;

typedef struct hu_style_phrase_stat {
    char *phrase;
    uint32_t count;
} hu_style_phrase_stat_t;

typedef struct hu_style_report {
    double avg_message_length;
    double emoji_per_message;
    double exclamation_per_message;
    double question_per_message;
    hu_style_phrase_stat_t *opening_phrases;
    size_t opening_count;
    hu_style_phrase_stat_t *closing_phrases;
    size_t closing_count;
    double vocabulary_richness;
    uint32_t messages_analyzed;
} hu_style_report_t;

void hu_timing_report_deinit(hu_allocator_t *alloc, hu_timing_report_t *report);
void hu_style_report_deinit(hu_allocator_t *alloc, hu_style_report_t *report);

hu_error_t hu_calibration_analyze_timing(hu_allocator_t *alloc, const char *db_path,
                                         const char *contact_filter, hu_timing_report_t *out_report);

hu_error_t hu_calibration_analyze_style(hu_allocator_t *alloc, const char *db_path,
                                        const char *contact_filter, hu_style_report_t *out_report);

/* Caller frees *out_recommendations with hu_str_free(alloc, *out_recommendations).
 * channel_name: overlay channel key for recommendations (e.g. "imessage", "telegram").
 * NULL means auto-detected / unspecified ("auto" in JSON). */
hu_error_t hu_calibrate(hu_allocator_t *alloc, const char *db_path, const char *contact_filter,
                        const char *channel_name, char **out_recommendations);

#endif /* HU_CALIBRATION_H */
