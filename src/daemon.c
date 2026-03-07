#include "seaclaw/daemon.h"
#include "seaclaw/agent.h"
#include "seaclaw/config.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/process_util.h"
#include "seaclaw/core/string.h"
#ifdef SC_HAS_CRON
#include "seaclaw/cron.h"
#include "seaclaw/crontab.h"
#endif
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__APPLE__) && defined(__MACH__) && defined(SC_ENABLE_SQLITE) && !defined(SC_IS_TEST)
#include <sqlite3.h>
#endif

#if !defined(_WIN32) && !defined(__CYGWIN__)
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define SC_DAEMON_PID_DIR  ".seaclaw"
#define SC_DAEMON_PID_FILE "seaclaw.pid"
#define SC_MAX_PATH        1024

/* GCC's warn_unused_result is not suppressed by (void) casts */
#define SC_IGNORE_RESULT(expr)   \
    do {                         \
        if ((expr)) { /* noop */ \
        }                        \
    } while (0)

static sc_error_t validate_home(const char *home) {
    if (!home || !home[0]) {
        fprintf(stderr, "HOME not set\n");
        return SC_ERR_INVALID_ARGUMENT;
    }
    for (const char *p = home; *p; p++) {
        char c = *p;
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9') &&
            c != '/' && c != '.' && c != '_' && c != '-' && c != ' ') {
            fprintf(stderr, "HOME contains unsafe characters\n");
            return SC_ERR_INVALID_ARGUMENT;
        }
    }
    return SC_OK;
}

static int get_pid_path(char *buf, size_t buf_size) {
    const char *home = getenv("HOME");
    if (!home)
        home = ".";
    if (validate_home(home) != SC_OK)
        return -1;
    return snprintf(buf, buf_size, "%s/%s/%s", home, SC_DAEMON_PID_DIR, SC_DAEMON_PID_FILE);
}

/* ── Cron field parsing and tick (only when SC_HAS_CRON) ────────────── */
#ifdef SC_HAS_CRON

static bool parse_cron_int(const char *s, int *out) {
    if (!s || !*s)
        return false;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0')
        return false;
    if (v < 0 || v > 999)
        return false;
    *out = (int)v;
    return true;
}

bool cron_atom_matches(const char *atom, size_t len, int value) {
    if (len == 0)
        return false;

    char buf[32];
    if (len >= sizeof(buf))
        return false;
    memcpy(buf, atom, len);
    buf[len] = '\0';

    char *slash = strchr(buf, '/');
    int step = 0;
    if (slash) {
        *slash = '\0';
        if (!parse_cron_int(slash + 1, &step) || step <= 0)
            return false;
    }

    char *dash = strchr(buf, '-');
    if (buf[0] == '*' && buf[1] == '\0') {
        return step > 0 ? (value % step == 0) : true;
    }

    if (dash && dash != buf) {
        *dash = '\0';
        int lo, hi;
        if (!parse_cron_int(buf, &lo) || !parse_cron_int(dash + 1, &hi))
            return false;
        if (value < lo || value > hi)
            return false;
        return step > 0 ? ((value - lo) % step == 0) : true;
    }

    int exact;
    if (!parse_cron_int(buf, &exact))
        return false;
    return exact == value;
}

bool cron_field_matches(const char *field, int value) {
    if (!field)
        return false;
    if (field[0] == '*' && field[1] == '\0')
        return true;

    const char *p = field;
    while (*p) {
        const char *start = p;
        while (*p && *p != ',')
            p++;
        size_t len = (size_t)(p - start);
        if (cron_atom_matches(start, len, value))
            return true;
        if (*p == ',')
            p++;
    }
    return false;
}

bool sc_cron_schedule_matches(const char *schedule, const struct tm *tm) {
    if (!schedule || !tm)
        return false;
    char buf[128];
    size_t slen = strlen(schedule);
    if (slen >= sizeof(buf))
        return false;
    memcpy(buf, schedule, slen + 1);

    char *fields[5] = {0};
    size_t fi = 0;
    char *tok = buf;
    while (fi < 5 && *tok) {
        while (*tok == ' ')
            tok++;
        if (!*tok)
            break;
        fields[fi++] = tok;
        while (*tok && *tok != ' ')
            tok++;
        if (*tok)
            *tok++ = '\0';
    }
    if (fi < 5)
        return false;

    return cron_field_matches(fields[0], tm->tm_min) &&
           cron_field_matches(fields[1], tm->tm_hour) &&
           cron_field_matches(fields[2], tm->tm_mday) &&
           cron_field_matches(fields[3], tm->tm_mon + 1) &&
           cron_field_matches(fields[4], tm->tm_wday);
}

/* ── Cron tick execution ─────────────────────────────────────────────── */

static void run_cron_tick(sc_allocator_t *alloc) {
    char *cron_path = NULL;
    size_t cron_path_len = 0;
    if (sc_crontab_get_path(alloc, &cron_path, &cron_path_len) != SC_OK)
        return;

    sc_crontab_entry_t *entries = NULL;
    size_t count = 0;
    if (sc_crontab_load(alloc, cron_path, &entries, &count) != SC_OK || count == 0) {
        alloc->free(alloc->ctx, cron_path, cron_path_len + 1);
        if (entries)
            sc_crontab_entries_free(alloc, entries, count);
        return;
    }

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    for (size_t i = 0; i < count; i++) {
        if (!entries[i].enabled || !entries[i].command)
            continue;
        if (!sc_cron_schedule_matches(entries[i].schedule, &tm))
            continue;

#ifndef SC_IS_TEST
        const char *argv[] = {"/bin/sh", "-c", entries[i].command, NULL};
        sc_run_result_t result = {0};
        sc_error_t run_err = sc_process_run(alloc, argv, NULL, 65536, &result);
        if (run_err != SC_OK)
            fprintf(stderr, "[seaclaw] cron job failed: %s (err=%d)\n", entries[i].command,
                    (int)run_err);
        sc_run_result_free(alloc, &result);
#endif
    }

    sc_crontab_entries_free(alloc, entries, count);
    alloc->free(alloc->ctx, cron_path, cron_path_len + 1);
}

sc_error_t sc_service_run_agent_cron(sc_allocator_t *alloc, sc_agent_t *agent,
                                     sc_service_channel_t *channels, size_t channel_count) {
    if (!alloc || !agent || !agent->scheduler)
        return SC_ERR_INVALID_ARGUMENT;

    size_t job_count = 0;
    const sc_cron_job_t *jobs = sc_cron_list_jobs(agent->scheduler, &job_count);
    if (!jobs || job_count == 0)
        return SC_OK;

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    for (size_t i = 0; i < job_count; i++) {
        if (jobs[i].type != SC_CRON_JOB_AGENT || !jobs[i].enabled || jobs[i].paused)
            continue;
        if (!jobs[i].command || !jobs[i].expression)
            continue;
        if (!sc_cron_schedule_matches(jobs[i].expression, &tm))
            continue;

        const char *prompt = jobs[i].command;
        size_t prompt_len = strlen(prompt);
        const char *target_channel = jobs[i].channel;

        if (target_channel && channels) {
            agent->active_channel = target_channel;
            agent->active_channel_len = strlen(target_channel);
        }
        agent->active_job_id = jobs[i].id;

        char *response = NULL;
        size_t response_len = 0;
#ifndef SC_IS_TEST
        sc_error_t err = sc_agent_turn(agent, prompt, prompt_len, &response, &response_len);
#else
        (void)prompt_len;
        sc_error_t err = SC_OK;
        response = sc_strndup(alloc, "[agent-cron-test]", 17);
        response_len = 17;
#endif

        agent->active_job_id = 0;
        if (err == SC_OK && response && response_len > 0 && target_channel && channels) {
            for (size_t c = 0; c < channel_count; c++) {
                if (!channels[c].channel || !channels[c].channel->vtable ||
                    !channels[c].channel->vtable->name)
                    continue;
                const char *ch_name = channels[c].channel->vtable->name(channels[c].channel->ctx);
                if (ch_name && strcmp(ch_name, target_channel) == 0) {
                    if (channels[c].channel->vtable->send) {
                        (void)channels[c].channel->vtable->send(channels[c].channel->ctx, NULL, 0,
                                                                response, response_len, NULL, 0);
                    }
                    break;
                }
            }
        }

        (void)sc_cron_add_run(agent->scheduler, alloc, jobs[i].id, (int64_t)now,
                              err == SC_OK ? "success" : "failed", response);

        if (response)
            alloc->free(alloc->ctx, response, response_len + 1);
        sc_agent_clear_history(agent);
    }
    return SC_OK;
}

#endif /* SC_HAS_CRON */

/* ── Signal handling (non-test only) ─────────────────────────────────── */

#if !defined(SC_IS_TEST) && !defined(_WIN32) && !defined(__CYGWIN__)
static volatile sig_atomic_t g_stop_flag = 0;

static void service_signal_handler(int sig) {
    (void)sig;
    g_stop_flag = 1;
}
#endif

/* ── Streaming callback for channels with send_event ─────────────────────── */

/* ---- iMessage conversation history loader ---- */

#if defined(__APPLE__) && defined(__MACH__) && defined(SC_ENABLE_SQLITE) && !defined(SC_IS_TEST)
static char *load_imessage_context(sc_allocator_t *alloc, const char *handle, size_t *out_len) {
    *out_len = 0;
    const char *home = getenv("HOME");
    if (!home)
        return NULL;

    char db_path[512];
    int n = snprintf(db_path, sizeof(db_path), "%s/Library/Messages/chat.db", home);
    if (n < 0 || (size_t)n >= sizeof(db_path))
        return NULL;

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return NULL;
    }

    const char *sql = "SELECT m.is_from_me, m.text, "
                      "  datetime(m.date/1000000000 + 978307200, 'unixepoch', 'localtime') as ts "
                      "FROM message m "
                      "JOIN handle h ON m.handle_id = h.ROWID "
                      "WHERE h.id = ?1 AND m.associated_message_type = 0 "
                      "ORDER BY m.date DESC LIMIT 25";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }
    sqlite3_bind_text(stmt, 1, handle, -1, NULL);

    typedef struct {
        int from_me;
        char text[512];
        char ts[32];
    } msg_row_t;
    msg_row_t rows[25];
    size_t row_count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW && row_count < 25) {
        rows[row_count].from_me = sqlite3_column_int(stmt, 0);
        const char *txt = (const char *)sqlite3_column_text(stmt, 1);
        const char *ts = (const char *)sqlite3_column_text(stmt, 2);
        if (txt && strlen(txt) > 0) {
            size_t tlen = strlen(txt);
            if (tlen >= sizeof(rows[0].text))
                tlen = sizeof(rows[0].text) - 1;
            memcpy(rows[row_count].text, txt, tlen);
            rows[row_count].text[tlen] = '\0';
        } else if (rows[row_count].from_me) {
            snprintf(rows[row_count].text, sizeof(rows[0].text), "[you replied]");
        } else {
            snprintf(rows[row_count].text, sizeof(rows[0].text), "[image or attachment]");
        }
        if (ts) {
            size_t tslen = strlen(ts);
            if (tslen >= sizeof(rows[0].ts))
                tslen = sizeof(rows[0].ts) - 1;
            memcpy(rows[row_count].ts, ts, tslen);
            rows[row_count].ts[tslen] = '\0';
        } else {
            rows[row_count].ts[0] = '\0';
        }
        row_count++;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (row_count == 0)
        return NULL;

    size_t buf_cap = 16384;
    char *buf = (char *)alloc->alloc(alloc->ctx, buf_cap);
    if (!buf)
        return NULL;

    size_t pos = 0;
    int w = snprintf(buf + pos, buf_cap - pos, "\n--- Recent conversation thread ---\n");
    if (w > 0)
        pos += (size_t)w;

    for (size_t i = row_count; i > 0; i--) {
        msg_row_t *r = &rows[i - 1];
        const char *who = r->from_me ? "You" : "Them";
        w = snprintf(buf + pos, buf_cap - pos, "[%s] %s: %s\n", r->ts, who, r->text);
        if (w > 0 && pos + (size_t)w < buf_cap)
            pos += (size_t)w;
    }

    w = snprintf(buf + pos, buf_cap - pos, "--- End of recent thread ---\n\n");
    if (w > 0)
        pos += (size_t)w;

    /*
     * Analyze conversation state: extract emotional tone, active topics,
     * and relationship dynamics from the thread. This gives the model
     * structured awareness beyond raw messages.
     */
    {
        /* Detect if last message from them was emotional */
        bool they_seem_frustrated = false;
        bool they_seem_excited = false;
        bool they_seem_sad = false;
        bool open_question = false;
        bool they_sent_link = false;
        bool logistics_thread = false;
        const char *last_them_msg = NULL;

        for (size_t i = 0; i < row_count; i++) {
            /* rows[0] is most recent */
            if (!rows[i].from_me) {
                if (!last_them_msg)
                    last_them_msg = rows[i].text;
                char *t = rows[i].text;
                size_t tl = strlen(t);
                for (size_t j = 0; j < tl; j++) {
                    if (t[j] == '?' && !rows[i].from_me)
                        open_question = true;
                    if (t[j] == '!' && tl > 3)
                        they_seem_excited = true;
                }
                /* Check for frustration words */
                for (size_t j = 0; j + 3 < tl; j++) {
                    char a = t[j];
                    if (a >= 'A' && a <= 'Z')
                        a += 32;
                    char b = t[j + 1];
                    if (b >= 'A' && b <= 'Z')
                        b += 32;
                    char c = t[j + 2];
                    if (c >= 'A' && c <= 'Z')
                        c += 32;
                    char d = t[j + 3];
                    if (d >= 'A' && d <= 'Z')
                        d += 32;
                    if (a == 'd' && b == 'a' && c == 'm' && d == 'n')
                        they_seem_frustrated = true;
                    if (a == 'u' && b == 'g' && c == 'h')
                        they_seem_frustrated = true;
                }
                /* Check for link */
                for (size_t j = 0; j + 4 < tl; j++) {
                    if (memcmp(t + j, "http", 4) == 0)
                        they_sent_link = true;
                }
            }
            /* Check for logistics keywords */
            char *t2 = rows[i].text;
            size_t t2l = strlen(t2);
            for (size_t j = 0; j + 5 < t2l; j++) {
                char lo[6];
                for (int k = 0; k < 5; k++) {
                    lo[k] = t2[j + k];
                    if (lo[k] >= 'A' && lo[k] <= 'Z')
                        lo[k] += 32;
                }
                lo[5] = 0;
                if (strcmp(lo, "fligh") == 0 || strcmp(lo, "airpo") == 0 ||
                    strcmp(lo, "leavi") == 0 || strcmp(lo, "booke") == 0 ||
                    strcmp(lo, "arriv") == 0 || strcmp(lo, "monda") == 0 ||
                    strcmp(lo, "frida") == 0)
                    logistics_thread = true;
            }
        }

        /* Build situational awareness block */
        w = snprintf(buf + pos, buf_cap - pos, "--- Conversation awareness ---\n");
        if (w > 0)
            pos += (size_t)w;

        if (they_seem_frustrated) {
            w = snprintf(buf + pos, buf_cap - pos,
                         "They seem frustrated. Be calm, acknowledge it, don't be dismissive.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (they_seem_excited) {
            w = snprintf(buf + pos, buf_cap - pos,
                         "They seem excited about something. Be genuinely happy for them.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (they_seem_sad) {
            w = snprintf(
                buf + pos, buf_cap - pos,
                "They seem sad or down. Be present and gentle. Don't try to fix it immediately.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (open_question) {
            w = snprintf(buf + pos, buf_cap - pos,
                         "They asked a question. Make sure you actually answer it.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (they_sent_link) {
            w = snprintf(
                buf + pos, buf_cap - pos,
                "They shared a link. Acknowledge it, say you'll look at it, or comment on it.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (logistics_thread) {
            w = snprintf(
                buf + pos, buf_cap - pos,
                "There's an active logistics/travel thread. Stay on topic with specifics.\n");
            if (w > 0)
                pos += (size_t)w;
        }

        w = snprintf(buf + pos, buf_cap - pos,
                     "\nUse this context naturally. Reference specific details they mentioned. "
                     "Do NOT summarize or acknowledge this context aloud.\n");
        if (w > 0)
            pos += (size_t)w;
    }

    buf[pos] = '\0';
    *out_len = pos;
    return buf;
}
#endif

/* ---- Response Decision Engine ---- */

/*
 * Classify a message and decide whether/how to respond.
 * Returns:
 *   0 = RESPOND normally (generate full response)
 *   1 = SKIP (don't respond at all — message doesn't warrant a reply)
 *   2 = DELAY (respond, but with extra delay — thoughtful/emotional content)
 *
 * Also sets *delay_extra_ms to additional delay beyond the base reading delay.
 */
#if !defined(SC_IS_TEST)
static int classify_response_action(const char *msg, size_t msg_len, unsigned int *delay_extra_ms) {
    *delay_extra_ms = 0;

    if (msg_len == 0)
        return 1; /* skip empty */

    /* Messages that don't warrant a response */
    static const char *skip_patterns[] = {"lol",   "haha",    "hahaha",     "lmao",     "ok",
                                          "okay",  "k",       "nice",       "cool",     "ya",
                                          "yep",   "yup",     "mhm",        "hmm",      "Liked",
                                          "Loved", "Laughed", "Emphasized", "Disliked", NULL};

    /* Normalize: strip whitespace and compare lowercase */
    char norm[64];
    size_t ni = 0;
    for (size_t i = 0; i < msg_len && ni < sizeof(norm) - 1; i++) {
        char c = msg[i];
        if (c >= 'A' && c <= 'Z')
            c += 32;
        if (c != ' ' && c != '\n' && c != '\r')
            norm[ni++] = c;
    }
    norm[ni] = '\0';

    for (int i = 0; skip_patterns[i]; i++) {
        if (strcmp(norm, skip_patterns[i]) == 0)
            return 1; /* skip */
    }

    /* Single emoji or very short non-text messages */
    if (msg_len <= 2)
        return 1; /* skip */

    /* URL-only messages: respond briefly, quick timing */
    {
        bool has_url = false;
        for (size_t i = 0; i + 4 < msg_len; i++) {
            if (memcmp(msg + i, "http", 4) == 0) {
                has_url = true;
                break;
            }
        }
        if (has_url) {
            /* Check if there's meaningful text beyond the URL */
            size_t non_url_chars = 0;
            bool in_url = false;
            for (size_t i = 0; i < msg_len; i++) {
                if (!in_url && i + 4 < msg_len && memcmp(msg + i, "http", 4) == 0)
                    in_url = true;
                if (in_url && (msg[i] == ' ' || msg[i] == '\n'))
                    in_url = false;
                if (!in_url && msg[i] != ' ' && msg[i] != '\n')
                    non_url_chars++;
            }
            if (non_url_chars < 5) {
                /* URL with minimal text — brief acknowledgment, short delay */
                *delay_extra_ms = 3000;
                return 0;
            }
        }
    }

    /* Emotional/important messages deserve a thoughtful delay */
    static const char *emotional_markers[] = {
        "miss",    "love",     "hurt",   "stress",    "depress",   "lonely",     "scared",
        "worried", "sorry",    "afraid", "giving up", "feel like", "don't know", "can't",
        "help me", "need you", "cry",    "sad",       NULL};

    for (int i = 0; emotional_markers[i]; i++) {
        const char *marker = emotional_markers[i];
        size_t mlen = strlen(marker);
        for (size_t j = 0; j + mlen <= msg_len; j++) {
            bool match = true;
            for (size_t k = 0; k < mlen; k++) {
                char a = msg[j + k];
                char b = marker[k];
                if (a >= 'A' && a <= 'Z')
                    a += 32;
                if (a != b) {
                    match = false;
                    break;
                }
            }
            if (match) {
                *delay_extra_ms = 8000; /* 8 extra seconds for emotional */
                return 2;               /* delayed response */
            }
        }
    }

    /* Question messages: normal response, moderate delay */
    for (size_t i = 0; i < msg_len; i++) {
        if (msg[i] == '?') {
            *delay_extra_ms = 2000;
            return 0;
        }
    }

    /* Default: respond normally */
    *delay_extra_ms = 0;
    return 0;
}
#endif

/* ── Service loop ──────────────────────────────────────────────────────── */

sc_error_t sc_service_run(sc_allocator_t *alloc, uint32_t tick_interval_ms,
                          sc_service_channel_t *channels, size_t channel_count, sc_agent_t *agent,
                          const sc_config_t *config) {
    if (!alloc)
        return SC_ERR_INVALID_ARGUMENT;
    if (tick_interval_ms == 0)
        tick_interval_ms = 1000;

#ifdef SC_IS_TEST
    (void)tick_interval_ms;
    (void)channels;
    (void)channel_count;
    (void)agent;
    (void)config;
#ifdef SC_HAS_CRON
    run_cron_tick(alloc);
    sc_service_run_agent_cron(alloc, agent, channels, channel_count);
#endif
    return SC_OK;
#else

#if !defined(_WIN32) && !defined(__CYGWIN__)
    g_stop_flag = 0;
    signal(SIGTERM, service_signal_handler);
    signal(SIGINT, service_signal_handler);
#endif

#ifdef SC_HAS_CRON
    time_t last_cron_minute = 0;
#endif

    {
        struct timespec ts_now;
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        int64_t start = (int64_t)ts_now.tv_sec * 1000 + ts_now.tv_nsec / 1000000;
        for (size_t i = 0; i < channel_count; i++)
            channels[i].last_poll_ms = start;
    }

#if !defined(_WIN32) && !defined(__CYGWIN__)
#define SC_STOP_FLAG g_stop_flag
#else
    volatile int local_stop = 0;
#define SC_STOP_FLAG local_stop
#endif

    while (!SC_STOP_FLAG) {
#ifdef SC_HAS_CRON
        {
            time_t t = time(NULL);
            time_t current_minute = t / 60;
            if (current_minute > last_cron_minute) {
                run_cron_tick(alloc);
                sc_service_run_agent_cron(alloc, agent, channels, channel_count);
                last_cron_minute = current_minute;
            }
        }
#endif

        struct timespec ts_now;
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        int64_t tick_now = (int64_t)ts_now.tv_sec * 1000 + ts_now.tv_nsec / 1000000;

        for (size_t i = 0; i < channel_count; i++) {
            sc_service_channel_t *ch = &channels[i];
            if (!ch->poll_fn || !ch->channel_ctx)
                continue;
            if (tick_now - ch->last_poll_ms < (int64_t)ch->interval_ms)
                continue;

            sc_channel_loop_msg_t msgs[16];
            size_t count = 0;
            ch->poll_fn(ch->channel_ctx, alloc, msgs, 16, &count);
            ch->last_poll_ms = tick_now;

            if (!agent || !ch->channel || !ch->channel->vtable || !ch->channel->vtable->send ||
                count == 0)
                continue;

            /*
             * Batch consecutive messages from the same sender into one prompt.
             * A real person reads all pending messages, then replies once.
             */
            for (size_t m = 0; m < count; /* advanced inside */) {
                size_t content_len = strlen(msgs[m].content);
                if (content_len == 0) {
                    m++;
                    continue;
                }

                const char *batch_key = msgs[m].session_key;
                size_t key_len = strlen(batch_key);

                /* Gather consecutive messages from the same sender */
                char combined[4096];
                size_t combined_len = 0;
                size_t batch_start = m;
                size_t batch_end = m;

                while (m < count && strcmp(msgs[m].session_key, batch_key) == 0) {
                    size_t mlen = strlen(msgs[m].content);
                    if (mlen == 0) {
                        m++;
                        continue;
                    }
                    if (combined_len + mlen + 2 >= sizeof(combined))
                        break;
                    if (combined_len > 0)
                        combined[combined_len++] = '\n';
                    memcpy(combined + combined_len, msgs[m].content, mlen);
                    combined_len += mlen;
                    batch_end = m;
                    m++;
                }
                combined[combined_len] = '\0';

                if (combined_len == 0)
                    continue;

#ifndef SC_IS_TEST
                /* Response decision: should we respond? How long to wait? */
                unsigned int extra_delay_ms = 0;
                int action = classify_response_action(combined, combined_len, &extra_delay_ms);
                if (action == 1) {
                    /* SKIP: message doesn't warrant a response (lol, ok, tapback, etc.) */
                    fprintf(stderr, "[seaclaw] skipping message (no response needed): %.*s\n",
                            (int)(combined_len > 40 ? 40 : combined_len), combined);
                    /* Still persist the message for history */
                    if (agent->session_store && agent->session_store->vtable &&
                        agent->session_store->vtable->save_message) {
                        for (size_t b = batch_start; b <= batch_end; b++) {
                            size_t blen = strlen(msgs[b].content);
                            if (blen > 0)
                                agent->session_store->vtable->save_message(
                                    agent->session_store->ctx, batch_key, key_len, "user", 4,
                                    msgs[b].content, blen);
                        }
                    }
                    continue;
                }
                /* Adaptive timing based on message type */
                {
                    size_t msg_count = batch_end - batch_start + 1;
                    unsigned int read_ms = 2500 + (unsigned int)(msg_count * 1500);
                    if (read_ms > 8000)
                        read_ms = 8000;
                    read_ms += extra_delay_ms;
                    if (read_ms > 15000)
                        read_ms = 15000;
                    usleep(read_ms * 1000);
                }
#endif

                sc_agent_clear_history(agent);

                /* Set active channel for per-channel persona overlays */
                if (ch->channel->vtable->name) {
                    agent->active_channel = ch->channel->vtable->name(ch->channel->ctx);
                    agent->active_channel_len =
                        agent->active_channel ? strlen(agent->active_channel) : 0;
                } else {
                    agent->active_channel = NULL;
                    agent->active_channel_len = 0;
                }

#ifdef SC_HAS_PERSONA
                /* Apply per-channel persona override if configured */
                if (config && agent->active_channel) {
                    const char *channel_persona =
                        sc_config_persona_for_channel(config, agent->active_channel);
                    const char *current = agent->persona_name ? agent->persona_name : "";
                    if (channel_persona && channel_persona[0] &&
                        strcmp(channel_persona, current) != 0) {
                        sc_error_t perr =
                            sc_agent_set_persona(agent, channel_persona, strlen(channel_persona));
                        if (perr != SC_OK) {
#ifndef SC_IS_TEST
                            fprintf(stderr,
                                    "[seaclaw] warning: failed to switch persona to '%s' for "
                                    "channel '%s'\n",
                                    channel_persona,
                                    agent->active_channel ? agent->active_channel : "?");
#endif
                        }
                    }
                }
#else
                (void)config;
#endif

                /* Restore prior conversation for this sender */
                if (agent->session_store && agent->session_store->vtable &&
                    agent->session_store->vtable->load_messages) {
                    sc_message_entry_t *entries = NULL;
                    size_t entry_count = 0;
                    if (agent->session_store->vtable->load_messages(
                            agent->session_store->ctx, alloc, msgs[m].session_key, key_len,
                            &entries, &entry_count) == SC_OK &&
                        entries && entry_count > 0) {
                        for (size_t e = 0; e < entry_count; e++) {
                            if (!entries[e].content || entries[e].content_len == 0)
                                continue;
                            sc_role_t role = SC_ROLE_USER;
                            if (entries[e].role) {
                                if (strcmp(entries[e].role, "assistant") == 0)
                                    role = SC_ROLE_ASSISTANT;
                                else if (strcmp(entries[e].role, "system") == 0)
                                    role = SC_ROLE_SYSTEM;
                            }
                            if (agent->history_count >= agent->history_cap) {
                                size_t new_cap = agent->history_cap ? agent->history_cap * 2 : 8;
                                sc_owned_message_t *arr = (sc_owned_message_t *)alloc->realloc(
                                    alloc->ctx, agent->history,
                                    agent->history_cap * sizeof(sc_owned_message_t),
                                    new_cap * sizeof(sc_owned_message_t));
                                if (!arr)
                                    break;
                                agent->history = arr;
                                agent->history_cap = new_cap;
                            }
                            sc_owned_message_t *hm = &agent->history[agent->history_count];
                            memset(hm, 0, sizeof(*hm));
                            hm->role = role;
                            hm->content = sc_strndup(agent->alloc, entries[e].content,
                                                     entries[e].content_len);
                            hm->content_len = entries[e].content_len;
                            if (hm->content)
                                agent->history_count++;
                        }
                        for (size_t e = 0; e < entry_count; e++) {
                            if (entries[e].role)
                                alloc->free(alloc->ctx, (void *)entries[e].role,
                                            entries[e].role_len + 1);
                            if (entries[e].content)
                                alloc->free(alloc->ctx, (void *)entries[e].content,
                                            entries[e].content_len + 1);
                        }
                        alloc->free(alloc->ctx, entries, entry_count * sizeof(sc_message_entry_t));
                    }
                }

                char *response = NULL;
                size_t response_len = 0;

                /* Temporarily inject iMessage conversation history as context */
                char *saved_ci = agent->custom_instructions;
                size_t saved_ci_len = agent->custom_instructions_len;
                char *convo_ctx = NULL;
                size_t convo_ctx_len = 0;

#if defined(__APPLE__) && defined(__MACH__) && defined(SC_ENABLE_SQLITE) && !defined(SC_IS_TEST)
                convo_ctx = load_imessage_context(alloc, batch_key, &convo_ctx_len);
                if (convo_ctx && convo_ctx_len > 0) {
                    size_t merged_len = convo_ctx_len + (saved_ci_len ? saved_ci_len + 1 : 0);
                    char *merged = (char *)alloc->alloc(alloc->ctx, merged_len + 1);
                    if (merged) {
                        size_t mpos = 0;
                        if (saved_ci && saved_ci_len > 0) {
                            memcpy(merged, saved_ci, saved_ci_len);
                            mpos = saved_ci_len;
                            merged[mpos++] = '\n';
                        }
                        memcpy(merged + mpos, convo_ctx, convo_ctx_len);
                        mpos += convo_ctx_len;
                        merged[mpos] = '\0';
                        agent->custom_instructions = merged;
                        agent->custom_instructions_len = mpos;
                    }
                }
#endif

                sc_error_t err =
                    sc_agent_turn(agent, combined, combined_len, &response, &response_len);

                /* Restore original custom_instructions */
                if (convo_ctx) {
                    if (agent->custom_instructions != saved_ci)
                        alloc->free(alloc->ctx, agent->custom_instructions,
                                    agent->custom_instructions_len + 1);
                    agent->custom_instructions = saved_ci;
                    agent->custom_instructions_len = saved_ci_len;
                    alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                }

                /* Persist each individual message + the single response */
                if (agent->session_store && agent->session_store->vtable &&
                    agent->session_store->vtable->save_message) {
                    for (size_t b = batch_start; b <= batch_end; b++) {
                        size_t blen = strlen(msgs[b].content);
                        if (blen > 0) {
                            agent->session_store->vtable->save_message(agent->session_store->ctx,
                                                                       batch_key, key_len, "user",
                                                                       4, msgs[b].content, blen);
                        }
                    }
                    if (err == SC_OK && response && response_len > 0) {
                        agent->session_store->vtable->save_message(agent->session_store->ctx,
                                                                   batch_key, key_len, "assistant",
                                                                   9, response, response_len);
                    }
                }

                if (err == SC_OK && response && response_len > 0) {
                    ch->channel->vtable->send(ch->channel->ctx, batch_key, key_len, response,
                                              response_len, NULL, 0);
                }
                if (response) {
                    alloc->free(alloc->ctx, response, response_len + 1);
                }
            }
        }

        struct timespec sleep_ts = {.tv_sec = tick_interval_ms / 1000,
                                    .tv_nsec = (long)(tick_interval_ms % 1000) * 1000000L};
        nanosleep(&sleep_ts, NULL);
    }

#undef SC_STOP_FLAG
    return SC_OK;
#endif /* SC_IS_TEST */
}

/* ── Daemon management ───────────────────────────────────────────────── */

#ifdef SC_IS_TEST
sc_error_t sc_daemon_start(void) {
    char path[SC_MAX_PATH];
    int n = get_pid_path(path, sizeof(path));
    if (n <= 0 || (size_t)n >= sizeof(path))
        return SC_ERR_INVALID_ARGUMENT;
    return SC_OK;
}

sc_error_t sc_daemon_stop(void) {
    return SC_OK;
}

bool sc_daemon_status(void) {
    return false;
}
#elif defined(_WIN32) || defined(__CYGWIN__)
sc_error_t sc_daemon_start(void) {
    return SC_ERR_NOT_SUPPORTED;
}

sc_error_t sc_daemon_stop(void) {
    return SC_ERR_NOT_SUPPORTED;
}

bool sc_daemon_status(void) {
    return false;
}
#else
sc_error_t sc_daemon_start(void) {
    char path[SC_MAX_PATH];
    int n = get_pid_path(path, sizeof(path));
    if (n <= 0 || (size_t)n >= sizeof(path))
        return SC_ERR_INVALID_ARGUMENT;

    const char *home = getenv("HOME");
    if (!home)
        home = ".";
    char dir[SC_MAX_PATH];
    n = snprintf(dir, sizeof(dir), "%s/%s", home, SC_DAEMON_PID_DIR);
    if (n <= 0 || (size_t)n >= sizeof(dir))
        return SC_ERR_IO;

    if (mkdir(dir, 0755) != 0 && errno != EEXIST)
        return SC_ERR_IO;

    pid_t pid = fork();
    if (pid < 0)
        return SC_ERR_IO;
    if (pid > 0) {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "%d\n", (int)pid);
            fclose(f);
        }
        return SC_OK;
    }

    setsid();
    if (chdir("/") != 0)
        _exit(127);
    if (!freopen("/dev/null", "r", stdin))
        _exit(127);
    if (!freopen("/dev/null", "w", stdout))
        _exit(127);
    if (!freopen("/dev/null", "w", stderr))
        _exit(127);

    execlp("seaclaw", "seaclaw", "service-loop", (char *)NULL);
    _exit(1);
}

sc_error_t sc_daemon_stop(void) {
    char path[SC_MAX_PATH];
    int n = get_pid_path(path, sizeof(path));
    if (n <= 0 || (size_t)n >= sizeof(path))
        return SC_ERR_INVALID_ARGUMENT;

    FILE *f = fopen(path, "r");
    if (!f)
        return SC_ERR_NOT_FOUND;

    int pid_val = 0;
    if (fscanf(f, "%d", &pid_val) != 1 || pid_val <= 0) {
        fclose(f);
        return SC_ERR_INVALID_ARGUMENT;
    }
    fclose(f);

    if (kill((pid_t)pid_val, SIGTERM) != 0)
        return SC_ERR_IO;

    remove(path);
    return SC_OK;
}

bool sc_daemon_status(void) {
    char path[SC_MAX_PATH];
    int n = get_pid_path(path, sizeof(path));
    if (n <= 0 || (size_t)n >= sizeof(path))
        return false;

    FILE *f = fopen(path, "r");
    if (!f)
        return false;

    int pid_val = 0;
    int ok = (fscanf(f, "%d", &pid_val) == 1 && pid_val > 0);
    fclose(f);
    if (!ok)
        return false;

    return kill((pid_t)pid_val, 0) == 0;
}
#endif

/* ── Platform service install/uninstall/logs ─────────────────────────── */

#if defined(SC_IS_TEST)

sc_error_t sc_daemon_install(sc_allocator_t *alloc) {
    (void)alloc;
    return SC_OK;
}

sc_error_t sc_daemon_uninstall(void) {
    return SC_OK;
}

sc_error_t sc_daemon_logs(void) {
    return SC_OK;
}

#elif defined(__APPLE__)

#define SC_LAUNCHD_LABEL "com.seaclaw.agent"
#define SC_LAUNCHD_DIR   "Library/LaunchAgents"

static int get_binary_path(char *buf, size_t buf_size) {
    const char *paths[] = {"/usr/local/bin/seaclaw", "/opt/homebrew/bin/seaclaw", NULL};
    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], X_OK) == 0) {
            size_t len = strlen(paths[i]);
            if (len < buf_size) {
                memcpy(buf, paths[i], len + 1);
                return (int)len;
            }
        }
    }
    return -1;
}

sc_error_t sc_daemon_install(sc_allocator_t *alloc) {
    (void)alloc;
    const char *home = getenv("HOME");
    if (validate_home(home) != SC_OK)
        return SC_ERR_INVALID_ARGUMENT;

    char bin[SC_MAX_PATH];
    if (get_binary_path(bin, sizeof(bin)) < 0)
        return SC_ERR_NOT_FOUND;

    char dir[SC_MAX_PATH];
    int n = snprintf(dir, sizeof(dir), "%s/%s", home, SC_LAUNCHD_DIR);
    if (n <= 0 || (size_t)n >= sizeof(dir))
        return SC_ERR_IO;
    mkdir(dir, 0755);

    char plist[SC_MAX_PATH];
    n = snprintf(plist, sizeof(plist), "%s/%s.plist", dir, SC_LAUNCHD_LABEL);
    if (n <= 0 || (size_t)n >= sizeof(plist))
        return SC_ERR_IO;

    char log_path[SC_MAX_PATH];
    n = snprintf(log_path, sizeof(log_path), "%s/.seaclaw/seaclaw.log", home);
    if (n <= 0 || (size_t)n >= sizeof(log_path))
        return SC_ERR_IO;

    FILE *f = fopen(plist, "w");
    if (!f)
        return SC_ERR_IO;

    fprintf(f,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "  <key>Label</key>\n"
            "  <string>%s</string>\n"
            "  <key>ProgramArguments</key>\n"
            "  <array>\n"
            "    <string>%s</string>\n"
            "    <string>service-loop</string>\n"
            "  </array>\n"
            "  <key>RunAtLoad</key>\n"
            "  <true/>\n"
            "  <key>KeepAlive</key>\n"
            "  <true/>\n"
            "  <key>StandardOutPath</key>\n"
            "  <string>%s</string>\n"
            "  <key>StandardErrorPath</key>\n"
            "  <string>%s</string>\n"
            "  <key>EnvironmentVariables</key>\n"
            "  <dict>\n"
            "    <key>HOME</key>\n"
            "    <string>%s</string>\n"
            "  </dict>\n"
            "</dict>\n"
            "</plist>\n",
            SC_LAUNCHD_LABEL, bin, log_path, log_path, home);
    fclose(f);

    char cmd[SC_MAX_PATH * 2];
    n = snprintf(cmd, sizeof(cmd), "launchctl load -w \"%s\"", plist);
    if (n <= 0 || (size_t)n >= sizeof(cmd))
        return SC_ERR_IO;
    if (system(cmd) != 0)
        return SC_ERR_IO;

    return SC_OK;
}

sc_error_t sc_daemon_uninstall(void) {
    const char *home = getenv("HOME");
    if (validate_home(home) != SC_OK)
        return SC_ERR_INVALID_ARGUMENT;

    char plist[SC_MAX_PATH];
    int n =
        snprintf(plist, sizeof(plist), "%s/%s/%s.plist", home, SC_LAUNCHD_DIR, SC_LAUNCHD_LABEL);
    if (n <= 0 || (size_t)n >= sizeof(plist))
        return SC_ERR_IO;

    char cmd[SC_MAX_PATH * 2];
    n = snprintf(cmd, sizeof(cmd), "launchctl unload \"%s\"", plist);
    if (n > 0 && (size_t)n < sizeof(cmd))
        SC_IGNORE_RESULT(system(cmd));

    remove(plist);
    return SC_OK;
}

sc_error_t sc_daemon_logs(void) {
    const char *home = getenv("HOME");
    if (validate_home(home) != SC_OK)
        return SC_ERR_INVALID_ARGUMENT;

    char log_path[SC_MAX_PATH];
    int n = snprintf(log_path, sizeof(log_path), "%s/.seaclaw/seaclaw.log", home);
    if (n <= 0 || (size_t)n >= sizeof(log_path))
        return SC_ERR_IO;

    char cmd[SC_MAX_PATH + 16];
    n = snprintf(cmd, sizeof(cmd), "tail -f \"%s\"", log_path);
    if (n <= 0 || (size_t)n >= sizeof(cmd))
        return SC_ERR_IO;

    return system(cmd) == 0 ? SC_OK : SC_ERR_IO;
}

#elif defined(__linux__)

#define SC_SYSTEMD_UNIT "seaclaw.service"

sc_error_t sc_daemon_install(sc_allocator_t *alloc) {
    (void)alloc;
    const char *home = getenv("HOME");
    if (validate_home(home) != SC_OK)
        return SC_ERR_INVALID_ARGUMENT;

    char dir[SC_MAX_PATH];
    int n = snprintf(dir, sizeof(dir), "%s/.config/systemd/user", home);
    if (n <= 0 || (size_t)n >= sizeof(dir))
        return SC_ERR_IO;

    char mkdir_cmd[SC_MAX_PATH + 16];
    n = snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", dir);
    if (n <= 0 || (size_t)n >= sizeof(mkdir_cmd))
        return SC_ERR_IO;
    SC_IGNORE_RESULT(system(mkdir_cmd));

    char bin[SC_MAX_PATH];
    int found = 0;
    const char *paths[] = {"/usr/local/bin/seaclaw", "/usr/bin/seaclaw", NULL};
    for (int i = 0; paths[i]; i++) {
        if (access(paths[i], X_OK) == 0) {
            size_t len = strlen(paths[i]);
            if (len < sizeof(bin)) {
                memcpy(bin, paths[i], len + 1);
                found = 1;
                break;
            }
        }
    }
    if (!found)
        return SC_ERR_NOT_FOUND;

    char unit_path[SC_MAX_PATH];
    n = snprintf(unit_path, sizeof(unit_path), "%s/%s", dir, SC_SYSTEMD_UNIT);
    if (n <= 0 || (size_t)n >= sizeof(unit_path))
        return SC_ERR_IO;

    FILE *f = fopen(unit_path, "w");
    if (!f)
        return SC_ERR_IO;

    fprintf(f,
            "[Unit]\n"
            "Description=SeaClaw AI Assistant\n"
            "After=network-online.target\n"
            "Wants=network-online.target\n"
            "\n"
            "[Service]\n"
            "Type=simple\n"
            "ExecStart=%s service-loop\n"
            "Restart=on-failure\n"
            "RestartSec=5\n"
            "Environment=HOME=%s\n"
            "\n"
            "[Install]\n"
            "WantedBy=default.target\n",
            bin, home);
    fclose(f);

    SC_IGNORE_RESULT(system("systemctl --user daemon-reload"));
    if (system("systemctl --user enable --now " SC_SYSTEMD_UNIT) != 0)
        return SC_ERR_IO;

    return SC_OK;
}

sc_error_t sc_daemon_uninstall(void) {
    SC_IGNORE_RESULT(system("systemctl --user disable --now " SC_SYSTEMD_UNIT));

    const char *home = getenv("HOME");
    if (!home)
        return SC_ERR_INVALID_ARGUMENT;

    char unit_path[SC_MAX_PATH];
    int n =
        snprintf(unit_path, sizeof(unit_path), "%s/.config/systemd/user/%s", home, SC_SYSTEMD_UNIT);
    if (n > 0 && (size_t)n < sizeof(unit_path))
        remove(unit_path);

    SC_IGNORE_RESULT(system("systemctl --user daemon-reload"));
    return SC_OK;
}

sc_error_t sc_daemon_logs(void) {
    return system("journalctl --user -u " SC_SYSTEMD_UNIT " -f") == 0 ? SC_OK : SC_ERR_IO;
}

#else

sc_error_t sc_daemon_install(sc_allocator_t *alloc) {
    (void)alloc;
    return SC_ERR_NOT_SUPPORTED;
}

sc_error_t sc_daemon_uninstall(void) {
    return SC_ERR_NOT_SUPPORTED;
}

sc_error_t sc_daemon_logs(void) {
    return SC_ERR_NOT_SUPPORTED;
}

#endif
