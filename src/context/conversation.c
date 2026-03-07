#include "seaclaw/context/conversation.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CTX_BUF_CAP 16384

char *sc_conversation_build_awareness(sc_allocator_t *alloc,
                                      const sc_channel_history_entry_t *entries, size_t count,
                                      size_t *out_len) {
    if (!alloc || !out_len)
        return NULL;
    *out_len = 0;
    if (!entries || count == 0)
        return NULL;

    char *buf = (char *)alloc->alloc(alloc->ctx, CTX_BUF_CAP);
    if (!buf)
        return NULL;

    size_t pos = 0;
    int w;

    /* ── Conversation thread ─────────────────────────────────────────── */
    w = snprintf(buf + pos, CTX_BUF_CAP - pos, "\n--- Recent conversation thread ---\n");
    if (w > 0)
        pos += (size_t)w;

    for (size_t i = 0; i < count; i++) {
        const char *who = entries[i].from_me ? "You" : "Them";
        w = snprintf(buf + pos, CTX_BUF_CAP - pos, "[%s] %s: %s\n", entries[i].timestamp, who,
                     entries[i].text);
        if (w > 0 && pos + (size_t)w < CTX_BUF_CAP)
            pos += (size_t)w;
    }

    w = snprintf(buf + pos, CTX_BUF_CAP - pos, "--- End of recent thread ---\n\n");
    if (w > 0)
        pos += (size_t)w;

    /* ── Emotional / situational analysis ────────────────────────────── */
    {
        bool they_seem_frustrated = false;
        bool they_seem_excited = false;
        bool they_seem_sad = false;
        bool open_question = false;
        bool they_sent_link = false;
        bool logistics_thread = false;

        for (size_t i = 0; i < count; i++) {
            const char *t = entries[i].text;
            size_t tl = strlen(t);
            if (!entries[i].from_me) {
                for (size_t j = 0; j < tl; j++) {
                    if (t[j] == '?')
                        open_question = true;
                    if (t[j] == '!' && tl > 3)
                        they_seem_excited = true;
                }
                for (size_t j = 0; j + 3 < tl; j++) {
                    char lo[5];
                    for (int k = 0; k < 4 && j + (size_t)k < tl; k++) {
                        lo[k] = t[j + k];
                        if (lo[k] >= 'A' && lo[k] <= 'Z')
                            lo[k] += 32;
                    }
                    lo[4] = 0;
                    if (strncmp(lo, "damn", 4) == 0 || strncmp(lo, "ugh", 3) == 0)
                        they_seem_frustrated = true;
                    if (strncmp(lo, "sad", 3) == 0 || strncmp(lo, "cry", 3) == 0)
                        they_seem_sad = true;
                }
                for (size_t j = 0; j + 4 < tl; j++) {
                    if (memcmp(t + j, "http", 4) == 0)
                        they_sent_link = true;
                }
            }
            for (size_t j = 0; j + 5 < tl; j++) {
                char lo[6];
                for (int k = 0; k < 5 && j + (size_t)k < tl; k++) {
                    lo[k] = t[j + k];
                    if (lo[k] >= 'A' && lo[k] <= 'Z')
                        lo[k] += 32;
                }
                lo[5] = 0;
                if (strcmp(lo, "fligh") == 0 || strcmp(lo, "airpo") == 0 ||
                    strcmp(lo, "leavi") == 0 || strcmp(lo, "booke") == 0 ||
                    strcmp(lo, "arriv") == 0)
                    logistics_thread = true;
            }
        }

        w = snprintf(buf + pos, CTX_BUF_CAP - pos, "--- Conversation awareness ---\n");
        if (w > 0)
            pos += (size_t)w;

        /* Time-of-day context triggers */
        {
            time_t now = time(NULL);
            struct tm *lt = localtime(&now);
            if (lt) {
                int hour = lt->tm_hour;
                int wday = lt->tm_wday;
                if (hour >= 0 && hour < 6) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "TIME: Very late at night. Be gentler, softer, no pressure. "
                                 "Short responses.\n");
                } else if (hour >= 6 && hour < 9) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "TIME: Early morning. Keep it light and warm.\n");
                } else if (hour >= 21) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "TIME: Evening/night. Relaxed energy, shorter texts.\n");
                } else {
                    w = 0;
                }
                if (w > 0)
                    pos += (size_t)w;

                if (wday == 1) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "DAY: Monday. Be a bit more supportive.\n");
                } else if (wday == 5) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "DAY: Friday. Weekend energy, lighter and upbeat.\n");
                } else if (wday == 0 && hour >= 17) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos, "DAY: Sunday evening. Be gentle.\n");
                } else if (wday == 0 || wday == 6) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos, "DAY: Weekend. Relaxed vibe.\n");
                } else {
                    w = 0;
                }
                if (w > 0)
                    pos += (size_t)w;
            }
        }

        if (they_seem_frustrated) {
            w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                         "They seem frustrated. Be calm, acknowledge it.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (they_seem_excited) {
            w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                         "They seem excited. Be genuinely happy for them.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (they_seem_sad) {
            w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                         "They seem sad or down. Be present and gentle.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (open_question) {
            w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                         "They asked a question. Make sure you answer it.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (they_sent_link) {
            w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                         "They shared a link. Acknowledge or comment on it.\n");
            if (w > 0)
                pos += (size_t)w;
        }
        if (logistics_thread) {
            w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                         "Active logistics/travel thread. Stay on topic.\n");
            if (w > 0)
                pos += (size_t)w;
        }

        /* Detected state analysis */
        {
            bool seems_rushed = false;
            bool seems_tired = false;
            size_t short_msg_count = 0;
            size_t their_recent = 0;
            size_t start = count > 5 ? count - 5 : 0;
            for (size_t i = start; i < count; i++) {
                if (!entries[i].from_me) {
                    their_recent++;
                    if (strlen(entries[i].text) < 15)
                        short_msg_count++;
                }
            }
            if (their_recent >= 3 && short_msg_count >= 2)
                seems_rushed = true;
            {
                time_t now = time(NULL);
                struct tm *lt = localtime(&now);
                if (lt && (lt->tm_hour >= 23 || lt->tm_hour < 5)) {
                    if (count > 0 && !entries[count - 1].from_me &&
                        strlen(entries[count - 1].text) < 30)
                        seems_tired = true;
                }
            }
            if (seems_rushed) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "STATE: They seem rushed. Get to the point.\n");
                if (w > 0)
                    pos += (size_t)w;
            }
            if (seems_tired) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "STATE: They might be tired. Be brief and gentle.\n");
                if (w > 0)
                    pos += (size_t)w;
            }
        }

        /* Verbosity mirroring */
        {
            size_t their_total_chars = 0;
            size_t their_msg_count = 0;
            for (size_t i = 0; i < count; i++) {
                if (!entries[i].from_me && strlen(entries[i].text) > 2) {
                    their_total_chars += strlen(entries[i].text);
                    their_msg_count++;
                }
            }
            if (their_msg_count > 0) {
                size_t avg_len = their_total_chars / their_msg_count;
                if (avg_len < 30) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "RESPONSE LENGTH: They text very briefly (avg %zu chars). "
                                 "Match — 1 sentence max, under 50 chars.\n",
                                 avg_len);
                } else if (avg_len < 80) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "RESPONSE LENGTH: Short messages (avg %zu chars). "
                                 "1-2 short sentences, under 150 chars.\n",
                                 avg_len);
                } else {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "RESPONSE LENGTH: Longer messages (avg %zu chars). "
                                 "2-3 sentences max, under 250 chars.\n",
                                 avg_len);
                }
                if (w > 0)
                    pos += (size_t)w;
            }
        }

        /* Conversation phase detection */
        {
            size_t exchanges = 0;
            bool last_was_me = false;
            for (size_t i = 0; i < count; i++) {
                if (i == 0 || entries[i].from_me != last_was_me)
                    exchanges++;
                last_was_me = entries[i].from_me;
            }
            if (exchanges <= 2) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "PHASE: Opening. Keep it light and warm. Short greeting.\n");
            } else if (exchanges <= 8) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "PHASE: Engaged. Build on what they said. Match depth.\n");
            } else {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "PHASE: Deep/winding. Been texting a while. Ok to be briefer.\n");
            }
            if (w > 0)
                pos += (size_t)w;
        }

        /* Real-time noticing (VoiceAI realtime-noticing) */
        {
            /* Detect energy drop: their messages getting shorter over time */
            if (count >= 6) {
                size_t first_half_len = 0, first_half_n = 0;
                size_t second_half_len = 0, second_half_n = 0;
                size_t mid = count / 2;
                for (size_t i = 0; i < count; i++) {
                    if (!entries[i].from_me) {
                        size_t tl = strlen(entries[i].text);
                        if (i < mid) {
                            first_half_len += tl;
                            first_half_n++;
                        } else {
                            second_half_len += tl;
                            second_half_n++;
                        }
                    }
                }
                if (first_half_n > 0 && second_half_n > 0) {
                    size_t avg1 = first_half_len / first_half_n;
                    size_t avg2 = second_half_len / second_half_n;
                    if (avg1 > 40 && avg2 < avg1 / 2) {
                        w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                     "NOTICE: Their messages are getting shorter "
                                     "(energy dropping). Be gentler, check in.\n");
                        if (w > 0)
                            pos += (size_t)w;
                    }
                }
            }

            /* Detect repeated theme: same words appearing 3+ times across their messages */
            {
                typedef struct {
                    char word[32];
                    int hits;
                } word_freq_t;
                word_freq_t freq[16];
                size_t freq_n = 0;
                for (size_t i = 0; i < count; i++) {
                    if (entries[i].from_me)
                        continue;
                    const char *t = entries[i].text;
                    size_t tl = strlen(t);
                    size_t wi = 0;
                    while (wi < tl) {
                        while (wi < tl && (t[wi] == ' ' || t[wi] == '\n'))
                            wi++;
                        size_t ws = wi;
                        while (wi < tl && t[wi] != ' ' && t[wi] != '\n')
                            wi++;
                        size_t wlen = wi - ws;
                        if (wlen < 4 || wlen > 30)
                            continue;
                        char word[32];
                        for (size_t k = 0; k < wlen && k < 31; k++) {
                            word[k] = t[ws + k];
                            if (word[k] >= 'A' && word[k] <= 'Z')
                                word[k] += 32;
                        }
                        word[wlen < 31 ? wlen : 31] = '\0';
                        /* Skip common words */
                        if (strcmp(word, "the") == 0 || strcmp(word, "and") == 0 ||
                            strcmp(word, "that") == 0 || strcmp(word, "this") == 0 ||
                            strcmp(word, "with") == 0 || strcmp(word, "have") == 0 ||
                            strcmp(word, "just") == 0 || strcmp(word, "like") == 0 ||
                            strcmp(word, "know") == 0 || strcmp(word, "what") == 0 ||
                            strcmp(word, "from") == 0 || strcmp(word, "about") == 0)
                            continue;
                        bool found = false;
                        for (size_t f = 0; f < freq_n; f++) {
                            if (strcmp(freq[f].word, word) == 0) {
                                freq[f].hits++;
                                found = true;
                                break;
                            }
                        }
                        if (!found && freq_n < 16) {
                            memcpy(freq[freq_n].word, word, strlen(word) + 1);
                            freq[freq_n].hits = 1;
                            freq_n++;
                        }
                    }
                }
                for (size_t f = 0; f < freq_n; f++) {
                    if (freq[f].hits >= 3) {
                        w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                     "NOTICE: They keep mentioning '%s' "
                                     "(%d times). This matters to them.\n",
                                     freq[f].word, freq[f].hits);
                        if (w > 0)
                            pos += (size_t)w;
                        break;
                    }
                }
            }

            /* Detect topic deflection: they asked you something, you answered,
             * they immediately changed topic without acknowledging */
            if (count >= 3) {
                for (size_t i = 2; i < count; i++) {
                    if (i >= 2 && !entries[i - 2].from_me && entries[i - 1].from_me &&
                        !entries[i].from_me) {
                        const char *q = entries[i - 2].text;
                        const char *r = entries[i].text;
                        bool had_question = false;
                        for (size_t j = 0; j < strlen(q); j++)
                            if (q[j] == '?')
                                had_question = true;
                        if (had_question && strlen(r) > 10) {
                            bool shares_words = false;
                            /* Quick check: does the follow-up share any significant word with the
                             * question? */
                            for (size_t j = 0; j + 4 < strlen(q); j++) {
                                char chunk[6];
                                for (int k = 0; k < 5; k++) {
                                    chunk[k] = q[j + k];
                                    if (chunk[k] >= 'A' && chunk[k] <= 'Z')
                                        chunk[k] += 32;
                                }
                                chunk[5] = '\0';
                                /* Check if this 5-char chunk appears in the follow-up */
                                for (size_t k = 0; k + 4 < strlen(r); k++) {
                                    char rc[6];
                                    for (int l = 0; l < 5; l++) {
                                        rc[l] = r[k + l];
                                        if (rc[l] >= 'A' && rc[l] <= 'Z')
                                            rc[l] += 32;
                                    }
                                    rc[5] = '\0';
                                    if (strcmp(chunk, rc) == 0) {
                                        shares_words = true;
                                        break;
                                    }
                                }
                                if (shares_words)
                                    break;
                            }
                            if (!shares_words) {
                                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                             "NOTICE: They may have deflected from a topic. "
                                             "Don't force it — follow their lead.\n");
                                if (w > 0)
                                    pos += (size_t)w;
                                break;
                            }
                        }
                    }
                }
            }
        }

        w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                     "\nUse this context naturally. Reference specific details they mentioned. "
                     "Do NOT summarize or acknowledge this context aloud.\n");
        if (w > 0)
            pos += (size_t)w;
    }

    buf[pos] = '\0';
    *out_len = pos;
    return buf;
}
