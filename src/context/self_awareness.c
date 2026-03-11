#include "human/context/self_awareness.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define HU_SELF_AWARENESS_ESCAPE_BUF 512

static void escape_sql_string(const char *s, size_t len, char *buf, size_t cap, size_t *out_len) {
    size_t pos = 0;
    for (size_t i = 0; i < len && pos + 2 < cap; i++) {
        if (s[i] == '\'') {
            buf[pos++] = '\'';
            buf[pos++] = '\'';
        } else {
            buf[pos++] = s[i];
        }
    }
    buf[pos] = '\0';
    *out_len = pos;
}

hu_error_t hu_self_awareness_create_table_sql(char *buf, size_t cap, size_t *out_len) {
    if (!buf || !out_len || cap < 512)
        return HU_ERR_INVALID_ARGUMENT;

    static const char sql[] =
        "CREATE TABLE IF NOT EXISTS self_awareness_stats (\n"
        "    contact_id TEXT PRIMARY KEY,\n"
        "    messages_sent_week INTEGER DEFAULT 0,\n"
        "    messages_received_week INTEGER DEFAULT 0,\n"
        "    initiations_week INTEGER DEFAULT 0,\n"
        "    their_initiations_week INTEGER DEFAULT 0,\n"
        "    last_topic TEXT,\n"
        "    topic_repeat_count INTEGER DEFAULT 0,\n"
        "    days_since_contact INTEGER DEFAULT 0,\n"
        "    updated_at INTEGER\n"
        ");\n"
        "CREATE TABLE IF NOT EXISTS reciprocity_scores (\n"
        "    contact_id TEXT NOT NULL,\n"
        "    metric TEXT NOT NULL,\n"
        "    value REAL,\n"
        "    updated_at INTEGER,\n"
        "    PRIMARY KEY (contact_id, metric)\n"
        ");";
    size_t len = sizeof(sql) - 1;
    if (len >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, len + 1);
    *out_len = len;
    return HU_OK;
}

hu_error_t hu_self_awareness_record_send_sql(const char *contact_id, size_t contact_id_len,
                                             bool we_initiated,
                                             const char *topic, size_t topic_len,
                                             char *buf, size_t cap, size_t *out_len) {
    if (!contact_id || contact_id_len == 0 || !buf || !out_len || cap < 512)
        return HU_ERR_INVALID_ARGUMENT;

    char contact_esc[HU_SELF_AWARENESS_ESCAPE_BUF];
    char topic_esc[HU_SELF_AWARENESS_ESCAPE_BUF];
    size_t ce_len = 0;
    size_t te_len = 0;
    escape_sql_string(contact_id, contact_id_len, contact_esc, sizeof(contact_esc), &ce_len);
    if (topic && topic_len > 0) {
        escape_sql_string(topic, topic_len, topic_esc, sizeof(topic_esc), &te_len);
    } else {
        topic_esc[0] = '\0';
    }
    (void)ce_len;
    (void)te_len;

    int64_t now_ts = (int64_t)time(NULL);
    uint32_t init_inc = we_initiated ? 1u : 0u;

    const char *topic_sql = (topic && topic_len > 0) ? topic_esc : "";
    char topic_quoted[1024];
    if (topic && topic_len > 0) {
        int nq = snprintf(topic_quoted, sizeof(topic_quoted), "'%s'", topic_esc);
        if (nq < 0 || (size_t)nq >= sizeof(topic_quoted))
            return HU_ERR_INVALID_ARGUMENT;
        topic_sql = topic_quoted;
    }

    int n;
    if (topic && topic_len > 0) {
        n = snprintf(buf, cap,
                     "INSERT INTO self_awareness_stats (contact_id, messages_sent_week, "
                     "messages_received_week, initiations_week, their_initiations_week, "
                     "last_topic, topic_repeat_count, days_since_contact, updated_at) VALUES "
                     "('%s', 1, 0, %u, 0, %s, 1, 0, %lld) "
                     "ON CONFLICT(contact_id) DO UPDATE SET "
                     "messages_sent_week = messages_sent_week + excluded.messages_sent_week, "
                     "initiations_week = initiations_week + excluded.initiations_week, "
                     "last_topic = excluded.last_topic, "
                     "topic_repeat_count = CASE WHEN COALESCE(last_topic,'') = COALESCE(excluded.last_topic,'') "
                     "THEN topic_repeat_count + 1 ELSE 1 END, "
                     "days_since_contact = 0, "
                     "updated_at = excluded.updated_at",
                     contact_esc, init_inc, topic_sql, (long long)now_ts);
    } else {
        n = snprintf(buf, cap,
                     "INSERT INTO self_awareness_stats (contact_id, messages_sent_week, "
                     "messages_received_week, initiations_week, their_initiations_week, "
                     "last_topic, topic_repeat_count, days_since_contact, updated_at) VALUES "
                     "('%s', 1, 0, %u, 0, NULL, 1, 0, %lld) "
                     "ON CONFLICT(contact_id) DO UPDATE SET "
                     "messages_sent_week = messages_sent_week + excluded.messages_sent_week, "
                     "initiations_week = initiations_week + excluded.initiations_week, "
                     "last_topic = excluded.last_topic, "
                     "topic_repeat_count = CASE WHEN COALESCE(last_topic,'') = COALESCE(excluded.last_topic,'') "
                     "THEN topic_repeat_count + 1 ELSE 1 END, "
                     "days_since_contact = 0, "
                     "updated_at = excluded.updated_at",
                     contact_esc, init_inc, (long long)now_ts);
    }
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_self_awareness_query_sql(const char *contact_id, size_t contact_id_len,
                                       char *buf, size_t cap, size_t *out_len) {
    if (!contact_id || contact_id_len == 0 || !buf || !out_len || cap < 128)
        return HU_ERR_INVALID_ARGUMENT;

    char contact_esc[HU_SELF_AWARENESS_ESCAPE_BUF];
    size_t ce_len;
    escape_sql_string(contact_id, contact_id_len, contact_esc, sizeof(contact_esc), &ce_len);

    int n = snprintf(buf, cap,
                     "SELECT contact_id, messages_sent_week, messages_received_week, "
                     "initiations_week, their_initiations_week, last_topic, topic_repeat_count, "
                     "days_since_contact, updated_at FROM self_awareness_stats WHERE contact_id = '%s'",
                     contact_esc);
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

double hu_self_awareness_initiation_ratio(const hu_self_stats_t *stats) {
    if (!stats)
        return 0.5;
    uint32_t total = stats->initiations_week + stats->their_initiations_week;
    if (total == 0)
        return 0.5;
    return (double)stats->initiations_week / (double)total;
}

bool hu_self_awareness_topic_repeating(const hu_self_stats_t *stats, uint32_t threshold) {
    if (!stats)
        return false;
    return stats->topic_repeat_count >= threshold;
}

hu_error_t hu_self_awareness_build_directive(hu_allocator_t *alloc,
                                            const hu_self_stats_t *stats,
                                            char **out, size_t *out_len) {
    if (!alloc || !stats || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    double ratio = hu_self_awareness_initiation_ratio(stats);
    if (ratio < 0.3) {
        char *s = hu_strndup(alloc, "I've been kind of quiet with them — should probably reach out more"
                         , 56);
        if (!s)
            return HU_ERR_OUT_OF_MEMORY;
        *out = s;
        *out_len = 56;
        return HU_OK;
    }
    if (stats->topic_repeat_count >= 3 && stats->last_topic && stats->last_topic_len > 0) {
        char buf[256];
        int n = snprintf(buf, sizeof(buf), "I keep bringing up %.*s — should talk about something else",
                        (int)stats->last_topic_len, stats->last_topic);
        if (n < 0 || (size_t)n >= sizeof(buf))
            return HU_ERR_INVALID_ARGUMENT;
        char *s = hu_strndup(alloc, buf, (size_t)n);
        if (!s)
            return HU_ERR_OUT_OF_MEMORY;
        *out = s;
        *out_len = (size_t)n;
        return HU_OK;
    }
    if (stats->days_since_contact > 7) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "Haven't talked to them in a while — it's been %u days",
                        stats->days_since_contact);
        if (n < 0 || (size_t)n >= sizeof(buf))
            return HU_ERR_INVALID_ARGUMENT;
        char *s = hu_strndup(alloc, buf, (size_t)n);
        if (!s)
            return HU_ERR_OUT_OF_MEMORY;
        *out = s;
        *out_len = (size_t)n;
        return HU_OK;
    }
    return HU_OK;
}

hu_reciprocity_metrics_t hu_reciprocity_compute(uint32_t our_initiations, uint32_t their_initiations,
                                                uint32_t our_questions, uint32_t their_questions,
                                                uint32_t our_shares, uint32_t their_shares,
                                                uint32_t our_responses, uint32_t their_messages) {
    hu_reciprocity_metrics_t m = {0};
    uint32_t total_init = our_initiations + their_initiations;
    m.initiation_ratio = (total_init > 0) ? (double)our_initiations / (double)total_init : 0.5;

    uint32_t total_q = our_questions + their_questions;
    m.question_balance = (total_q > 0) ? (double)our_questions / (double)total_q : 0.5;

    uint32_t total_s = our_shares + their_shares;
    m.share_balance = (total_s > 0) ? (double)our_shares / (double)total_s : 0.5;

    m.response_ratio = (their_messages > 0) ? (double)our_responses / (double)their_messages : 0.0;
    return m;
}

hu_error_t hu_reciprocity_upsert_sql(const char *contact_id, size_t contact_id_len,
                                     const hu_reciprocity_metrics_t *metrics,
                                     char *buf, size_t cap, size_t *out_len) {
    if (!contact_id || contact_id_len == 0 || !metrics || !buf || !out_len || cap < 512)
        return HU_ERR_INVALID_ARGUMENT;

    char contact_esc[HU_SELF_AWARENESS_ESCAPE_BUF];
    size_t ce_len;
    escape_sql_string(contact_id, contact_id_len, contact_esc, sizeof(contact_esc), &ce_len);

    int64_t now_ts = (int64_t)time(NULL);

    int n = snprintf(buf, cap,
                     "INSERT OR REPLACE INTO reciprocity_scores (contact_id, metric, value, updated_at) "
                     "VALUES ('%s', 'initiation_ratio', %f, %lld); "
                     "INSERT OR REPLACE INTO reciprocity_scores (contact_id, metric, value, updated_at) "
                     "VALUES ('%s', 'question_balance', %f, %lld); "
                     "INSERT OR REPLACE INTO reciprocity_scores (contact_id, metric, value, updated_at) "
                     "VALUES ('%s', 'share_balance', %f, %lld); "
                     "INSERT OR REPLACE INTO reciprocity_scores (contact_id, metric, value, updated_at) "
                     "VALUES ('%s', 'response_ratio', %f, %lld)",
                     contact_esc, metrics->initiation_ratio, (long long)now_ts,
                     contact_esc, metrics->question_balance, (long long)now_ts,
                     contact_esc, metrics->share_balance, (long long)now_ts,
                     contact_esc, metrics->response_ratio, (long long)now_ts);
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_reciprocity_build_directive(hu_allocator_t *alloc,
                                         const hu_reciprocity_metrics_t *metrics,
                                         char **out, size_t *out_len) {
    if (!alloc || !metrics || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

    char parts[4][512];
    size_t part_count = 0;

    if (metrics->initiation_ratio < 0.35) {
        snprintf(parts[part_count], sizeof(parts[part_count]),
                 "You've been receiving more than initiating lately. Consider starting a conversation.");
        part_count++;
    }
    if (metrics->question_balance < 0.4) {
        snprintf(parts[part_count], sizeof(parts[part_count]),
                 "They've been asking more about you than you about them. Ask about their life.");
        part_count++;
    }
    if (metrics->share_balance > 0.65) {
        snprintf(parts[part_count], sizeof(parts[part_count]),
                 "You've been sharing a lot — give them space to share.");
        part_count++;
    }

    if (part_count == 0)
        return HU_OK;

    char combined[1024];
    size_t pos = 0;
    for (size_t i = 0; i < part_count && pos < sizeof(combined) - 1; i++) {
        if (i > 0) {
            combined[pos++] = ' ';
            combined[pos++] = ' ';
        }
        size_t plen = strlen(parts[i]);
        if (pos + plen >= sizeof(combined))
            plen = sizeof(combined) - pos - 1;
        memcpy(combined + pos, parts[i], plen);
        pos += plen;
    }
    combined[pos] = '\0';

    char *s = hu_strndup(alloc, combined, pos);
    if (!s)
        return HU_ERR_OUT_OF_MEMORY;
    *out = s;
    *out_len = pos;
    return HU_OK;
}

void hu_self_stats_deinit(hu_allocator_t *alloc, hu_self_stats_t *stats) {
    if (!alloc || !stats)
        return;
    hu_str_free(alloc, stats->contact_id);
    stats->contact_id = NULL;
    stats->contact_id_len = 0;
    hu_str_free(alloc, stats->last_topic);
    stats->last_topic = NULL;
    stats->last_topic_len = 0;
}
