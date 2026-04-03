/*
 * Transcript preprocessor for human-quality Cartesia TTS.
 *
 * Transforms plain text into SSML-annotated speech with:
 *   - Sentence segmentation (respecting abbreviations, ellipsis, decimals)
 *   - Per-sentence emotion tagging via hu_cartesia_emotion_from_context
 *   - SSML <break> injection at clause/sentence/emotion-shift boundaries
 *   - Speed variation (slower for emotional beats, faster for asides)
 *   - Discourse marker weaving ("you know,", "I mean,", "honestly,")
 *   - Context-aware nonverbal injection ([laughter], thoughtful pauses)
 *   - Emotion-derived volume
 */
#include "human/tts/transcript_prep.h"
#include "human/tts/emotion_map.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Forward declaration (defined below) */
static bool ci_has(const char *hay, size_t hlen, const char *needle);

/* ── Abbreviation table (don't split on these periods) ────────────────── */

static const char *const ABBREVIATIONS[] = {
    "Mr.", "Mrs.", "Ms.", "Dr.", "Prof.", "Sr.", "Jr.", "St.",
    "vs.", "etc.", "i.e.", "e.g.", "a.m.", "p.m.", "Inc.", "Ltd.",
    "Corp.", "Gen.", "Gov.", "Sgt.", "Capt.", "Lt.", "Col.",
};
#define ABBREV_COUNT (sizeof(ABBREVIATIONS) / sizeof(ABBREVIATIONS[0]))

static bool ends_with_abbreviation(const char *text, size_t pos) {
    for (size_t i = 0; i < ABBREV_COUNT; i++) {
        size_t alen = strlen(ABBREVIATIONS[i]);
        if (pos + 1 >= alen) {
            size_t start = pos + 1 - alen;
            if (memcmp(text + start, ABBREVIATIONS[i], alen) == 0)
                return true;
        }
    }
    return false;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

/* ── Sentence segmenter ──────────────────────────────────────────────── */

size_t hu_transcript_segment(const char *text, size_t text_len,
                             hu_prep_sentence_t *out, size_t max_sentences) {
    if (!text || text_len == 0 || !out || max_sentences == 0)
        return 0;

    size_t count = 0;
    size_t sent_start = 0;

    /* Skip leading whitespace */
    while (sent_start < text_len && (text[sent_start] == ' ' || text[sent_start] == '\n'))
        sent_start++;

    for (size_t i = sent_start; i < text_len && count < max_sentences; i++) {
        char c = text[i];
        bool is_end = false;

        if (c == '!' || c == '?') {
            is_end = true;
        } else if (c == '.') {
            /* Ellipsis: ... */
            if (i + 2 < text_len && text[i + 1] == '.' && text[i + 2] == '.')
                continue;
            /* Decimal: 3.14 */
            if (i > 0 && is_digit(text[i - 1]) && i + 1 < text_len && is_digit(text[i + 1]))
                continue;
            /* Abbreviation */
            if (ends_with_abbreviation(text, i))
                continue;
            is_end = true;
        }

        if (is_end) {
            /* Include trailing punctuation (e.g. "!" or "?!" or "...") */
            size_t end = i + 1;
            while (end < text_len && (text[end] == '!' || text[end] == '?' || text[end] == '.'))
                end++;

            size_t slen = end - sent_start;
            if (slen > 0) {
                /* Trim leading whitespace */
                while (slen > 0 && (text[sent_start] == ' ' || text[sent_start] == '\n')) {
                    sent_start++;
                    slen--;
                }
                if (slen > 0) {
                    out[count].text = text + sent_start;
                    out[count].len = slen;
                    out[count].emotion = NULL;
                    out[count].speed_ratio = 1.0f;
                    count++;
                }
            }
            sent_start = end;
            while (sent_start < text_len &&
                   (text[sent_start] == ' ' || text[sent_start] == '\n'))
                sent_start++;
            i = sent_start - 1;
        }
    }

    /* Trailing text without terminal punctuation */
    if (sent_start < text_len && count < max_sentences) {
        size_t slen = text_len - sent_start;
        while (slen > 0 && (text[sent_start] == ' ' || text[sent_start] == '\n')) {
            sent_start++;
            slen--;
        }
        while (slen > 0 &&
               (text[sent_start + slen - 1] == ' ' || text[sent_start + slen - 1] == '\n'))
            slen--;
        if (slen > 0) {
            out[count].text = text + sent_start;
            out[count].len = slen;
            out[count].emotion = NULL;
            out[count].speed_ratio = 1.0f;
            count++;
        }
    }

    return count;
}

/* ── Per-sentence emotion via context heuristics ─────────────────────── */

static const char *emotion_for_sentence(const char *sentence, size_t len,
                                        const char *incoming, size_t incoming_len,
                                        uint8_t hour) {
    return hu_cartesia_emotion_from_context(incoming, incoming_len, sentence, len, hour);
}

/* ── Speed variation (informed by voiceai rhythm analysis) ────────────── */

static float speed_for_sentence(const char *sentence, size_t len, float base_speed) {
    if (!sentence || len == 0)
        return base_speed;

    /* Parenthetical asides: slightly faster */
    if (sentence[0] == '(' || (len > 2 && sentence[0] == '-' && sentence[1] == '-'))
        return base_speed * 1.15f;

    bool has_excl = false;
    bool has_question = false;
    size_t comma_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (sentence[i] == '!')
            has_excl = true;
        if (sentence[i] == '?')
            has_question = true;
        if (sentence[i] == ',')
            comma_count++;
    }

    /* Emotional keywords: slower for weight */
    if (ci_has(sentence, len, "feel") || ci_has(sentence, len, "love") ||
        ci_has(sentence, len, "care") || ci_has(sentence, len, "heart") ||
        ci_has(sentence, len, "worry"))
        return base_speed * 0.90f;

    /* Important/emphasis words: slower, deliberate */
    if (ci_has(sentence, len, "important") || ci_has(sentence, len, "crucial") ||
        ci_has(sentence, len, "remember"))
        return base_speed * 0.92f;

    /* Conclusions: slower, more weight */
    if (ci_has(sentence, len, "so ") || ci_has(sentence, len, "therefore") ||
        ci_has(sentence, len, "the point is"))
        return base_speed * 0.93f;

    /* Questions: slightly slower, thoughtful */
    if (has_question)
        return base_speed * 0.95f;

    /* Emphatic exclamatory: slower for gravity */
    if (has_excl)
        return base_speed * 0.92f;

    /* Lists/examples: slightly faster */
    if (ci_has(sentence, len, "for example") || ci_has(sentence, len, "such as") ||
        ci_has(sentence, len, "first") || ci_has(sentence, len, "second"))
        return base_speed * 1.05f;

    /* Long compound sentences: slightly faster to stay natural */
    if (comma_count >= 3 && len > 80)
        return base_speed * 1.05f;

    return base_speed;
}

/* ── Volume from emotion ─────────────────────────────────────────────── */

float hu_emotion_to_volume(const char *emotion) {
    if (!emotion)
        return 1.0f;
    if (strcmp(emotion, "calm") == 0 || strcmp(emotion, "peaceful") == 0 ||
        strcmp(emotion, "serene") == 0)
        return 0.88f;
    if (strcmp(emotion, "sympathetic") == 0 || strcmp(emotion, "empathy") == 0 ||
        strcmp(emotion, "contemplative") == 0)
        return 0.90f;
    if (strcmp(emotion, "sad") == 0 || strcmp(emotion, "dejected") == 0 ||
        strcmp(emotion, "melancholic") == 0 || strcmp(emotion, "apologetic") == 0)
        return 0.85f;
    if (strcmp(emotion, "excited") == 0 || strcmp(emotion, "triumphant") == 0 ||
        strcmp(emotion, "enthusiastic") == 0 || strcmp(emotion, "elated") == 0)
        return 1.15f;
    if (strcmp(emotion, "angry") == 0 || strcmp(emotion, "outraged") == 0 ||
        strcmp(emotion, "frustrated") == 0)
        return 1.20f;
    if (strcmp(emotion, "anxious") == 0 || strcmp(emotion, "panicked") == 0 ||
        strcmp(emotion, "alarmed") == 0 || strcmp(emotion, "scared") == 0)
        return 1.10f;
    return 1.0f;
}

/* ── Break duration (ms) between sentences ───────────────────────────── */

static int break_duration_ms(const char *prev_emo, const char *next_emo, float pause_factor) {
    int base_ms = 350;

    /* Emotional transition: longer pause to let the shift land */
    if (prev_emo && next_emo && strcmp(prev_emo, next_emo) != 0)
        base_ms = 650;

    return (int)(base_ms * pause_factor);
}

/* ── Breath-group clause pauses (inspired by voiceai) ────────────────── */

static size_t inject_clause_breaks(const char *text, size_t len, float pause_factor,
                                   bool strip_ssml, char *out, size_t cap) {
    size_t pos = 0;
    for (size_t i = 0; i < len && pos < cap - 1; i++) {
        out[pos++] = text[i];

        if (text[i] == ',' || text[i] == ';' || text[i] == ':') {
            if (i + 1 < len && text[i + 1] == ' ') {
                int ms = (text[i] == ',') ? (int)(150 * pause_factor) : (int)(200 * pause_factor);
                /* Before conjunctions: slightly longer pause */
                if (i + 2 < len) {
                    const char *after = text + i + 2;
                    size_t remain = len - (i + 2);
                    if ((remain >= 4 && (memcmp(after, "but ", 4) == 0 ||
                                         memcmp(after, "yet ", 4) == 0)) ||
                        (remain >= 8 && memcmp(after, "however ", 8) == 0) ||
                        (remain >= 9 && memcmp(after, "although ", 9) == 0))
                        ms = (int)(250 * pause_factor);
                }
                if (strip_ssml) {
                    /* SSML-free mode: use punctuation spacing (already have comma) */
                } else if (pos + 30 < cap) {
                    int n = snprintf(out + pos, cap - pos, " <break time=\"%dms\"/>", ms);
                    if (n > 0 && pos + (size_t)n < cap)
                        pos += (size_t)n;
                    i++;
                    continue;
                }
            }
        }

        /* Em-dash: short pause before */
        if (text[i] == '\xe2' && i + 2 < len && (unsigned char)text[i + 1] == 0x80 &&
            (unsigned char)text[i + 2] == 0x94) {
            if (!strip_ssml && pos + 30 < cap) {
                int n = snprintf(out + pos, cap - pos, "<break time=\"%dms\"/>",
                                 (int)(150 * pause_factor));
                if (n > 0 && pos + (size_t)n < cap)
                    pos += (size_t)n;
            }
        }
    }
    out[pos] = '\0';
    return pos;
}

/* ── Discourse marker injection ──────────────────────────────────────── */

typedef struct {
    const char *marker;
    const char *const *triggers;
    size_t trigger_count;
} discourse_rule_t;

static const char *const SINCERE_TRIGGERS[] = {"proud", "love", "care", "believe", "mean"};
static const char *const RELATABLE_TRIGGERS[] = {"right", "same", "feel", "get it", "totally"};
static const char *const CLARIFY_TRIGGERS[] = {"actually", "really", "not just", "more like"};

static const discourse_rule_t DISCOURSE_RULES[] = {
    {"honestly, ", SINCERE_TRIGGERS, sizeof(SINCERE_TRIGGERS) / sizeof(SINCERE_TRIGGERS[0])},
    {"you know, ", RELATABLE_TRIGGERS, sizeof(RELATABLE_TRIGGERS) / sizeof(RELATABLE_TRIGGERS[0])},
    {"I mean, ", CLARIFY_TRIGGERS, sizeof(CLARIFY_TRIGGERS) / sizeof(CLARIFY_TRIGGERS[0])},
};
#define DISCOURSE_RULE_COUNT (sizeof(DISCOURSE_RULES) / sizeof(DISCOURSE_RULES[0]))

static bool ci_has(const char *hay, size_t hlen, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen > hlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        bool ok = true;
        for (size_t j = 0; j < nlen; j++) {
            char a = hay[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z')
                a += 32;
            if (b >= 'A' && b <= 'Z')
                b += 32;
            if (a != b) {
                ok = false;
                break;
            }
        }
        if (ok)
            return true;
    }
    return false;
}

static const char *pick_discourse_marker(const char *sentence, size_t len, uint32_t seed) {
    for (size_t r = 0; r < DISCOURSE_RULE_COUNT; r++) {
        for (size_t t = 0; t < DISCOURSE_RULES[r].trigger_count; t++) {
            if (ci_has(sentence, len, DISCOURSE_RULES[r].triggers[t])) {
                /* Don't always inject — ~30% chance when trigger matches */
                if ((seed ^ (uint32_t)(r * 31 + t * 17)) % 100 < 30)
                    return DISCOURSE_RULES[r].marker;
            }
        }
    }
    return NULL;
}

/* ── Enhanced nonverbal injection ─────────────────────────────────────── */

static const char *pick_nonverbal(const char *sentence, size_t len, const char *emotion,
                                  uint32_t seed) {
    if (!sentence || len == 0)
        return NULL;

    /* Higher probability for emotional content (25%), lower for neutral (10%) */
    int threshold = 10;
    if (emotion) {
        if (strcmp(emotion, "sympathetic") == 0 || strcmp(emotion, "sad") == 0 ||
            strcmp(emotion, "contemplative") == 0)
            threshold = 20;
        else if (strcmp(emotion, "excited") == 0 || strcmp(emotion, "joking/comedic") == 0)
            threshold = 25;
    }

    if ((seed % 100) >= (uint32_t)threshold)
        return NULL;

    /* Context-appropriate nonverbal */
    if (ci_has(sentence, len, "lol") || ci_has(sentence, len, "haha") ||
        ci_has(sentence, len, "funny"))
        return "[laughter] ";

    if (emotion && (strcmp(emotion, "contemplative") == 0 || strcmp(emotion, "calm") == 0))
        return "<break time=\"500ms\"/>";

    if (emotion && strcmp(emotion, "sympathetic") == 0)
        return "<break time=\"400ms\"/>";

    uint32_t pick = seed % 3;
    if (pick == 0)
        return "[laughter] ";
    if (pick == 1)
        return "Hmm... ";
    return "<break time=\"300ms\"/>";
}

/* ── Main preprocessor ───────────────────────────────────────────────── */

hu_error_t hu_transcript_prep(const char *transcript, size_t transcript_len,
                              const hu_prep_config_t *config, hu_prep_result_t *result) {
    if (!transcript || transcript_len == 0 || !config || !result)
        return HU_ERR_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));

    /* Segment into sentences */
    result->sentence_count = hu_transcript_segment(
        transcript, transcript_len, result->sentences, HU_PREP_MAX_SENTENCES);

    if (result->sentence_count == 0) {
        /* No sentence boundaries found — pass through as-is */
        size_t cp = transcript_len < HU_PREP_MAX_OUTPUT - 1 ? transcript_len
                                                            : HU_PREP_MAX_OUTPUT - 1;
        memcpy(result->output, transcript, cp);
        result->output[cp] = '\0';
        result->output_len = cp;
        result->dominant_emotion = config->default_emotion ? config->default_emotion : "content";
        result->volume = hu_emotion_to_volume(result->dominant_emotion);
        return HU_OK;
    }

    float base_speed = config->base_speed > 0.0f ? config->base_speed : 0.95f;
    float pause_factor = config->pause_factor > 0.0f ? config->pause_factor : 1.0f;

    /* Tag each sentence with emotion and speed */
    for (size_t i = 0; i < result->sentence_count; i++) {
        hu_prep_sentence_t *s = &result->sentences[i];
        s->emotion = emotion_for_sentence(s->text, s->len, config->incoming_msg,
                                          config->incoming_msg_len, config->hour_local);
        s->speed_ratio = speed_for_sentence(s->text, s->len, base_speed);
    }

    /* Dominant emotion = first sentence's emotion (sets the tone) */
    result->dominant_emotion = result->sentences[0].emotion;
    result->volume = hu_emotion_to_volume(result->dominant_emotion);

    bool strip = config->strip_ssml;

    /* Build output (SSML-annotated or punctuation-stripped depending on config) */
    char *out = result->output;
    size_t cap = HU_PREP_MAX_OUTPUT - 1;
    size_t pos = 0;

    for (size_t i = 0; i < result->sentence_count; i++) {
        hu_prep_sentence_t *s = &result->sentences[i];

        /* Insert break between sentences */
        if (i > 0) {
            const char *prev_emo = result->sentences[i - 1].emotion;
            int brk_ms = break_duration_ms(prev_emo, s->emotion, pause_factor);
            if (strip) {
                /* voiceai approach: convert breaks to punctuation */
                if (brk_ms >= 500 && pos + 2 < cap) {
                    out[pos++] = '.';
                    out[pos++] = ' ';
                } else if (brk_ms >= 200 && pos + 2 < cap) {
                    out[pos++] = ',';
                    out[pos++] = ' ';
                } else if (pos + 1 < cap) {
                    out[pos++] = ' ';
                }
            } else {
                int n = snprintf(out + pos, cap - pos, "<break time=\"%dms\"/>", brk_ms);
                if (n > 0 && pos + (size_t)n < cap)
                    pos += (size_t)n;
            }
        }

        /* Emotion tag if different from previous (SSML mode only) */
        if (!strip && s->emotion &&
            (i == 0 || strcmp(s->emotion, result->sentences[i - 1].emotion) != 0)) {
            int n = snprintf(out + pos, cap - pos, "<emotion value=\"%s\"/>", s->emotion);
            if (n > 0 && pos + (size_t)n < cap)
                pos += (size_t)n;
        }

        /* Speed tag if non-default (SSML mode only) */
        float speed_delta = s->speed_ratio - base_speed;
        if (!strip && (speed_delta > 0.03f || speed_delta < -0.03f)) {
            int n = snprintf(out + pos, cap - pos, "<speed ratio=\"%.2f\"/>",
                             (double)s->speed_ratio);
            if (n > 0 && pos + (size_t)n < cap)
                pos += (size_t)n;
        }

        /* Per-sentence volume (SSML mode only) */
        if (!strip) {
            float sv = hu_emotion_to_volume(s->emotion);
            float vol_delta = sv - result->volume;
            if (vol_delta > 0.05f || vol_delta < -0.05f) {
                int n = snprintf(out + pos, cap - pos, "<volume ratio=\"%.2f\"/>", (double)sv);
                if (n > 0 && pos + (size_t)n < cap)
                    pos += (size_t)n;
            }
        }

        /* Nonverbal before sentence (context-dependent) */
        if (config->nonverbals_enabled) {
            const char *nv = pick_nonverbal(s->text, s->len, s->emotion,
                                            config->seed ^ (uint32_t)(i * 97));
            if (nv) {
                if (strip) {
                    /* In strip mode, only emit text nonverbals, not SSML breaks */
                    if (nv[0] != '<') {
                        size_t nvlen = strlen(nv);
                        if (pos + nvlen < cap) {
                            memcpy(out + pos, nv, nvlen);
                            pos += nvlen;
                        }
                    }
                } else {
                    size_t nvlen = strlen(nv);
                    if (pos + nvlen < cap) {
                        memcpy(out + pos, nv, nvlen);
                        pos += nvlen;
                    }
                }
            }
        }

        /* Discourse marker (sparse, contextual — opt-in via discourse_rate) */
        if (config->discourse_rate > 0.0f && i > 0) {
            const char *marker = pick_discourse_marker(
                s->text, s->len, config->seed ^ (uint32_t)(i * 53));
            if (marker) {
                size_t mlen = strlen(marker);
                if (pos + mlen < cap) {
                    memcpy(out + pos, marker, mlen);
                    pos += mlen;
                    size_t text_start = pos;
                    if (s->len > 0 && pos + s->len < cap) {
                        memcpy(out + pos, s->text, s->len);
                        if (out[text_start] >= 'A' && out[text_start] <= 'Z')
                            out[text_start] += 32;
                        pos += s->len;
                        if (i + 1 < result->sentence_count && pos < cap)
                            out[pos++] = ' ';
                        continue;
                    }
                }
            }
        }

        /* Sentence text with intra-clause breath-group pauses */
        if (pos + s->len + 256 < cap) {
            char clause_buf[4096];
            size_t clen = inject_clause_breaks(s->text, s->len, pause_factor, strip,
                                               clause_buf, sizeof(clause_buf));
            if (clen > 0 && pos + clen < cap) {
                memcpy(out + pos, clause_buf, clen);
                pos += clen;
            }
        } else if (pos + s->len < cap) {
            memcpy(out + pos, s->text, s->len);
            pos += s->len;
        } else {
            size_t remain = cap - pos;
            if (remain > 0) {
                memcpy(out + pos, s->text, remain);
                pos += remain;
            }
            break;
        }

        if (i + 1 < result->sentence_count && pos < cap)
            out[pos++] = ' ';
    }

    out[pos] = '\0';
    result->output_len = pos;
    return HU_OK;
}
