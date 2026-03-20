#include "human/calibration.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/json_util.h"

#include <stdio.h>
#include <string.h>

static void hu_calib_weighted_median_reply(const hu_timing_report_t *t, double *out_med,
                                           uint32_t *out_samples) {
    double sum = 0.0;
    uint32_t n = 0;
    for (int b = 0; b < HU_CALIB_TOD_BUCKET_COUNT; b++) {
        sum += t->by_tod[b].p50_sec * (double)t->by_tod[b].sample_count;
        n += t->by_tod[b].sample_count;
    }
    if (n == 0) {
        *out_med = 300.0;
        *out_samples = 0;
        return;
    }
    *out_med = sum / (double)n;
    *out_samples = n;
}

static const char *hu_calib_tempo_label(double median_sec) {
    if (median_sec < 60.0)
        return "within_a_minute";
    if (median_sec < 900.0)
        return "within_minutes";
    if (median_sec < 7200.0)
        return "within_an_hour_or_two";
    return "often_delayed";
}

static const char *hu_calib_emoji_label(double per_msg) {
    if (per_msg < 0.05)
        return "low";
    if (per_msg < 0.28)
        return "moderate";
    return "high";
}

static const char *hu_calib_formality_label(const hu_style_report_t *s) {
    if (s->question_per_message > 0.22 && s->exclamation_per_message > 0.15)
        return "casual";
    if (s->avg_message_length > 90.0)
        return "formal";
    if (s->avg_message_length < 28.0)
        return "casual";
    return "adaptive";
}

static hu_error_t hu_calib_build_recommendations_json(hu_allocator_t *alloc,
                                                      const hu_timing_report_t *timing,
                                                      const hu_style_report_t *style,
                                                      char **out_json) {
    if (!alloc || !timing || !style || !out_json)
        return HU_ERR_INVALID_ARGUMENT;
    *out_json = NULL;

    double wmed = 0.0;
    uint32_t tsamp = 0;
    hu_calib_weighted_median_reply(timing, &wmed, &tsamp);
    const char *tempo = hu_calib_tempo_label(wmed);
    const char *emoji = hu_calib_emoji_label(style->emoji_per_message);
    const char *formality = hu_calib_formality_label(style);

    char avg_len[32];
    int an = snprintf(avg_len, sizeof(avg_len), "%.0f", style->avg_message_length);
    if (an < 0 || (size_t)an >= sizeof(avg_len))
        return HU_ERR_INTERNAL;

    hu_json_buf_t buf;
    hu_error_t err = hu_json_buf_init(&buf, alloc);
    if (err != HU_OK)
        return err;

    err = hu_json_buf_append_raw(&buf, "{", 1);
    if (err != HU_OK)
        goto fail;

    err = hu_json_buf_append_raw(&buf, "\"recommended_overlay\":{", 23);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_value(&buf, "channel", "imessage");
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, ",", 1);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_value(&buf, "formality", formality);
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, ",", 1);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_value(&buf, "avg_length", avg_len);
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, ",", 1);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_value(&buf, "emoji_usage", emoji);
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, "}", 1);
    if (err != HU_OK)
        goto fail;

    err = hu_json_buf_append_raw(&buf, ",\"recommended_voice_rhythm\":{", 31);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_value(&buf, "response_tempo", tempo);
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, "}", 1);
    if (err != HU_OK)
        goto fail;

    err = hu_json_buf_append_raw(&buf, ",\"calibration_meta\":{", 22);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_int(&buf, "timing_weighted_median_reply_sec", (int64_t)(wmed + 0.5));
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, ",", 1);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_int(&buf, "timing_samples", (int64_t)tsamp);
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, ",", 1);
    if (err != HU_OK)
        goto fail;
    err = hu_json_util_append_key_int(&buf, "style_messages_analyzed", (int64_t)style->messages_analyzed);
    if (err != HU_OK)
        goto fail;
    err = hu_json_buf_append_raw(&buf, ",", 1);
    if (err != HU_OK)
        goto fail;
    {
        char vr[24];
        int vn = snprintf(vr, sizeof(vr), "%.3f", style->vocabulary_richness);
        if (vn < 0 || (size_t)vn >= sizeof(vr)) {
            err = HU_ERR_INTERNAL;
            goto fail;
        }
        err = hu_json_util_append_key_value(&buf, "vocabulary_richness", vr);
        if (err != HU_OK)
            goto fail;
    }
    err = hu_json_buf_append_raw(&buf, "}", 1);
    if (err != HU_OK)
        goto fail;

    if (style->opening_count > 0) {
        err = hu_json_buf_append_raw(&buf, ",\"sample_opening_phrases\":[", 28);
        if (err != HU_OK)
            goto fail;
        for (size_t i = 0; i < style->opening_count; i++) {
            if (i > 0) {
                err = hu_json_buf_append_raw(&buf, ",", 1);
                if (err != HU_OK)
                    goto fail;
            }
            err = hu_json_util_append_string(&buf, style->opening_phrases[i].phrase);
            if (err != HU_OK)
                goto fail;
        }
        err = hu_json_buf_append_raw(&buf, "]", 1);
        if (err != HU_OK)
            goto fail;
    }

    if (style->closing_count > 0) {
        err = hu_json_buf_append_raw(&buf, ",\"sample_closing_phrases\":[", 28);
        if (err != HU_OK)
            goto fail;
        for (size_t i = 0; i < style->closing_count; i++) {
            if (i > 0) {
                err = hu_json_buf_append_raw(&buf, ",", 1);
                if (err != HU_OK)
                    goto fail;
            }
            err = hu_json_util_append_string(&buf, style->closing_phrases[i].phrase);
            if (err != HU_OK)
                goto fail;
        }
        err = hu_json_buf_append_raw(&buf, "]", 1);
        if (err != HU_OK)
            goto fail;
    }

    err = hu_json_buf_append_raw(&buf, "}", 1);
    if (err != HU_OK)
        goto fail;

    *out_json = hu_strdup(alloc, buf.ptr);
    if (!*out_json)
        err = HU_ERR_OUT_OF_MEMORY;
fail:
    hu_json_buf_free(&buf);
    if (err != HU_OK)
        *out_json = NULL;
    return err;
}

hu_error_t hu_calibrate(hu_allocator_t *alloc, const char *db_path, const char *contact_filter,
                        char **out_recommendations) {
    if (!alloc || !out_recommendations)
        return HU_ERR_INVALID_ARGUMENT;
    *out_recommendations = NULL;

#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)db_path;
    (void)contact_filter;
    static const char k_mock[] =
        "{\"recommended_overlay\":{\"channel\":\"imessage\",\"formality\":\"casual\","
        "\"avg_length\":\"42\",\"emoji_usage\":\"moderate\"},"
        "\"recommended_voice_rhythm\":{\"response_tempo\":\"within_minutes\"},"
        "\"calibration_meta\":{\"timing_weighted_median_reply_sec\":180,"
        "\"timing_samples\":40,\"style_messages_analyzed\":120,\"vocabulary_richness\":\"0.620\"}}";
    *out_recommendations = hu_strdup(alloc, k_mock);
    return *out_recommendations ? HU_OK : HU_ERR_OUT_OF_MEMORY;
#else

    hu_timing_report_t timing;
    hu_style_report_t style;
    memset(&timing, 0, sizeof(timing));
    memset(&style, 0, sizeof(style));

    hu_error_t err = hu_calibration_analyze_timing(alloc, db_path, contact_filter, &timing);
    if (err != HU_OK)
        return err;

    err = hu_calibration_analyze_style(alloc, db_path, contact_filter, &style);
    if (err != HU_OK) {
        hu_timing_report_deinit(alloc, &timing);
        return err;
    }

    err = hu_calib_build_recommendations_json(alloc, &timing, &style, out_recommendations);
    hu_timing_report_deinit(alloc, &timing);
    hu_style_report_deinit(alloc, &style);
    return err;
#endif
}
