#include "seaclaw/daemon.h"
#include "seaclaw/agent.h"
#include "seaclaw/agent/episodic.h"
#include "seaclaw/agent/outcomes.h"
#include "seaclaw/agent/proactive.h"
#include "seaclaw/memory/consolidation.h"
#include "seaclaw/memory/deep_extract.h"
#include "seaclaw/config.h"
#include "seaclaw/context/conversation.h"
#include "seaclaw/context/event_extract.h"
#include "seaclaw/context/mood.h"
#include "seaclaw/memory/emotional_graph.h"
#include "seaclaw/memory/promotion.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/process_util.h"
#include "seaclaw/core/string.h"
#ifdef SC_HAS_PERSONA
#include "seaclaw/persona.h"
#include "seaclaw/persona/replay.h"
#endif
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

/* sqlite3.h no longer needed in daemon — moved to channel vtable implementations */

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

#ifndef SC_IS_TEST
/* Build conversation callback context from memory.
 * Queries memory for past topics relevant to the current message.
 * Returns an allocated string (caller frees) or NULL. */
static char *build_callback_context(sc_allocator_t *alloc, sc_memory_t *memory,
                                    const char *session_id, size_t session_id_len,
                                    const char *msg, size_t msg_len, size_t *out_len) {
    *out_len = 0;
    if (!memory || !memory->vtable || !memory->vtable->recall || !msg || msg_len == 0)
        return NULL;

    sc_memory_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = memory->vtable->recall(memory->ctx, alloc, msg, msg_len, 3, session_id,
                                            session_id_len, &entries, &count);
    if (err != SC_OK || !entries || count == 0)
        return NULL;

    char buf[2048];
    size_t pos = 0;
    int w = snprintf(buf, sizeof(buf),
                     "\n--- Conversation memory ---\n"
                     "Past topics from your conversations with this person:\n");
    if (w > 0)
        pos = (size_t)w;

    size_t usable = 0;
    for (size_t i = 0; i < count && i < 3; i++) {
        if (!entries[i].content || entries[i].content_len == 0)
            continue;
        size_t show = entries[i].content_len;
        if (show > 200)
            show = 200;
        w = snprintf(buf + pos, sizeof(buf) - pos, "- %.*s\n", (int)show, entries[i].content);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) {
            pos += (size_t)w;
            usable++;
        }
    }

    for (size_t i = 0; i < count; i++)
        sc_memory_entry_free_fields(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(sc_memory_entry_t));

    if (usable == 0)
        return NULL;

    w = snprintf(buf + pos, sizeof(buf) - pos,
                 "If naturally relevant, bring up a past topic — but NEVER say "
                 "'I remember you mentioned'. Instead: 'oh wait how did that thing "
                 "with X go?' or 'did you ever end up doing Y?'\n"
                 "Only reference if it fits naturally. Don't force it.\n"
                 "--- End memory ---\n");
    if (w > 0 && pos + (size_t)w < sizeof(buf))
        pos += (size_t)w;

    char *result = (char *)alloc->alloc(alloc->ctx, pos + 1);
    if (!result)
        return NULL;
    memcpy(result, buf, pos);
    result[pos] = '\0';
    *out_len = pos;
    return result;
}

/* Store a conversation summary as long-term memory.
 * Concatenates the user message and agent response, runs deep-extract
 * on the full exchange, and stores extracted facts scoped to the contact. */
static void store_conversation_summary(sc_allocator_t *alloc, sc_memory_t *memory,
                                       const char *session_id, size_t session_id_len,
                                       const char *user_msg, size_t user_msg_len,
                                       const char *response, size_t response_len) {
    if (!alloc || !memory || !memory->vtable || !memory->vtable->store)
        return;
    if (!user_msg || user_msg_len == 0)
        return;

    /* Build "them: ... | me: ..." for richer extraction context */
    size_t total = user_msg_len + response_len + 16;
    char *combined = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!combined)
        return;
    int n = snprintf(combined, total + 1, "them: %.*s | me: %.*s", (int)user_msg_len, user_msg,
                     (int)response_len, response);
    if (n <= 0) {
        alloc->free(alloc->ctx, combined, total + 1);
        return;
    }
    size_t combined_len = (size_t)n < total ? (size_t)n : total;

    sc_deep_extract_result_t de;
    memset(&de, 0, sizeof(de));
    sc_error_t err = sc_deep_extract_lightweight(alloc, combined, combined_len, &de);
    if (err == SC_OK && de.fact_count > 0) {
        static const char cat_name[] = "conversation_summary";
        sc_memory_category_t cat = {
            .tag = SC_MEMORY_CATEGORY_CUSTOM,
            .data.custom = {.name = cat_name, .name_len = sizeof(cat_name) - 1},
        };
        for (size_t i = 0; i < de.fact_count; i++) {
            const sc_extracted_fact_t *f = &de.facts[i];
            if (!f->subject || !f->predicate || !f->object)
                continue;
            char key_buf[256];
            int kn = snprintf(key_buf, sizeof(key_buf), "%s:%s:%s", f->subject, f->predicate,
                              f->object);
            if (kn > 0 && (size_t)kn < sizeof(key_buf)) {
                (void)memory->vtable->store(memory->ctx, key_buf, (size_t)kn, f->object,
                                            strlen(f->object), &cat,
                                            session_id ? session_id : "",
                                            session_id ? session_id_len : 0);
            }
        }
    }
    sc_deep_extract_result_deinit(&de, alloc);
    alloc->free(alloc->ctx, combined, total + 1);
}
#endif /* !SC_IS_TEST */

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
            /* If response is exactly "SKIP", the agent decided not to engage */
            bool skip = (response_len == 4 && memcmp(response, "SKIP", 4) == 0);
            if (!skip) {
                /* Parse "channel:target" format for directed messages */
                const char *ch_part = target_channel;
                const char *target_part = NULL;
                size_t target_part_len = 0;
                const char *colon = strchr(target_channel, ':');
                char ch_buf[64] = {0};
                if (colon) {
                    size_t ch_len = (size_t)(colon - target_channel);
                    if (ch_len < sizeof(ch_buf)) {
                        memcpy(ch_buf, target_channel, ch_len);
                        ch_buf[ch_len] = '\0';
                        ch_part = ch_buf;
                        target_part = colon + 1;
                        target_part_len = strlen(target_part);
                    }
                }
                for (size_t c = 0; c < channel_count; c++) {
                    if (!channels[c].channel || !channels[c].channel->vtable ||
                        !channels[c].channel->vtable->name)
                        continue;
                    const char *ch_name =
                        channels[c].channel->vtable->name(channels[c].channel->ctx);
                    if (ch_name && strcmp(ch_name, ch_part) == 0) {
                        if (channels[c].channel->vtable->send) {
                            (void)channels[c].channel->vtable->send(
                                channels[c].channel->ctx, target_part, target_part_len, response,
                                response_len, NULL, 0);
                        }
                        break;
                    }
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

/* ── Proactive check-in ──────────────────────────────────────────────── */

#if defined(SC_HAS_PERSONA) && !defined(SC_IS_TEST)

static char *proactive_prompt_for_contact(sc_allocator_t *alloc, sc_memory_t *memory,
                                          const sc_contact_profile_t *cp, size_t *out_len) {
    char *starter = NULL;
    size_t starter_len = 0;
    if (memory && cp->contact_id) {
        (void)sc_proactive_build_starter(alloc, memory, cp->contact_id, strlen(cp->contact_id),
                                         &starter, &starter_len);
    }

    static const char RULES[] =
        "\nRules: "
        "1. One short natural message (not 'hey how are you' — too generic). "
        "2. Reference something specific you know about them or ask about "
        "something from a previous conversation. "
        "3. Keep it under 10 words. "
        "4. If you have nothing specific, share something you saw/did "
        "that made you think of them. "
        "5. Reply SKIP if you genuinely have nothing natural to say.";
    size_t rules_len = sizeof(RULES) - 1;

    char base_buf[256];
    int w = snprintf(base_buf, sizeof(base_buf),
                     "You're initiating a casual check-in text to %s. ",
                     cp->name ? cp->name : "this person");
    size_t base_len = (w > 0 && (size_t)w < sizeof(base_buf)) ? (size_t)w : 0;

    size_t total = base_len + rules_len;
    if (starter && starter_len > 0)
        total += 2 + starter_len; /* "\n\n" + starter */

    char *result = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!result) {
        if (starter)
            alloc->free(alloc->ctx, starter, starter_len + 1);
        *out_len = 0;
        return NULL;
    }

    size_t pos = 0;
    memcpy(result, base_buf, base_len);
    pos = base_len;

    if (starter && starter_len > 0) {
        result[pos++] = '\n';
        result[pos++] = '\n';
        memcpy(result + pos, starter, starter_len);
        pos += starter_len;
        alloc->free(alloc->ctx, starter, starter_len + 1);
    }

    memcpy(result + pos, RULES, rules_len);
    pos += rules_len;
    result[pos] = '\0';
    *out_len = pos;
    return result;
}

void sc_service_run_proactive_checkins(sc_allocator_t *alloc, sc_agent_t *agent,
                                       sc_service_channel_t *channels, size_t channel_count) {
    if (!alloc || !agent || !agent->persona)
        return;

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    int hour = tm_now.tm_hour;

    /* Only check in during social hours (9am-9pm) */
    if (hour < 9 || hour > 21)
        return;

    for (size_t i = 0; i < agent->persona->contacts_count; i++) {
        const sc_contact_profile_t *cp = &agent->persona->contacts[i];
        if (!cp->proactive_checkin || !cp->proactive_channel || !cp->contact_id)
            continue;

        /* Parse channel:target format */
        const char *ch_part = cp->proactive_channel;
        const char *target_part = cp->contact_id;
        size_t target_len = strlen(target_part);
        const char *colon = strchr(cp->proactive_channel, ':');
        char ch_buf[64] = {0};
        if (colon) {
            size_t ch_len = (size_t)(colon - cp->proactive_channel);
            if (ch_len < sizeof(ch_buf)) {
                memcpy(ch_buf, cp->proactive_channel, ch_len);
                ch_buf[ch_len] = '\0';
                ch_part = ch_buf;
                target_part = colon + 1;
                target_len = strlen(target_part);
            }
        }

        /* Check last interaction time via conversation history */
        for (size_t c = 0; c < channel_count; c++) {
            char *silence_ctx = NULL;
            size_t silence_ctx_len = 0;

            if (!channels[c].channel || !channels[c].channel->vtable ||
                !channels[c].channel->vtable->name)
                continue;
            const char *ch_name = channels[c].channel->vtable->name(channels[c].channel->ctx);
            if (!ch_name || strcmp(ch_name, ch_part) != 0)
                continue;

            if (!channels[c].channel->vtable->load_conversation_history)
                break;

            sc_channel_history_entry_t *entries = NULL;
            size_t entry_count = 0;
            channels[c].channel->vtable->load_conversation_history(
                channels[c].channel->ctx, alloc, cp->contact_id, strlen(cp->contact_id), 15,
                &entries, &entry_count);

            bool should_checkin = true;
            if (entries && entry_count > 0) {
                struct tm last_tm = {0};
                if (strptime(entries[entry_count - 1].timestamp, "%Y-%m-%d %H:%M", &last_tm)) {
                    time_t last_time = mktime(&last_tm);
                    double hours_since = difftime(now, last_time) / 3600.0;
                    if (hours_since < 24.0)
                        should_checkin = false;
                }
            }

            /* Build combined text from user messages for event extraction */
            char combined[4096];
            size_t combined_len = 0;
            if (entries && entry_count > 0) {
                for (size_t e = 0; e < entry_count; e++) {
                    if (entries[e].from_me)
                        continue;
                    size_t tlen = strlen(entries[e].text);
                    if (tlen == 0)
                        continue;
                    if (combined_len + tlen + 2 >= sizeof(combined))
                        break;
                    if (combined_len > 0)
                        combined[combined_len++] = '\n';
                    memcpy(combined + combined_len, entries[e].text, tlen);
                    combined_len += tlen;
                }
                combined[combined_len] = '\0';
            }
            if (entries)
                alloc->free(alloc->ctx, entries, entry_count * sizeof(*entries));

#ifndef SC_IS_TEST
            /* Silence check-in: if channel has been quiet, consider reaching out */
            {
                sc_silence_config_t silence_cfg = SC_SILENCE_DEFAULTS;
                sc_proactive_result_t silence_result;
                memset(&silence_result, 0, sizeof(silence_result));
                uint64_t now_ms = (uint64_t)now * 1000ULL;
                if (sc_proactive_check_silence(alloc, channels[c].last_contact_ms, now_ms,
                                               &silence_cfg, &silence_result) == SC_OK &&
                    silence_result.count > 0) {
                    should_checkin = true;
                    sc_proactive_build_context(&silence_result, alloc, 3, &silence_ctx,
                                              &silence_ctx_len);
                }
                sc_proactive_result_deinit(&silence_result, alloc);
            }
#endif

            if (!should_checkin)
                break;

#ifndef SC_IS_TEST
            /* Event-triggered follow-ups from recent messages */
            char *event_ctx = NULL;
            size_t event_ctx_len = 0;
            if (combined_len > 0) {
                sc_event_extract_result_t extract_result;
                memset(&extract_result, 0, sizeof(extract_result));
                if (sc_event_extract(alloc, combined, combined_len, &extract_result) == SC_OK &&
                    extract_result.event_count > 0) {
                    sc_proactive_result_t event_result;
                    memset(&event_result, 0, sizeof(event_result));
                    if (sc_proactive_check_events(alloc, extract_result.events,
                                                  extract_result.event_count,
                                                  &event_result) == SC_OK &&
                        event_result.count > 0) {
                        sc_proactive_build_context(&event_result, alloc, 3, &event_ctx,
                                                   &event_ctx_len);
                    }
                    sc_proactive_result_deinit(&event_result, alloc);
                }
                sc_event_extract_result_deinit(&extract_result, alloc);
            }

            /* Build and send the check-in */
            size_t prompt_len = 0;
            char *prompt =
                proactive_prompt_for_contact(alloc, agent->memory, cp, &prompt_len);
            if (prompt && event_ctx && event_ctx_len > 0) {
                size_t merged_len = prompt_len + 1 + event_ctx_len + 1;
                char *merged = (char *)alloc->alloc(alloc->ctx, merged_len);
                if (merged) {
                    memcpy(merged, prompt, prompt_len);
                    merged[prompt_len] = '\n';
                    memcpy(merged + prompt_len + 1, event_ctx, event_ctx_len);
                    merged[prompt_len + 1 + event_ctx_len] = '\0';
                    alloc->free(alloc->ctx, prompt, prompt_len + 1);
                    prompt = merged;
                    prompt_len = prompt_len + 1 + event_ctx_len;
                }
            }
            if (prompt && silence_ctx && silence_ctx_len > 0) {
                size_t merged_len = prompt_len + 1 + silence_ctx_len + 1;
                char *merged = (char *)alloc->alloc(alloc->ctx, merged_len);
                if (merged) {
                    memcpy(merged, prompt, prompt_len);
                    merged[prompt_len] = '\n';
                    memcpy(merged + prompt_len + 1, silence_ctx, silence_ctx_len);
                    merged[prompt_len + 1 + silence_ctx_len] = '\0';
                    alloc->free(alloc->ctx, prompt, prompt_len + 1);
                    prompt = merged;
                    prompt_len = prompt_len + 1 + silence_ctx_len;
                }
            }
            if (prompt) {
                sc_agent_clear_history(agent);
                agent->active_channel = ch_part;
                agent->active_channel_len = strlen(ch_part);

                char *response = NULL;
                size_t response_len = 0;
                sc_error_t err =
                    sc_agent_turn(agent, prompt, prompt_len, &response, &response_len);

                if (err == SC_OK && response && response_len > 0) {
                    bool skip = (response_len == 4 && memcmp(response, "SKIP", 4) == 0);
                    if (!skip && channels[c].channel->vtable->send) {
                        channels[c].channel->vtable->send(channels[c].channel->ctx, target_part,
                                                          target_len, response, response_len,
                                                          NULL, 0);
                        fprintf(stderr, "[seaclaw] proactive check-in sent to %s: %.*s\n",
                                cp->name ? cp->name : cp->contact_id, (int)response_len, response);
                    }
                }
                if (response)
                    alloc->free(alloc->ctx, response, response_len + 1);
                alloc->free(alloc->ctx, prompt, prompt_len + 1);
                sc_agent_clear_history(agent);
            }
            if (event_ctx)
                alloc->free(alloc->ctx, event_ctx, event_ctx_len + 1);
            if (silence_ctx)
                alloc->free(alloc->ctx, silence_ctx, silence_ctx_len + 1);
#endif
            break;
        }
    }
}

#endif /* SC_HAS_PERSONA && !SC_IS_TEST */

/* ── Signal handling (non-test only) ─────────────────────────────────── */

#if !defined(SC_IS_TEST) && !defined(_WIN32) && !defined(__CYGWIN__)
static volatile sig_atomic_t g_stop_flag = 0;

static void service_signal_handler(int sig) {
    (void)sig;
    g_stop_flag = 1;
}
#endif

/* ── Streaming callback for channels with send_event ─────────────────────── */

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

    /* Enable outcome tracking so persona feedback loop works in daemon */
    sc_outcome_tracker_t daemon_outcomes;
    sc_outcome_tracker_init(&daemon_outcomes, true);
    if (agent && !agent->outcomes)
        sc_agent_set_outcomes(agent, &daemon_outcomes);

#ifdef SC_HAS_CRON
    time_t last_cron_minute = 0;
#endif

    {
        struct timespec ts_now;
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        int64_t start = (int64_t)ts_now.tv_sec * 1000 + ts_now.tv_nsec / 1000000;
        for (size_t i = 0; i < channel_count; i++) {
            channels[i].last_poll_ms = start;
            channels[i].last_contact_ms = 0;
        }
    }

#if !defined(_WIN32) && !defined(__CYGWIN__)
#define SC_STOP_FLAG g_stop_flag
#else
    volatile int local_stop = 0;
#define SC_STOP_FLAG local_stop
#endif

#ifndef SC_IS_TEST
    static char replay_insights[2048] = {0};
    static size_t replay_insights_len = 0;
    static size_t promotion_counter = 0;
#endif

    while (!SC_STOP_FLAG) {
#ifdef SC_HAS_CRON
        {
            time_t t = time(NULL);
            time_t current_minute = t / 60;
            if (current_minute > last_cron_minute) {
                run_cron_tick(alloc);
                sc_service_run_agent_cron(alloc, agent, channels, channel_count);
#ifdef SC_HAS_PERSONA
                /* Run proactive check-ins at the top of each hour */
                if (current_minute % 60 == 0) {
                    sc_service_run_proactive_checkins(alloc, agent, channels, channel_count);
                }
#endif
#ifndef SC_IS_TEST
                /* Daily memory consolidation at 3 AM */
                {
                    static bool consolidated_today = false;
                    struct tm *lt = localtime(&t);
                    if (lt && lt->tm_hour == 4) {
                        consolidated_today = false;
                    }
                    if (lt && lt->tm_hour == 3 && lt->tm_min == 0 && agent && agent->memory &&
                        !consolidated_today) {
                        sc_consolidation_config_t cons_cfg = {
                            .decay_days = 30,
                            .decay_factor = 0.5,
                            .dedup_threshold = 70,
                            .max_entries = 5000,
                        };
                        sc_memory_consolidate(alloc, agent->memory, &cons_cfg);
                        consolidated_today = true;
                        fprintf(stderr, "[seaclaw] daily memory consolidation completed\n");
                    }
                }
#endif
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
            memset(msgs, 0, sizeof(msgs));
            size_t count = 0;
            sc_error_t poll_err = ch->poll_fn(ch->channel_ctx, alloc, msgs, 16, &count);
            ch->last_poll_ms = tick_now;
            if (count > 0)
                ch->last_contact_ms = (uint64_t)time(NULL) * 1000ULL;
            if (poll_err != SC_OK && getenv("SC_DEBUG"))
                fprintf(stderr, "[seaclaw] poll error on channel %zu: %d\n", i, (int)poll_err);

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
                /* Only respond to contacts explicitly listed in persona_contacts */
#ifdef SC_HAS_PERSONA
                if (agent->persona && agent->persona->contacts_count > 0) {
                    const sc_contact_profile_t *cp_gate =
                        sc_persona_find_contact(agent->persona, batch_key, key_len);
                    if (!cp_gate) {
                        if (getenv("SC_DEBUG"))
                            fprintf(stderr,
                                    "[seaclaw] ignoring message from unknown contact: %.*s\n",
                                    (int)(key_len > 20 ? 20 : key_len), batch_key);
                        continue;
                    }
                }
#endif

                /* Group chat gating: use group classifier to decide engagement */
                if (msgs[batch_start].is_group) {
                    const char *persona_name = NULL;
#ifdef SC_HAS_PERSONA
                    if (agent->persona && agent->persona->identity)
                        persona_name = agent->persona->identity;
#endif
                    sc_group_response_t gr = sc_conversation_classify_group(
                        combined, combined_len, persona_name,
                        persona_name ? strlen(persona_name) : 0, NULL, 0);
                    if (gr == SC_GROUP_SKIP) {
                        fprintf(stderr, "[seaclaw] group: skipping (not addressed): %.*s\n",
                                (int)(combined_len > 40 ? 40 : combined_len), combined);
                        continue;
                    }
                }

                /* Response decision using conversation-aware classifier */
                uint32_t extra_delay_ms = 0;

                /* Preload channel history early so the classifier can use it */
                sc_channel_history_entry_t *early_history = NULL;
                size_t early_history_count = 0;
                if (ch->channel->vtable->load_conversation_history) {
                    ch->channel->vtable->load_conversation_history(
                        ch->channel->ctx, alloc, batch_key, key_len, 10, &early_history,
                        &early_history_count);
                }

                sc_response_action_t action = sc_conversation_classify_response(
                    combined, combined_len, early_history, early_history_count, &extra_delay_ms);

                if (early_history)
                    alloc->free(alloc->ctx, early_history,
                                early_history_count * sizeof(sc_channel_history_entry_t));

                if (action == SC_RESPONSE_SKIP) {
                    fprintf(stderr, "[seaclaw] skipping message (no response needed): %.*s\n",
                            (int)(combined_len > 40 ? 40 : combined_len), combined);
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

                /* For BRIEF actions, override max_response_chars to force ultra-short */
                bool brief_mode = (action == SC_RESPONSE_BRIEF);

                /* Adaptive timing based on message type.
                 * This sleep also serves as the burst accumulation window —
                 * messages arriving while we "read" get picked up below. */
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

                /* Burst accumulation: re-poll for messages that arrived during
                 * the read delay. Humans finish their thought in 2-3 messages,
                 * so we pick up any follow-ups before responding. */
                {
                    sc_channel_loop_msg_t burst[16];
                    size_t burst_count = 0;
                    ch->poll_fn(ch->channel_ctx, alloc, burst, 16, &burst_count);
                    for (size_t bi = 0; bi < burst_count; bi++) {
                        if (strcmp(burst[bi].session_key, batch_key) != 0)
                            continue;
                        size_t mlen = strlen(burst[bi].content);
                        if (mlen == 0)
                            continue;
                        if (combined_len + mlen + 2 >= sizeof(combined))
                            break;
                        combined[combined_len++] = '\n';
                        memcpy(combined + combined_len, burst[bi].content, mlen);
                        combined_len += mlen;
                        if (agent->session_store && agent->session_store->vtable &&
                            agent->session_store->vtable->save_message) {
                            agent->session_store->vtable->save_message(
                                agent->session_store->ctx, batch_key, key_len, "user", 4,
                                burst[bi].content, mlen);
                        }
                    }
                    combined[combined_len] = '\0';
                }
#else
                bool brief_mode = false;
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
                /* Apply persona override: per-contact takes priority, then per-channel */
                if (config) {
                    const char *persona_override = NULL;
                    if (batch_key && key_len > 0)
                        persona_override = sc_config_persona_for_contact(config, batch_key);
                    if (!persona_override && agent->active_channel)
                        persona_override =
                            sc_config_persona_for_channel(config, agent->active_channel);
                    if (persona_override && persona_override[0]) {
                        const char *current = agent->persona_name ? agent->persona_name : "";
                        if (strcmp(persona_override, current) != 0) {
                            sc_error_t perr = sc_agent_set_persona(agent, persona_override,
                                                                   strlen(persona_override));
                            if (perr != SC_OK) {
#ifndef SC_IS_TEST
                                fprintf(stderr,
                                        "[seaclaw] warning: failed to switch persona to '%s'\n",
                                        persona_override);
#endif
                            }
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
                            agent->session_store->ctx, alloc, batch_key, key_len, &entries,
                            &entry_count) == SC_OK &&
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

                /* Build per-turn context via proper architecture:
                 * 1. Contact profile from persona (sc_persona_find_contact)
                 * 2. Conversation history from channel vtable (load_conversation_history)
                 * 3. Awareness from shared analyzer (sc_conversation_build_awareness)
                 * 4. Response constraints from channel vtable (get_response_constraints)
                 */
                char *contact_ctx = NULL;
                size_t contact_ctx_len = 0;
                char *convo_ctx = NULL;
                size_t convo_ctx_len = 0;
                sc_channel_history_entry_t *history_entries = NULL;
                size_t history_count = 0;

#ifndef SC_IS_TEST
                /* 1. Per-contact profile via persona struct */
#ifdef SC_HAS_PERSONA
                if (agent->persona) {
                    const sc_contact_profile_t *cp =
                        sc_persona_find_contact(agent->persona, batch_key, key_len);
                    if (cp) {
                        sc_contact_profile_build_context(alloc, cp, &contact_ctx, &contact_ctx_len);

                        size_t iw_len = 0;
                        char *iw_ctx = sc_persona_build_inner_world_context(
                            alloc, agent->persona, cp->relationship_stage, &iw_len);
                        if (iw_ctx && iw_len > 0 && contact_ctx) {
                            size_t total = contact_ctx_len + iw_len + 1;
                            char *merged = (char *)alloc->alloc(alloc->ctx, total);
                            if (merged) {
                                memcpy(merged, contact_ctx, contact_ctx_len);
                                memcpy(merged + contact_ctx_len, iw_ctx, iw_len);
                                merged[total - 1] = '\0';
                                alloc->free(alloc->ctx, contact_ctx, contact_ctx_len + 1);
                                contact_ctx = merged;
                                contact_ctx_len = total - 1;
                            }
                            alloc->free(alloc->ctx, iw_ctx, iw_len + 1);
                        } else if (iw_ctx) {
                            alloc->free(alloc->ctx, iw_ctx, iw_len + 1);
                        }
                    }
                }
#endif

                /* 2. Conversation history via channel vtable */
                if (ch->channel->vtable->load_conversation_history) {
                    ch->channel->vtable->load_conversation_history(
                        ch->channel->ctx, alloc, batch_key, key_len, 25, &history_entries,
                        &history_count);
                }

                /* 2b. Cross-channel awareness: load history from OTHER channels
                 * for the same contact (e.g. Gmail history when texting via iMessage) */
                sc_channel_history_entry_t *cross_entries = NULL;
                size_t cross_count = 0;
#ifdef SC_HAS_PERSONA
                {
                    const sc_contact_profile_t *cp_cross = NULL;
                    if (agent->persona)
                        cp_cross = sc_persona_find_contact(agent->persona, batch_key, key_len);
                    if (cp_cross && cp_cross->email) {
                        size_t email_len = strlen(cp_cross->email);
                        for (size_t ci = 0; ci < channel_count; ci++) {
                            if (&channels[ci] == ch)
                                continue;
                            if (!channels[ci].channel->vtable->load_conversation_history)
                                continue;
                            channels[ci].channel->vtable->load_conversation_history(
                                channels[ci].channel->ctx, alloc, cp_cross->email, email_len, 5,
                                &cross_entries, &cross_count);
                            if (cross_entries && cross_count > 0)
                                break;
                        }
                    }
                }
#endif

                /* 3. Build awareness context from history via shared analyzer.
                 * Merge primary + cross-channel history into one array. */
                if (history_count > 0 || cross_count > 0) {
                    size_t total = history_count + cross_count;
                    sc_channel_history_entry_t *merged_history = NULL;
                    if (cross_count > 0 && history_count > 0) {
                        merged_history = (sc_channel_history_entry_t *)alloc->alloc(
                            alloc->ctx, total * sizeof(sc_channel_history_entry_t));
                        if (merged_history) {
                            if (cross_entries)
                                memcpy(merged_history, cross_entries,
                                       cross_count * sizeof(sc_channel_history_entry_t));
                            memcpy(merged_history + cross_count, history_entries,
                                   history_count * sizeof(sc_channel_history_entry_t));
                        }
                    }
                    const sc_channel_history_entry_t *ctx_entries = merged_history ? merged_history
                                                                    : history_entries
                                                                        ? history_entries
                                                                        : cross_entries;
                    size_t ctx_count = merged_history    ? total
                                       : history_entries ? history_count
                                                         : cross_count;
                    if (ctx_entries && ctx_count > 0) {
                        convo_ctx = sc_conversation_build_awareness(alloc, ctx_entries, ctx_count,
                                                                    &convo_ctx_len);
                    }
                    if (merged_history)
                        alloc->free(alloc->ctx, merged_history,
                                    total * sizeof(sc_channel_history_entry_t));
                }

                /* 2c. Length calibration fallback for channels without history.
                 * When history exists, calibration runs inside build_awareness.
                 * When it doesn't, we still want message-type guidance. */
                if (!convo_ctx && combined_len > 0) {
                    char cal_buf[1024];
                    size_t cal_len = sc_conversation_calibrate_length(combined, combined_len, NULL, 0,
                                                                      cal_buf, sizeof(cal_buf));
                    if (cal_len > 0) {
                        convo_ctx = (char *)alloc->alloc(alloc->ctx, cal_len + 1);
                        if (convo_ctx) {
                            memcpy(convo_ctx, cal_buf, cal_len);
                            convo_ctx[cal_len] = '\0';
                            convo_ctx_len = cal_len;
                        }
                    }
                }

                /* 2d. Style analysis: analyze their texting patterns for mirroring */
                char *style_ctx = NULL;
                size_t style_ctx_len = 0;
                if (history_entries && history_count > 0) {
                    style_ctx = sc_conversation_analyze_style(alloc, history_entries, history_count,
                                                              &style_ctx_len);
                }

                /* Merge style context into conversation context */
                if (style_ctx && convo_ctx) {
                    size_t merged_len = convo_ctx_len + style_ctx_len + 2;
                    char *merged = (char *)alloc->alloc(alloc->ctx, merged_len + 1);
                    if (merged) {
                        memcpy(merged, convo_ctx, convo_ctx_len);
                        merged[convo_ctx_len] = '\n';
                        memcpy(merged + convo_ctx_len + 1, style_ctx, style_ctx_len);
                        merged[merged_len - 1] = '\n';
                        merged[merged_len] = '\0';
                        alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                        convo_ctx = merged;
                        convo_ctx_len = merged_len;
                    }
                    alloc->free(alloc->ctx, style_ctx, style_ctx_len + 1);
                    style_ctx = NULL;
                } else if (style_ctx && !convo_ctx) {
                    convo_ctx = style_ctx;
                    convo_ctx_len = style_ctx_len;
                    style_ctx = NULL;
                }
                if (style_ctx)
                    alloc->free(alloc->ctx, style_ctx, style_ctx_len + 1);

                /* 6. Attachment context: guidance when attachments detected in history */
                if (history_entries && history_count > 0) {
                    size_t attach_ctx_len = 0;
                    char *attach_ctx =
                        sc_conversation_attachment_context(alloc, history_entries, history_count,
                                                          &attach_ctx_len);
                    if (attach_ctx && attach_ctx_len > 0) {
                        if (convo_ctx) {
                            size_t total = convo_ctx_len + attach_ctx_len + 2;
                            char *merged = (char *)alloc->alloc(alloc->ctx, total);
                            if (merged) {
                                memcpy(merged, convo_ctx, convo_ctx_len);
                                merged[convo_ctx_len] = '\n';
                                memcpy(merged + convo_ctx_len + 1, attach_ctx, attach_ctx_len);
                                merged[total - 1] = '\0';
                                alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                                convo_ctx = merged;
                                convo_ctx_len = total - 1;
                            }
                        } else {
                            convo_ctx = attach_ctx;
                            convo_ctx_len = attach_ctx_len;
                            attach_ctx = NULL;
                        }
                        if (attach_ctx)
                            alloc->free(alloc->ctx, attach_ctx, attach_ctx_len + 1);
                    } else if (attach_ctx) {
                        alloc->free(alloc->ctx, attach_ctx, attach_ctx_len + 1);
                    }
                }

                /* 3b. Conversation callbacks: thread callback (history) + memory-based */
                {
                    char *thread_cb = NULL;
                    size_t thread_cb_len = 0;
                    char *mem_cb = NULL;
                    size_t mem_cb_len = 0;
                    if (history_entries && history_count > 0) {
                        thread_cb = sc_conversation_build_callback(alloc, history_entries,
                                                                   history_count,
                                                                   &thread_cb_len);
                    }
                    if (agent->memory && agent->memory->vtable && agent->memory->vtable->recall) {
                        mem_cb = build_callback_context(alloc, agent->memory, batch_key, key_len,
                                                        combined, combined_len, &mem_cb_len);
                    }
                    char *cb_ctx = NULL;
                    size_t cb_len = 0;
                    if (thread_cb && mem_cb) {
                        size_t merged_len = thread_cb_len + mem_cb_len + 2;
                        cb_ctx = (char *)alloc->alloc(alloc->ctx, merged_len + 1);
                        if (cb_ctx) {
                            memcpy(cb_ctx, thread_cb, thread_cb_len);
                            cb_ctx[thread_cb_len] = '\n';
                            memcpy(cb_ctx + thread_cb_len + 1, mem_cb, mem_cb_len);
                            cb_ctx[merged_len] = '\0';
                            cb_len = merged_len;
                        }
                        alloc->free(alloc->ctx, thread_cb, thread_cb_len + 1);
                        alloc->free(alloc->ctx, mem_cb, mem_cb_len + 1);
                    } else if (thread_cb) {
                        cb_ctx = thread_cb;
                        cb_len = thread_cb_len;
                    } else if (mem_cb) {
                        cb_ctx = mem_cb;
                        cb_len = mem_cb_len;
                    }
                    if (cb_ctx && convo_ctx) {
                        size_t merged_len = convo_ctx_len + cb_len + 1;
                        char *merged = (char *)alloc->alloc(alloc->ctx, merged_len + 1);
                        if (merged) {
                            memcpy(merged, convo_ctx, convo_ctx_len);
                            merged[convo_ctx_len] = '\n';
                            memcpy(merged + convo_ctx_len + 1, cb_ctx, cb_len);
                            merged[merged_len] = '\0';
                            alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                            convo_ctx = merged;
                            convo_ctx_len = merged_len;
                        }
                        alloc->free(alloc->ctx, cb_ctx, cb_len + 1);
                    } else if (cb_ctx && !convo_ctx) {
                        convo_ctx = cb_ctx;
                        convo_ctx_len = cb_len;
                    }
                }

#ifndef SC_IS_TEST
                /* Episodic: load recent sessions for context */
                char *episodic_ctx = NULL;
                size_t episodic_ctx_len = 0;
                if (agent->memory) {
                    sc_episodic_load(agent->memory, alloc, &episodic_ctx, &episodic_ctx_len);
                }
                if (episodic_ctx && episodic_ctx_len > 0) {
                    if (convo_ctx) {
                        size_t merged_len = convo_ctx_len + episodic_ctx_len + 2;
                        char *merged = (char *)alloc->alloc(alloc->ctx, merged_len + 1);
                        if (merged) {
                            memcpy(merged, convo_ctx, convo_ctx_len);
                            merged[convo_ctx_len] = '\n';
                            memcpy(merged + convo_ctx_len + 1, episodic_ctx, episodic_ctx_len);
                            merged[merged_len - 1] = '\n';
                            merged[merged_len] = '\0';
                            alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                            convo_ctx = merged;
                            convo_ctx_len = merged_len;
                        }
                        alloc->free(alloc->ctx, episodic_ctx, episodic_ctx_len + 1);
                    } else {
                        convo_ctx = episodic_ctx;
                        convo_ctx_len = episodic_ctx_len;
                    }
                }
#endif

#ifdef SC_HAS_PERSONA
#ifndef SC_IS_TEST
                /* Replay insights: inject stored insights from previous conversation */
                if (replay_insights_len > 0) {
                    if (convo_ctx) {
                        size_t merged_len = convo_ctx_len + replay_insights_len + 2;
                        char *merged = (char *)alloc->alloc(alloc->ctx, merged_len + 1);
                        if (merged) {
                            memcpy(merged, convo_ctx, convo_ctx_len);
                            merged[convo_ctx_len] = '\n';
                            memcpy(merged + convo_ctx_len + 1, replay_insights, replay_insights_len);
                            merged[merged_len - 1] = '\n';
                            merged[merged_len] = '\0';
                            alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                            convo_ctx = merged;
                            convo_ctx_len = merged_len;
                        }
                    } else {
                        convo_ctx = (char *)alloc->alloc(alloc->ctx, replay_insights_len + 1);
                        if (convo_ctx) {
                            memcpy(convo_ctx, replay_insights, replay_insights_len);
                            convo_ctx[replay_insights_len] = '\0';
                            convo_ctx_len = replay_insights_len;
                        }
                    }
                }
#endif
#endif

                /* 4. Response constraints via channel vtable */
                uint32_t max_chars = 0;
                if (ch->channel->vtable->get_response_constraints) {
                    sc_channel_response_constraints_t constraints = {0};
                    if (ch->channel->vtable->get_response_constraints(ch->channel->ctx,
                                                                      &constraints) == SC_OK) {
                        max_chars = constraints.max_chars;
                    }
                }

                /* Brief mode: force ultra-short response */
                if (brief_mode && max_chars > 50)
                    max_chars = 50;

                /* Honesty guardrail: inject if they asked "did you do X?" */
                {
                    char *honesty = sc_conversation_honesty_check(alloc, combined, combined_len);
                    if (honesty && convo_ctx) {
                        size_t h_len = strlen(honesty);
                        size_t total = convo_ctx_len + h_len + 2;
                        char *merged = (char *)alloc->alloc(alloc->ctx, total);
                        if (merged) {
                            memcpy(merged, convo_ctx, convo_ctx_len);
                            merged[convo_ctx_len] = '\n';
                            memcpy(merged + convo_ctx_len + 1, honesty, h_len);
                            merged[total - 1] = '\0';
                            alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                            convo_ctx = merged;
                            convo_ctx_len = total - 1;
                        }
                        alloc->free(alloc->ctx, honesty, h_len + 1);
                    } else if (honesty && !convo_ctx) {
                        convo_ctx = honesty;
                        convo_ctx_len = strlen(honesty);
                    } else if (honesty) {
                        alloc->free(alloc->ctx, honesty, strlen(honesty) + 1);
                    }
                }

                /* 5. Anti-repetition: detect patterns in our recent messages */
                if (history_entries && history_count >= 4) {
                    char rep_buf[1024];
                    size_t rep_len = sc_conversation_detect_repetition(
                        history_entries, history_count, rep_buf, sizeof(rep_buf));
                    if (rep_len > 0 && convo_ctx) {
                        size_t total = convo_ctx_len + rep_len + 1;
                        char *merged = (char *)alloc->alloc(alloc->ctx, total + 1);
                        if (merged) {
                            memcpy(merged, convo_ctx, convo_ctx_len);
                            merged[convo_ctx_len] = '\n';
                            memcpy(merged + convo_ctx_len + 1, rep_buf, rep_len);
                            merged[total] = '\0';
                            alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                            convo_ctx = merged;
                            convo_ctx_len = total;
                        }
                    } else if (rep_len > 0 && !convo_ctx) {
                        convo_ctx = (char *)alloc->alloc(alloc->ctx, rep_len + 1);
                        if (convo_ctx) {
                            memcpy(convo_ctx, rep_buf, rep_len);
                            convo_ctx[rep_len] = '\0';
                            convo_ctx_len = rep_len;
                        }
                    }
                }

                /* 6. Relationship-tier calibration from contact profile */
#ifdef SC_HAS_PERSONA
                if (agent->persona) {
                    const sc_contact_profile_t *cp_rel =
                        sc_persona_find_contact(agent->persona, batch_key, key_len);
                    if (cp_rel && (cp_rel->relationship_stage || cp_rel->warmth_level ||
                                   cp_rel->vulnerability_level)) {
                        char rel_buf[512];
                        size_t rel_len = sc_conversation_calibrate_relationship(
                            cp_rel->relationship_stage, cp_rel->warmth_level,
                            cp_rel->vulnerability_level, rel_buf, sizeof(rel_buf));
                        if (rel_len > 0 && convo_ctx) {
                            size_t total = convo_ctx_len + rel_len + 1;
                            char *merged = (char *)alloc->alloc(alloc->ctx, total + 1);
                            if (merged) {
                                memcpy(merged, convo_ctx, convo_ctx_len);
                                merged[convo_ctx_len] = '\n';
                                memcpy(merged + convo_ctx_len + 1, rel_buf, rel_len);
                                merged[total] = '\0';
                                alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                                convo_ctx = merged;
                                convo_ctx_len = total;
                            }
                        } else if (rel_len > 0 && !convo_ctx) {
                            convo_ctx = (char *)alloc->alloc(alloc->ctx, rel_len + 1);
                            if (convo_ctx) {
                                memcpy(convo_ctx, rel_buf, rel_len);
                                convo_ctx[rel_len] = '\0';
                                convo_ctx_len = rel_len;
                            }
                        }
                    }
                }
#endif

                /* 6b. Link sharing context: when conversation calls for sharing a link */
                if (sc_conversation_should_share_link(combined, combined_len, history_entries,
                                                       history_count)) {
                    static const char LINK_CTX[] =
                        "\n### Link Sharing\nThe conversation naturally calls for sharing a link "
                        "or recommendation. If you have a relevant URL, include it in your "
                        "response.\n";
                    size_t link_len = sizeof(LINK_CTX) - 1;
                    if (convo_ctx) {
                        size_t total = convo_ctx_len + link_len + 1;
                        char *merged = (char *)alloc->alloc(alloc->ctx, total + 1);
                        if (merged) {
                            memcpy(merged, convo_ctx, convo_ctx_len);
                            memcpy(merged + convo_ctx_len, LINK_CTX, link_len + 1);
                            alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                            convo_ctx = merged;
                            convo_ctx_len = total;
                        }
                    } else {
                        convo_ctx = (char *)alloc->alloc(alloc->ctx, link_len + 1);
                        if (convo_ctx) {
                            memcpy(convo_ctx, LINK_CTX, link_len + 1);
                            convo_ctx_len = link_len;
                        }
                    }
                }

                /* 7. Emotional topic map: topics → dominant emotions from STM */
                {
                    sc_emotional_graph_t egraph;
                    size_t egraph_len = 0;
                    sc_egraph_init(&egraph, *alloc);
                    (void)sc_egraph_populate_from_stm(&egraph, &agent->stm);
                    char *egraph_ctx = sc_egraph_build_context(alloc, &egraph, &egraph_len);
                    sc_egraph_deinit(&egraph);
                    if (egraph_ctx && egraph_len > 0) {
                        if (convo_ctx) {
                            size_t total = convo_ctx_len + egraph_len + 2;
                            char *merged = (char *)alloc->alloc(alloc->ctx, total);
                            if (merged) {
                                memcpy(merged, convo_ctx, convo_ctx_len);
                                merged[convo_ctx_len] = '\n';
                                memcpy(merged + convo_ctx_len + 1, egraph_ctx, egraph_len + 1);
                                alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                                convo_ctx = merged;
                                convo_ctx_len = total - 1;
                            }
                        } else {
                            convo_ctx = egraph_ctx;
                            convo_ctx_len = egraph_len;
                        }
                        if (convo_ctx != egraph_ctx)
                            alloc->free(alloc->ctx, egraph_ctx, egraph_len + 1);
                    } else if (egraph_ctx) {
                        alloc->free(alloc->ctx, egraph_ctx, egraph_len + 1);
                    }
                }

                /* 7b. Mood context: recent emotional state from memory */
                {
                    char *mood_ctx = NULL;
                    size_t mood_ctx_len = 0;
                    if (agent->memory &&
                        sc_mood_build_context(alloc, agent->memory, batch_key, key_len,
                                              &mood_ctx, &mood_ctx_len) == SC_OK &&
                        mood_ctx && mood_ctx_len > 0) {
                        if (convo_ctx) {
                            size_t total = convo_ctx_len + mood_ctx_len + 2;
                            char *merged = (char *)alloc->alloc(alloc->ctx, total);
                            if (merged) {
                                memcpy(merged, convo_ctx, convo_ctx_len);
                                merged[convo_ctx_len] = '\n';
                                memcpy(merged + convo_ctx_len + 1, mood_ctx, mood_ctx_len);
                                merged[total - 1] = '\0';
                                alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                                convo_ctx = merged;
                                convo_ctx_len = total - 1;
                            }
                        } else {
                            convo_ctx = mood_ctx;
                            convo_ctx_len = mood_ctx_len;
                            mood_ctx = NULL;
                        }
                        if (mood_ctx)
                            alloc->free(alloc->ctx, mood_ctx, mood_ctx_len + 1);
                    }
                }

                /* Set agent per-turn context fields (prompt builder reads these) */
                agent->contact_context = contact_ctx;
                agent->contact_context_len = contact_ctx_len;
                agent->conversation_context = convo_ctx;
                agent->conversation_context_len = convo_ctx_len;
                agent->ab_history_entries = history_entries;
                agent->ab_history_count = history_count;
                agent->max_response_chars = max_chars;

                /* Scope memory to this contact */
                agent->memory_session_id = batch_key;
                agent->memory_session_id_len = key_len;
                if (agent->memory && agent->memory->vtable) {
                    agent->memory->current_session_id = batch_key;
                    agent->memory->current_session_id_len = key_len;
                }

                /* Start typing indicator before LLM call */
                if (ch->channel->vtable->start_typing) {
                    ch->channel->vtable->start_typing(ch->channel->ctx, batch_key, key_len);
                }

                /* Thinking response: send filler first if message warrants it */
                {
                    sc_thinking_response_t thinking;
                    memset(&thinking, 0, sizeof(thinking));
                    bool needs_thinking =
                        sc_conversation_classify_thinking(combined, combined_len, history_entries,
                                                          history_count, &thinking,
                                                          (uint32_t)time(NULL));
                    if (needs_thinking && thinking.filler_len > 0) {
                        ch->channel->vtable->send(ch->channel->ctx, batch_key, key_len,
                                                  thinking.filler, thinking.filler_len, NULL, 0);
#ifndef SC_IS_TEST
                        usleep((useconds_t)(thinking.delay_ms * 1000));
#endif
                    }
                }

                /* Tapback reactions: check if reaction is more appropriate before text */
                if (ch->channel->vtable->react) {
                    sc_reaction_type_t reaction = sc_conversation_classify_reaction(
                        combined, combined_len, false, history_entries, history_count,
                        (uint32_t)time(NULL));
                    if (reaction != SC_REACTION_NONE) {
                        int64_t msg_id = msgs[batch_end].message_id;
                        if (msg_id > 0) {
                            ch->channel->vtable->react(ch->channel->ctx, batch_key, key_len,
                                                        msg_id, reaction);
                        }
                    }
                }
#endif

                sc_error_t err =
                    sc_agent_turn(agent, combined, combined_len, &response, &response_len);

#ifndef SC_IS_TEST
                /* Stop typing indicator after LLM call */
                if (ch->channel->vtable->stop_typing) {
                    ch->channel->vtable->stop_typing(ch->channel->ctx, batch_key, key_len);
                }

                /* Quality gate: check response for unnatural patterns.
                 * Run before freeing history so the evaluator has full context.
                 * If needs_revision, log it — the reflection system inside
                 * agent_turn handles retries, this catches what slips through. */
                if (err == SC_OK && response && response_len > 0 && history_entries) {
                    sc_quality_score_t qscore = sc_conversation_evaluate_quality(
                        response, response_len, history_entries, history_count, max_chars);
                    if (qscore.needs_revision) {
                        fprintf(stderr,
                                "[seaclaw] quality warning: score=%d (b=%d v=%d w=%d n=%d) "
                                "for %.40s...\n",
                                qscore.total, qscore.brevity, qscore.validation, qscore.warmth,
                                qscore.naturalness,
                                response_len > 40 ? response : response);
                    }
                }

#ifdef SC_HAS_PERSONA
                /* Replay learning: analyze conversation and store insights for future prompts */
                if (history_entries && history_count > 0) {
                    sc_replay_result_t replay = {0};
                    sc_error_t rerr =
                        sc_replay_analyze(alloc, history_entries, history_count, 2000, &replay);
                    if (rerr == SC_OK) {
                        size_t rctx_len = 0;
                        char *rctx = sc_replay_build_context(alloc, &replay, &rctx_len);
                        if (rctx && rctx_len > 0) {
#ifndef SC_IS_TEST
                            if (rctx_len < sizeof(replay_insights)) {
                                memcpy(replay_insights, rctx, rctx_len);
                                replay_insights[rctx_len] = '\0';
                                replay_insights_len = rctx_len;
                            }
#endif
                            if (history_count > 2 && agent->memory &&
                                agent->memory->vtable && agent->memory->vtable->store) {
                                static const char cat_name[] = "replay_insights";
                                sc_memory_category_t cat = {
                                    .tag = SC_MEMORY_CATEGORY_CUSTOM,
                                    .data.custom = {.name = cat_name, .name_len = sizeof(cat_name) - 1},
                                };
                                agent->memory->vtable->store(agent->memory->ctx, "replay:latest", 13,
                                                            rctx, rctx_len, &cat, batch_key, key_len);
                            }
                        }
                        if (rctx)
                            alloc->free(alloc->ctx, rctx, rctx_len + 1);
                    }
                    sc_replay_result_deinit(&replay, alloc);
                }
#endif
#endif

                /* Clear per-turn context and free */
#ifndef SC_IS_TEST
                agent->contact_context = NULL;
                agent->contact_context_len = 0;
                agent->conversation_context = NULL;
                agent->conversation_context_len = 0;
                agent->ab_history_entries = NULL;
                agent->ab_history_count = 0;
                agent->max_response_chars = 0;
                agent->memory_session_id = NULL;
                agent->memory_session_id_len = 0;
                if (agent->memory && agent->memory->vtable) {
                    agent->memory->current_session_id = NULL;
                    agent->memory->current_session_id_len = 0;
                }
                if (contact_ctx)
                    alloc->free(alloc->ctx, contact_ctx, contact_ctx_len + 1);
                if (convo_ctx)
                    alloc->free(alloc->ctx, convo_ctx, convo_ctx_len + 1);
                if (history_entries)
                    alloc->free(alloc->ctx, history_entries,
                                history_count * sizeof(sc_channel_history_entry_t));
                if (cross_entries)
                    alloc->free(alloc->ctx, cross_entries,
                                cross_count * sizeof(sc_channel_history_entry_t));
#endif

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

                /* Store conversation summary as long-term memory */
                if (err == SC_OK && response && response_len > 0 && agent->memory) {
                    store_conversation_summary(alloc, agent->memory, batch_key, key_len,
                                               combined, combined_len, response, response_len);
                }

#ifndef SC_IS_TEST
                /* Episodic: summarize this interaction */
                if (err == SC_OK && response && response_len > 0 && agent->memory &&
                    agent->provider.vtable) {
                    const char *ep_msgs[2] = {combined, response};
                    size_t ep_lens[2] = {combined_len, response_len};
                    size_t summary_len = 0;
                    char *summary =
                        sc_episodic_summarize_session(alloc, ep_msgs, ep_lens, 2, &summary_len);
                    if (summary && summary_len > 0) {
                        sc_episodic_store(agent->memory, alloc, batch_key, key_len, summary,
                                         summary_len);
                        alloc->free(alloc->ctx, summary, summary_len + 1);
                    } else if (summary) {
                        alloc->free(alloc->ctx, summary, summary_len + 1);
                    }
                }

                /* Promote STM entities to persistent memory every 5 turns */
                if (err == SC_OK && ++promotion_counter % 5 == 0 && agent->stm.turn_count > 0 &&
                    agent->memory) {
                    sc_promotion_config_t promo_cfg = {
                        .min_mention_count = 2,
                        .min_importance = 0.3,
                        .max_entities = 10,
                    };
                    sc_promotion_run(alloc, &agent->stm, agent->memory, &promo_cfg);
                    sc_promotion_run_emotions(alloc, &agent->stm, agent->memory, batch_key, key_len);
                }
#endif

                /* Emotion promotion: promote emotions from STM to long-term memory */
                if (err == SC_OK && response && response_len > 0 && agent->stm.turn_count > 0 &&
                    agent->memory && agent->memory->vtable && agent->memory->vtable->store) {
                    sc_promotion_run_emotions(alloc, &agent->stm, agent->memory, batch_key,
                                              key_len);
                }

                size_t response_alloc_len = response_len;
                uint32_t typo_seed = 0;
                if (err == SC_OK && response && response_len > 0) {
#ifndef SC_IS_TEST
#ifdef SC_HAS_PERSONA
                    /* Apply typing quirks from persona overlay as post-processing.
                     * This shrinks the buffer in-place; keep original size for free. */
                    const sc_persona_overlay_t *overlay = NULL;
                    if (agent->persona && agent->active_channel) {
                        overlay = sc_persona_find_overlay(agent->persona, agent->active_channel,
                                                          agent->active_channel_len);
                        if (overlay && overlay->typing_quirks && overlay->typing_quirks_count > 0) {
                            response_len = sc_conversation_apply_typing_quirks(
                                response, response_len,
                                (const char *const *)overlay->typing_quirks,
                                overlay->typing_quirks_count);
                        }
                    }

                    /* Typo simulation: requires "occasional_typos" quirk and buffer capacity */
                    bool has_typo_quirk = false;
                    if (overlay && overlay->typing_quirks) {
                        for (size_t tq = 0; tq < overlay->typing_quirks_count; tq++) {
                            if (overlay->typing_quirks[tq] &&
                                strcmp(overlay->typing_quirks[tq], "occasional_typos") == 0) {
                                has_typo_quirk = true;
                                break;
                            }
                        }
                    }

                    char *original_response = NULL;
                    size_t original_len = response_len;
                    if (has_typo_quirk && response && response_len > 0) {
                        original_response = (char *)alloc->alloc(alloc->ctx, response_len + 1);
                        if (original_response) {
                            memcpy(original_response, response, response_len);
                            original_response[response_len] = '\0';
                        }
                    }

                    if (has_typo_quirk && response && response_len > 0) {
                        size_t cap = response_alloc_len + 1;
                        if (cap <= response_len + 1) {
                            char *grown = (char *)alloc->realloc(alloc->ctx, response,
                                                                 response_alloc_len + 1,
                                                                 response_len + 2);
                            if (grown) {
                                response = grown;
                                response_alloc_len = response_len + 1;
                                cap = response_len + 2;
                            }
                        }
                        if (cap > response_len + 1) {
                            typo_seed = (uint32_t)time(NULL);
                            size_t new_len =
                                sc_conversation_apply_typos(response, response_len, cap, typo_seed);
                            response_len = new_len;
                        }
                    }
                    if (original_response) {
                        if (has_typo_quirk && typo_seed != 0 && response && response_len > 0) {
                            char correction[128];
                            size_t corr_len = sc_conversation_generate_correction(
                                original_response, original_len, response, response_len, correction,
                                sizeof(correction), typo_seed + 1, 40);
                            if (corr_len > 0) {
                                unsigned int delay_ms =
                                    2500 + (unsigned int)(typo_seed % 2500);
                                usleep((useconds_t)(delay_ms * 1000));
                                ch->channel->vtable->send(ch->channel->ctx, batch_key, key_len,
                                                          correction, corr_len, NULL, 0);
                            }
                        }
                        alloc->free(alloc->ctx, original_response, original_len + 1);
                    }
#endif
                    /* Split response into natural multi-message fragments */
                    sc_message_fragment_t fragments[4];
                    size_t frag_count =
                        sc_conversation_split_response(alloc, response, response_len, fragments, 4);
                    if (frag_count > 0) {
                        for (size_t f = 0; f < frag_count; f++) {
                            if (f > 0 && fragments[f].delay_ms > 0)
                                usleep(fragments[f].delay_ms * 1000);
                            ch->channel->vtable->send(ch->channel->ctx, batch_key, key_len,
                                                      fragments[f].text, fragments[f].text_len,
                                                      NULL, 0);
                        }
                        for (size_t f = 0; f < frag_count; f++) {
                            if (fragments[f].text)
                                alloc->free(alloc->ctx, fragments[f].text,
                                            fragments[f].text_len + 1);
                        }
                    } else {
                        ch->channel->vtable->send(ch->channel->ctx, batch_key, key_len, response,
                                                  response_len, NULL, 0);
                    }
#else
                    ch->channel->vtable->send(ch->channel->ctx, batch_key, key_len, response,
                                              response_len, NULL, 0);
#endif
                }
                if (response) {
                    alloc->free(alloc->ctx, response, response_alloc_len + 1);
                }
            }
        }

        struct timespec sleep_ts = {.tv_sec = tick_interval_ms / 1000,
                                    .tv_nsec = (long)(tick_interval_ms % 1000) * 1000000L};
        nanosleep(&sleep_ts, NULL);
    }

#undef SC_STOP_FLAG
    if (agent && agent->outcomes == &daemon_outcomes)
        agent->outcomes = NULL;
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
