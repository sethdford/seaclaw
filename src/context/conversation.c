#include "seaclaw/context/conversation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CTX_BUF_CAP 16384

static bool str_contains_ci(const char *haystack, size_t hlen, const char *needle)
    __attribute__((unused));
static bool str_contains_ci(const char *haystack, size_t hlen, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen > hlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z')
                a += 32;
            if (b >= 'A' && b <= 'Z')
                b += 32;
            if (a != b) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

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

        /* Narrative arc tracking */
        {
            sc_narrative_phase_t arc = sc_conversation_detect_narrative(entries, count);
            const char *arc_labels[] = {
                "OPENING — Keep light, warm greeting. Don't go deep yet.",
                "BUILDING — They're engaged. Match their energy, build rapport.",
                "APPROACHING CLIMAX — Conversation is intensifying. "
                "Be present, listen more than you talk.",
                "PEAK — Emotional peak. Hold space. Validate. Don't fix. "
                "This is where real connection happens.",
                "RELEASE — Intensity is easing. Lighter touches. "
                "Maybe humor if appropriate.",
                "CLOSING — They're wrapping up. Keep it brief, warm sign-off. "
                "Don't start new topics.",
            };
            w = snprintf(buf + pos, CTX_BUF_CAP - pos, "ARC: %s\n", arc_labels[arc]);
            if (w > 0)
                pos += (size_t)w;
        }

        /* Engagement scoring */
        {
            sc_engagement_level_t eng = sc_conversation_detect_engagement(entries, count);
            if (eng == SC_ENGAGEMENT_LOW) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "ENGAGEMENT: Low. Try changing topic, share something "
                             "surprising, or ask a genuine question.\n");
                if (w > 0)
                    pos += (size_t)w;
            } else if (eng == SC_ENGAGEMENT_DISTRACTED) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "ENGAGEMENT: Distracted. They may be busy. "
                             "Keep responses ultra-short or let them re-engage.\n");
                if (w > 0)
                    pos += (size_t)w;
            } else if (eng == SC_ENGAGEMENT_HIGH) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "ENGAGEMENT: High. They're invested. Match depth.\n");
                if (w > 0)
                    pos += (size_t)w;
            }
        }

        /* Emotional state */
        {
            sc_emotional_state_t emo = sc_conversation_detect_emotion(entries, count);
            if (emo.intensity > 0.3f) {
                w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                             "EMOTION: They seem %s (intensity: %.1f). ", emo.dominant_emotion,
                             (double)emo.intensity);
                if (w > 0)
                    pos += (size_t)w;
                if (emo.concerning) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "This is concerning. Be gentle and present. "
                                 "Check in directly.\n");
                } else if (emo.valence < -0.3f) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos,
                                 "Acknowledge their feelings before anything else.\n");
                } else if (emo.valence > 0.3f) {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos, "Share in their positive energy.\n");
                } else {
                    w = snprintf(buf + pos, CTX_BUF_CAP - pos, "\n");
                }
                if (w > 0)
                    pos += (size_t)w;
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

/* ── Conversation Quality Evaluator ─────────────────────────────────── */

sc_quality_score_t sc_conversation_evaluate_quality(const char *response, size_t response_len,
                                                    const sc_channel_history_entry_t *entries,
                                                    size_t count, uint32_t max_chars) {
    sc_quality_score_t score = {0, 0, 0, 0, 0, false};
    if (!response || response_len == 0)
        return score;

    /* Brevity (0-25): shorter is better for text channels */
    if (max_chars > 0) {
        if (response_len <= max_chars / 2)
            score.brevity = 25;
        else if (response_len <= max_chars)
            score.brevity = 20;
        else if (response_len <= max_chars * 2)
            score.brevity = 10;
        else
            score.brevity = 0;
    } else {
        score.brevity = response_len < 200 ? 25 : response_len < 500 ? 15 : 5;
    }

    /* Validation (0-25): does the response reflect/acknowledge their feelings? */
    {
        bool has_validation = false;
        const char *validators[] = {"that makes sense",
                                    "i get it",
                                    "i understand",
                                    "of course",
                                    "that sounds",
                                    "no wonder",
                                    "totally",
                                    "i hear you",
                                    "i bet",
                                    "i can imagine",
                                    "that must",
                                    "sorry to hear",
                                    "i'm sorry",
                                    "proud of you",
                                    "you got this",
                                    NULL};
        for (int i = 0; validators[i]; i++) {
            if (str_contains_ci(response, response_len, validators[i])) {
                has_validation = true;
                break;
            }
        }
        /* Only expect validation if they expressed emotion */
        bool their_emotion = false;
        if (entries && count > 0) {
            for (size_t i = count > 3 ? count - 3 : 0; i < count; i++) {
                if (!entries[i].from_me) {
                    const char *t = entries[i].text;
                    size_t tl = strlen(t);
                    if (str_contains_ci(t, tl, "sad") || str_contains_ci(t, tl, "stressed") ||
                        str_contains_ci(t, tl, "excited") || str_contains_ci(t, tl, "worried") ||
                        str_contains_ci(t, tl, "happy") || str_contains_ci(t, tl, "frustrated") ||
                        str_contains_ci(t, tl, "scared") || str_contains_ci(t, tl, "nervous"))
                        their_emotion = true;
                }
            }
        }
        if (!their_emotion)
            score.validation = 20;
        else
            score.validation = has_validation ? 25 : 5;
    }

    /* Warmth (0-25): personal touch, not robotic */
    {
        int warmth = 10;
        if (str_contains_ci(response, response_len, "!"))
            warmth += 5;
        if (str_contains_ci(response, response_len, "love"))
            warmth += 5;
        if (str_contains_ci(response, response_len, "miss"))
            warmth += 3;
        /* Penalize robotic patterns */
        if (str_contains_ci(response, response_len, "certainly"))
            warmth -= 8;
        if (str_contains_ci(response, response_len, "I'd be happy to"))
            warmth -= 10;
        if (str_contains_ci(response, response_len, "let me know if"))
            warmth -= 8;
        if (str_contains_ci(response, response_len, "feel free"))
            warmth -= 8;
        if (str_contains_ci(response, response_len, "as an AI"))
            warmth -= 20;
        if (warmth < 0)
            warmth = 0;
        if (warmth > 25)
            warmth = 25;
        score.warmth = warmth;
    }

    /* Naturalness (0-25): no markdown, no service language, contractions */
    {
        int nat = 20;
        if (str_contains_ci(response, response_len, "**"))
            nat -= 10;
        if (str_contains_ci(response, response_len, "##"))
            nat -= 10;
        if (str_contains_ci(response, response_len, "```"))
            nat -= 10;
        if (str_contains_ci(response, response_len, "- "))
            nat -= 3;
        if (str_contains_ci(response, response_len, "1. "))
            nat -= 3;
        /* Bonus for contractions (human-like) */
        if (str_contains_ci(response, response_len, "don't") ||
            str_contains_ci(response, response_len, "can't") ||
            str_contains_ci(response, response_len, "I'm") ||
            str_contains_ci(response, response_len, "it's"))
            nat += 5;
        if (nat < 0)
            nat = 0;
        if (nat > 25)
            nat = 25;
        score.naturalness = nat;
    }

    score.total = score.brevity + score.validation + score.warmth + score.naturalness;
    score.needs_revision = score.total < 50 || score.warmth < 5 || score.naturalness < 5;
    return score;
}

/* ── Honesty Guardrail ──────────────────────────────────────────────── */

char *sc_conversation_honesty_check(sc_allocator_t *alloc, const char *message,
                                    size_t message_len) {
    if (!alloc || !message || message_len == 0)
        return NULL;

    /* Detect "did you do X?" patterns */
    const char *action_queries[] = {"did you call",    "did you text",     "did you email",
                                    "did you send",    "did you check",    "did you look",
                                    "did you buy",     "did you book",     "did you talk to",
                                    "did you find",    "did you ask",      "have you called",
                                    "have you texted", "have you checked", "have you looked",
                                    "have you sent",   "have you talked",  NULL};

    bool found = false;
    const char *matched = NULL;
    for (int i = 0; action_queries[i]; i++) {
        if (str_contains_ci(message, message_len, action_queries[i])) {
            found = true;
            matched = action_queries[i];
            break;
        }
    }

    if (!found)
        return NULL;

    char *buf = (char *)alloc->alloc(alloc->ctx, 512);
    if (!buf)
        return NULL;

    int n = snprintf(buf, 512,
                     "HONESTY GUARDRAIL: They asked '%s...'. "
                     "You have NO record of actually doing this action. "
                     "Do NOT imply you did it. Be honest: 'Not yet' or 'I haven't gotten to that' "
                     "or 'Let me do that now' are acceptable. NEVER fabricate completed actions.",
                     matched);
    if (n <= 0 || (size_t)n >= 512) {
        alloc->free(alloc->ctx, buf, 512);
        return NULL;
    }
    return buf;
}

/* ── Narrative Arc Detection ────────────────────────────────────────── */

sc_narrative_phase_t sc_conversation_detect_narrative(const sc_channel_history_entry_t *entries,
                                                      size_t count) {
    if (!entries || count == 0)
        return SC_NARRATIVE_OPENING;

    /* Count exchanges (direction changes) */
    size_t exchanges = 0;
    bool last_from_me = false;
    for (size_t i = 0; i < count; i++) {
        if (i == 0 || entries[i].from_me != last_from_me)
            exchanges++;
        last_from_me = entries[i].from_me;
    }

    /* Measure emotional intensity in recent messages */
    int emotional_words = 0;
    int question_marks __attribute__((unused)) = 0;
    int exclamation_marks = 0;
    size_t recent_start = count > 5 ? count - 5 : 0;
    for (size_t i = recent_start; i < count; i++) {
        if (entries[i].from_me)
            continue;
        const char *t = entries[i].text;
        size_t tl = strlen(t);
        for (size_t j = 0; j < tl; j++) {
            if (t[j] == '?')
                question_marks++;
            if (t[j] == '!')
                exclamation_marks++;
        }
        if (str_contains_ci(t, tl, "love") || str_contains_ci(t, tl, "hate") ||
            str_contains_ci(t, tl, "scared") || str_contains_ci(t, tl, "angry") ||
            str_contains_ci(t, tl, "can't believe") || str_contains_ci(t, tl, "so happy") ||
            str_contains_ci(t, tl, "so sad") || str_contains_ci(t, tl, "hurt") ||
            str_contains_ci(t, tl, "need to tell") || str_contains_ci(t, tl, "important"))
            emotional_words++;
    }

    /* Detect closing signals */
    if (count >= 2) {
        const char *last = entries[count - 1].text;
        size_t ll = strlen(last);
        if (!entries[count - 1].from_me &&
            (str_contains_ci(last, ll, "gotta go") || str_contains_ci(last, ll, "talk later") ||
             str_contains_ci(last, ll, "bye") || str_contains_ci(last, ll, "night") ||
             str_contains_ci(last, ll, "ttyl") || str_contains_ci(last, ll, "heading out") ||
             (ll < 10 && (str_contains_ci(last, ll, "ok") || str_contains_ci(last, ll, "k")))))
            return SC_NARRATIVE_CLOSING;
    }

    /* Map to phases */
    if (exchanges <= 2)
        return SC_NARRATIVE_OPENING;
    if (exchanges <= 5)
        return SC_NARRATIVE_BUILDING;
    if (emotional_words >= 2 || exclamation_marks >= 3)
        return SC_NARRATIVE_PEAK;
    if (exchanges > 5 && emotional_words >= 1)
        return SC_NARRATIVE_APPROACHING_CLIMAX;
    if (exchanges > 10)
        return SC_NARRATIVE_RELEASE;
    return SC_NARRATIVE_BUILDING;
}

/* ── Engagement Scoring ─────────────────────────────────────────────── */

sc_engagement_level_t sc_conversation_detect_engagement(const sc_channel_history_entry_t *entries,
                                                        size_t count) {
    if (!entries || count == 0)
        return SC_ENGAGEMENT_MODERATE;

    size_t their_msgs = 0;
    size_t total_their_len = 0;
    size_t very_short = 0;
    size_t questions_to_us = 0;
    size_t recent = count > 6 ? count - 6 : 0;

    for (size_t i = recent; i < count; i++) {
        if (entries[i].from_me)
            continue;
        their_msgs++;
        size_t tl = strlen(entries[i].text);
        total_their_len += tl;
        if (tl < 8)
            very_short++;
        for (size_t j = 0; j < tl; j++) {
            if (entries[i].text[j] == '?') {
                questions_to_us++;
                break;
            }
        }
    }

    if (their_msgs == 0)
        return SC_ENGAGEMENT_DISTRACTED;

    size_t avg_len = total_their_len / their_msgs;

    /* High: asking questions, longer messages, engaged */
    if (questions_to_us >= 2 || avg_len > 60)
        return SC_ENGAGEMENT_HIGH;

    /* Low: very short responses, no questions */
    if (avg_len < 15 && very_short >= 2 && questions_to_us == 0)
        return SC_ENGAGEMENT_LOW;

    /* Distracted: single-word or empty responses */
    if (their_msgs >= 2 && very_short == their_msgs)
        return SC_ENGAGEMENT_DISTRACTED;

    return SC_ENGAGEMENT_MODERATE;
}

/* ── Emotional State Detection ──────────────────────────────────────── */

sc_emotional_state_t sc_conversation_detect_emotion(const sc_channel_history_entry_t *entries,
                                                    size_t count) {
    sc_emotional_state_t state = {0.0f, 0.0f, false, "neutral"};
    if (!entries || count == 0)
        return state;

    float positive = 0, negative = 0;
    int samples = 0;
    size_t start = count > 8 ? count - 8 : 0;

    for (size_t i = start; i < count; i++) {
        if (entries[i].from_me)
            continue;
        const char *t = entries[i].text;
        size_t tl = strlen(t);
        samples++;

        /* Positive signals */
        if (str_contains_ci(t, tl, "happy"))
            positive += 1.0f;
        if (str_contains_ci(t, tl, "excited"))
            positive += 1.2f;
        if (str_contains_ci(t, tl, "love"))
            positive += 0.8f;
        if (str_contains_ci(t, tl, "amazing"))
            positive += 1.0f;
        if (str_contains_ci(t, tl, "great"))
            positive += 0.6f;
        if (str_contains_ci(t, tl, "awesome"))
            positive += 0.8f;
        if (str_contains_ci(t, tl, "lol") || str_contains_ci(t, tl, "haha"))
            positive += 0.4f;
        if (str_contains_ci(t, tl, "yay") || str_contains_ci(t, tl, "yes!"))
            positive += 0.7f;

        /* Negative signals */
        if (str_contains_ci(t, tl, "sad"))
            negative += 1.0f;
        if (str_contains_ci(t, tl, "depressed"))
            negative += 1.5f;
        if (str_contains_ci(t, tl, "stressed"))
            negative += 1.0f;
        if (str_contains_ci(t, tl, "worried"))
            negative += 0.8f;
        if (str_contains_ci(t, tl, "anxious"))
            negative += 1.0f;
        if (str_contains_ci(t, tl, "angry"))
            negative += 1.2f;
        if (str_contains_ci(t, tl, "frustrated"))
            negative += 1.0f;
        if (str_contains_ci(t, tl, "hurt"))
            negative += 1.2f;
        if (str_contains_ci(t, tl, "scared"))
            negative += 1.0f;
        if (str_contains_ci(t, tl, "lonely"))
            negative += 1.3f;
        if (str_contains_ci(t, tl, "crying") || str_contains_ci(t, tl, "cry"))
            negative += 1.5f;
        if (str_contains_ci(t, tl, "hate"))
            negative += 1.0f;
        if (str_contains_ci(t, tl, "ugh") || str_contains_ci(t, tl, "damn"))
            negative += 0.5f;
    }

    if (samples == 0)
        return state;

    state.valence = (positive - negative) / (float)samples;
    state.intensity = (positive + negative) / (float)samples;
    if (state.valence < -0.5f)
        state.valence = -1.0f;
    if (state.valence > 1.0f)
        state.valence = 1.0f;
    if (state.intensity > 2.0f)
        state.intensity = 2.0f;

    if (negative > positive * 1.5f) {
        state.concerning = negative >= 2.0f;
        if (str_contains_ci(entries[count - 1].text, strlen(entries[count - 1].text),
                            "depressed") ||
            str_contains_ci(entries[count - 1].text, strlen(entries[count - 1].text), "crying"))
            state.dominant_emotion = "distressed";
        else if (negative >= 1.5f)
            state.dominant_emotion = "upset";
        else
            state.dominant_emotion = "down";
    } else if (positive > negative * 1.5f) {
        if (positive >= 2.0f)
            state.dominant_emotion = "excited";
        else
            state.dominant_emotion = "positive";
    } else if (state.intensity > 0.3f) {
        state.dominant_emotion = "mixed";
    }

    return state;
}
