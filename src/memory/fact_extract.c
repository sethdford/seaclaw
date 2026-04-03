#include "human/memory/fact_extract.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ── Heuristic fact extraction patterns ──────────────────────────── */

typedef struct fact_pattern {
    const char *marker;
    hu_knowledge_type_t type;
} fact_pattern_t;

static const fact_pattern_t patterns[] = {
    /* Propositional — user preferences and facts */
    {"i like ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i love ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i hate ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i prefer ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"my favorite ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i work at ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i live in ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i am a ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i'm a ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i have a ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"my name is ", HU_KNOWLEDGE_PROPOSITIONAL},
    {"i enjoy ", HU_KNOWLEDGE_PROPOSITIONAL},
    /* Prescriptive — behavioral patterns and preferences */
    {"when i'm ", HU_KNOWLEDGE_PRESCRIPTIVE},
    {"i usually ", HU_KNOWLEDGE_PRESCRIPTIVE},
    {"i always ", HU_KNOWLEDGE_PRESCRIPTIVE},
    {"i never ", HU_KNOWLEDGE_PRESCRIPTIVE},
    {"please don't ", HU_KNOWLEDGE_PRESCRIPTIVE},
    {"i'd rather ", HU_KNOWLEDGE_PRESCRIPTIVE},
    {"i tend to ", HU_KNOWLEDGE_PRESCRIPTIVE},
};
static const size_t pattern_count = sizeof(patterns) / sizeof(patterns[0]);

static bool ci_match(const char *text, size_t text_len,
                     const char *pat, size_t pat_len) {
    if (text_len < pat_len)
        return false;
    for (size_t i = 0; i < pat_len; i++) {
        if (tolower((unsigned char)text[i]) != tolower((unsigned char)pat[i]))
            return false;
    }
    return true;
}

static size_t find_end(const char *text, size_t start, size_t len) {
    for (size_t i = start; i < len; i++) {
        if (text[i] == '.' || text[i] == '!' || text[i] == '?' ||
            text[i] == '\n' || text[i] == ',')
            return i;
    }
    return len;
}

static void extract_spo(const char *sentence, size_t sent_len,
                         const char *marker, size_t marker_len,
                         hu_heuristic_fact_t *fact) {
    /* Subject: "User" for first-person statements */
    strncpy(fact->subject, "user", sizeof(fact->subject) - 1);

    /* Predicate: the marker verb/phrase */
    size_t plen = marker_len;
    if (plen > sizeof(fact->predicate) - 1)
        plen = sizeof(fact->predicate) - 1;
    memcpy(fact->predicate, marker, plen);
    fact->predicate[plen] = '\0';
    /* Trim trailing space from predicate */
    while (plen > 0 && fact->predicate[plen - 1] == ' ')
        fact->predicate[--plen] = '\0';

    /* Object: rest of the sentence after marker */
    if (marker_len < sent_len) {
        size_t obj_start = marker_len;
        size_t obj_len = sent_len - obj_start;
        if (obj_len > sizeof(fact->object) - 1)
            obj_len = sizeof(fact->object) - 1;
        memcpy(fact->object, sentence + obj_start, obj_len);
        fact->object[obj_len] = '\0';
        /* Trim trailing whitespace/punctuation */
        while (obj_len > 0 && (fact->object[obj_len - 1] == ' ' ||
               fact->object[obj_len - 1] == '.' || fact->object[obj_len - 1] == ','))
            fact->object[--obj_len] = '\0';
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

hu_error_t hu_fact_extract(const char *text, size_t text_len,
                           hu_fact_extract_result_t *result) {
    if (!text || !result)
        return HU_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));

    for (size_t pos = 0; pos < text_len && result->fact_count < HU_FACT_EXTRACT_MAX;) {
        /* Skip to start of a sentence or line */
        while (pos < text_len && (text[pos] == ' ' || text[pos] == '\n'))
            pos++;

        bool found = false;
        for (size_t p = 0; p < pattern_count; p++) {
            size_t mlen = strlen(patterns[p].marker);
            if (ci_match(text + pos, text_len - pos, patterns[p].marker, mlen)) {
                size_t end = find_end(text, pos + mlen, text_len);
                size_t sent_len = end - pos;

                hu_heuristic_fact_t *f = &result->facts[result->fact_count];
                f->type = patterns[p].type;
                f->confidence = 0.7f;
                extract_spo(text + pos, sent_len, patterns[p].marker, mlen, f);
                snprintf(f->source_hint, sizeof(f->source_hint), "conversation");

                result->fact_count++;
                if (f->type == HU_KNOWLEDGE_PROPOSITIONAL)
                    result->propositional_count++;
                else
                    result->prescriptive_count++;

                pos = end;
                found = true;
                break;
            }
        }
        if (!found) {
            size_t next = find_end(text, pos, text_len);
            pos = (next > pos) ? next + 1 : pos + 1;
        }
    }
    return HU_OK;
}

size_t hu_fact_dedup(hu_fact_extract_result_t *result,
                     const hu_heuristic_fact_t *existing, size_t existing_count) {
    if (!result || !existing || existing_count == 0)
        return result ? result->fact_count : 0;

    size_t novel = 0;
    for (size_t i = 0; i < result->fact_count; i++) {
        bool dup = false;
        for (size_t j = 0; j < existing_count; j++) {
            if (strcmp(result->facts[i].subject, existing[j].subject) == 0 &&
                strcmp(result->facts[i].predicate, existing[j].predicate) == 0) {
                dup = true;
                break;
            }
        }
        if (!dup)
            novel++;
    }
    return novel;
}

hu_error_t hu_fact_format_for_store(hu_allocator_t *alloc,
                                    const hu_heuristic_fact_t *fact,
                                    char **key, size_t *key_len,
                                    char **value, size_t *value_len) {
    if (!alloc || !fact || !key || !key_len || !value || !value_len)
        return HU_ERR_INVALID_ARGUMENT;

    char kbuf[768];
    char vbuf[768];
    int kn, vn;

    if (fact->type == HU_KNOWLEDGE_PROPOSITIONAL) {
        kn = snprintf(kbuf, sizeof(kbuf), "fact:%s:%s:%s",
                      fact->subject, fact->predicate, fact->object);
        vn = snprintf(vbuf, sizeof(vbuf), "%s %s %s (confidence: %.2f)",
                      fact->subject, fact->predicate, fact->object, fact->confidence);
    } else {
        kn = snprintf(kbuf, sizeof(kbuf), "skill:%s:%s",
                      fact->subject, fact->predicate);
        vn = snprintf(vbuf, sizeof(vbuf), "%s %s %s (confidence: %.2f)",
                      fact->subject, fact->predicate, fact->object, fact->confidence);
    }

    if (kn <= 0 || vn <= 0 || (size_t)kn >= sizeof(kbuf) || (size_t)vn >= sizeof(vbuf))
        return HU_ERR_INVALID_ARGUMENT;

    *key = (char *)alloc->alloc(alloc->ctx, (size_t)kn + 1);
    if (!*key)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(*key, kbuf, (size_t)kn + 1);
    *key_len = (size_t)kn;

    *value = (char *)alloc->alloc(alloc->ctx, (size_t)vn + 1);
    if (!*value) {
        alloc->free(alloc->ctx, *key, (size_t)kn + 1);
        *key = NULL;
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(*value, vbuf, (size_t)vn + 1);
    *value_len = (size_t)vn;
    return HU_OK;
}
