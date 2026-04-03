#include "human/memory/consolidation.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include "human/memory/connections.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HU_CONS_MAX_TOKENS 64

static void tokenize(const char *s, size_t len, const char **tokens, size_t *count) {
    *count = 0;
    if (!s || len == 0)
        return;
    size_t i = 0;
    while (i < len && *count < HU_CONS_MAX_TOKENS) {
        while (i < len && (unsigned char)s[i] <= ' ')
            i++;
        if (i >= len)
            break;
        tokens[(*count)++] = s + i;
        while (i < len && (unsigned char)s[i] > ' ')
            i++;
    }
}

static size_t token_len(const char *token, const char *end) {
    size_t n = 0;
    while (token + n < end && (unsigned char)token[n] > ' ')
        n++;
    return n;
}

uint32_t hu_similarity_score(const char *a, size_t a_len, const char *b, size_t b_len) {
    if (!a || !b)
        return 0;
    if (a_len == 0 && b_len == 0)
        return 100;

    const char *a_tokens[HU_CONS_MAX_TOKENS];
    const char *b_tokens[HU_CONS_MAX_TOKENS];
    size_t a_count = 0, b_count = 0;
    tokenize(a, a_len, a_tokens, &a_count);
    tokenize(b, b_len, b_tokens, &b_count);

    if (a_count == 0 && b_count == 0)
        return 100;
    if (a_count == 0 || b_count == 0)
        return 0;

    size_t shared = 0;
    const char *a_end = a + a_len;
    const char *b_end = b + b_len;

    for (size_t i = 0; i < a_count; i++) {
        size_t ai_len = token_len(a_tokens[i], a_end);
        for (size_t j = 0; j < b_count; j++) {
            size_t bj_len = token_len(b_tokens[j], b_end);
            if (ai_len == bj_len && memcmp(a_tokens[i], b_tokens[j], ai_len) == 0) {
                shared++;
                break;
            }
        }
    }

    size_t total_unique = a_count + b_count - shared;
    if (total_unique == 0)
        return 100;
    return (uint32_t)((shared * 100) / total_unique);
}

static time_t parse_iso_timestamp(const char *ts, size_t ts_len) {
    if (!ts || ts_len == 0)
        return 0;
    struct tm tm_buf;
    memset(&tm_buf, 0, sizeof(tm_buf));
    if (ts_len >= 19 && ts[4] == '-' && ts[7] == '-' && ts[10] == 'T') {
        tm_buf.tm_year = (int)strtol(ts, NULL, 10) - 1900;
        tm_buf.tm_mon = (int)strtol(ts + 5, NULL, 10) - 1;
        tm_buf.tm_mday = (int)strtol(ts + 8, NULL, 10);
        tm_buf.tm_hour = (int)strtol(ts + 11, NULL, 10);
        tm_buf.tm_min = (int)strtol(ts + 14, NULL, 10);
        tm_buf.tm_sec = (int)strtol(ts + 17, NULL, 10);
        return mktime(&tm_buf);
    }
    long raw = strtol(ts, NULL, 10);
    return raw > 0 ? (time_t)raw : 0;
}

static int compare_timestamp(const char *ts_a, size_t ts_a_len, const char *ts_b, size_t ts_b_len) {
    if (!ts_a || ts_a_len == 0)
        return 1;
    if (!ts_b || ts_b_len == 0)
        return -1;
    /* ISO 8601 timestamps (YYYY-MM-DDThh:mm:ssZ) sort lexicographically */
    size_t cmp_len = ts_a_len < ts_b_len ? ts_a_len : ts_b_len;
    int cmp = memcmp(ts_a, ts_b, cmp_len);
    if (cmp != 0)
        return cmp < 0 ? -1 : 1;
    if (ts_a_len != ts_b_len)
        return ts_a_len < ts_b_len ? -1 : 1;
    return 0;
}

/* Extract the contact prefix from a key. Contact keys are "contact:<id>:..."
 * Returns the length through the second colon (inclusive), or 0 for non-contact keys. */
static size_t contact_prefix_len(const char *key, size_t key_len) {
    static const char pfx[] = "contact:";
    static const size_t pfx_len = 8;
    if (!key || key_len <= pfx_len || memcmp(key, pfx, pfx_len) != 0)
        return 0;
    for (size_t i = pfx_len; i < key_len; i++) {
        if (key[i] == ':')
            return i + 1;
    }
    return 0;
}

static bool same_contact(const hu_memory_entry_t *a, const hu_memory_entry_t *b) {
    size_t pa = contact_prefix_len(a->key, a->key_len);
    size_t pb = contact_prefix_len(b->key, b->key_len);
    if (pa == 0 && pb == 0) return true; /* both non-contact */
    if (pa != pb) return false;
    return memcmp(a->key, b->key, pa) == 0;
}

hu_error_t hu_memory_consolidate(hu_allocator_t *alloc, hu_memory_t *memory,
                                 const hu_consolidation_config_t *config) {
    if (!alloc || !memory || !memory->vtable || !config)
        return HU_ERR_INVALID_ARGUMENT;
    if (!memory->vtable->list || !memory->vtable->forget)
        return HU_ERR_NOT_SUPPORTED;

    hu_memory_entry_t *entries = NULL;
    size_t count = 0;
    hu_error_t err = memory->vtable->list(memory->ctx, alloc, NULL, NULL, 0, &entries, &count);
    if (err != HU_OK)
        return err;
    if (!entries || count == 0)
        return HU_OK;

    bool *to_forget = (bool *)alloc->alloc(alloc->ctx, count * sizeof(bool));
    if (!to_forget) {
        for (size_t i = 0; i < count; i++)
            hu_memory_entry_free_fields(alloc, &entries[i]);
        alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
        return HU_ERR_OUT_OF_MEMORY;
    }
    memset(to_forget, 0, count * sizeof(bool));

    /* MEM-003: Only compare entries within the same contact scope */
    for (size_t i = 0; i < count; i++) {
        if (to_forget[i])
            continue;
        for (size_t j = i + 1; j < count; j++) {
            if (to_forget[j])
                continue;
            if (!same_contact(&entries[i], &entries[j]))
                continue;
            uint32_t sim = hu_similarity_score(entries[i].content, entries[i].content_len,
                                               entries[j].content, entries[j].content_len);
            if (sim >= config->dedup_threshold) {
                int cmp = compare_timestamp(entries[i].timestamp, entries[i].timestamp_len,
                                            entries[j].timestamp, entries[j].timestamp_len);
                if (cmp <= 0)
                    to_forget[i] = true;
                else
                    to_forget[j] = true;
            }
        }
    }

    /* Time-based decay: mark old entries for removal */
    if (config->decay_days > 0) {
        time_t now = time(NULL);
        time_t threshold = now - (time_t)(config->decay_days * 86400);
        for (size_t i = 0; i < count; i++) {
            if (to_forget[i])
                continue;
            if (entries[i].timestamp && entries[i].timestamp_len > 0) {
                time_t ts = parse_iso_timestamp(entries[i].timestamp, entries[i].timestamp_len);
                if (ts > 0 && ts < threshold) {
                    to_forget[i] = true;
                }
            }
        }
    }

#ifndef HU_IS_TEST
    if (config->provider && config->provider->vtable &&
        config->provider->vtable->chat_with_system) {
        size_t surviving = 0;
        for (size_t i = 0; i < count; i++)
            if (!to_forget[i])
                surviving++;

        if (surviving >= 2) {
            hu_memory_entry_t *surv = (hu_memory_entry_t *)alloc->alloc(
                alloc->ctx, surviving * sizeof(hu_memory_entry_t));
            if (surv) {
                size_t si = 0;
                for (size_t i = 0; i < count; i++)
                    if (!to_forget[i])
                        surv[si++] = entries[i];

                char *prompt = NULL;
                size_t prompt_len = 0;
                if (hu_connections_build_prompt(alloc, surv, surviving, &prompt, &prompt_len) ==
                        HU_OK &&
                    prompt) {
                    char *response = NULL;
                    size_t response_len = 0;
                    const char *sys = "Return JSON only.";
                    hu_error_t chat_err = config->provider->vtable->chat_with_system(
                        config->provider->ctx, alloc, sys, 17, prompt, prompt_len, config->model,
                        config->model_len, 0.2, &response, &response_len);
                    alloc->free(alloc->ctx, prompt, HU_CONN_PROMPT_CAP);

                    if (chat_err == HU_OK && response) {
                        hu_connection_result_t conn_result = {0};
                        if (hu_connections_parse(alloc, response, response_len, surviving,
                                                 &conn_result) == HU_OK) {
                            (void)hu_connections_store_insights(alloc, memory, &conn_result, surv,
                                                                surviving);
                            hu_connection_result_deinit(&conn_result, alloc);
                        }
                        alloc->free(alloc->ctx, response, response_len + 1);
                    }
                }
                alloc->free(alloc->ctx, surv, surviving * sizeof(hu_memory_entry_t));
            }
        }
    }
#endif

    for (size_t i = 0; i < count; i++) {
        if (to_forget[i] && entries[i].key) {
            bool deleted = false;
            hu_error_t ferr =
                memory->vtable->forget(memory->ctx, entries[i].key, entries[i].key_len, &deleted);
            if (ferr != HU_OK)
                fprintf(stderr, "[consolidation] forget key '%.*s' failed: %s\n",
                        (int)entries[i].key_len, entries[i].key, hu_error_string(ferr));
        }
    }

    alloc->free(alloc->ctx, to_forget, count * sizeof(bool));

    for (size_t i = 0; i < count; i++)
        hu_memory_entry_free_fields(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));

    size_t new_count = 0;
    err = memory->vtable->count(memory->ctx, &new_count);
    if (err != HU_OK)
        return HU_OK;

    if (new_count <= config->max_entries)
        return HU_OK;

    err = memory->vtable->list(memory->ctx, alloc, NULL, NULL, 0, &entries, &count);
    if (err != HU_OK || !entries || count <= config->max_entries)
        return HU_OK;

    /* Sort by timestamp (oldest first) for eviction */
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            int cmp = compare_timestamp(entries[i].timestamp, entries[i].timestamp_len,
                                        entries[j].timestamp, entries[j].timestamp_len);
            if (cmp > 0) {
                hu_memory_entry_t tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }

    /* Count entries per contact to ensure eviction doesn't wipe a contact.
     * Track evicted indices to correctly count remaining per-contact entries. */
    size_t to_remove = count - config->max_entries;
    bool *evicted = (bool *)alloc->alloc(alloc->ctx, count * sizeof(bool));
    if (!evicted) {
        for (size_t i = 0; i < count; i++)
            hu_memory_entry_free_fields(alloc, &entries[i]);
        alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
        return HU_ERR_OUT_OF_MEMORY;
    }
    memset(evicted, 0, count * sizeof(bool));

    size_t removed = 0;
    for (size_t i = 0; i < count && removed < to_remove; i++) {
        if (!entries[i].key || evicted[i])
            continue;
        /* Count how many non-evicted entries share this contact prefix */
        size_t contact_remaining = 0;
        for (size_t j = 0; j < count; j++) {
            if (j == i || evicted[j] || !entries[j].key)
                continue;
            if (same_contact(&entries[i], &entries[j]))
                contact_remaining++;
        }
        /* Never evict the last entry for a contact */
        if (contact_remaining == 0)
            continue;
        bool deleted = false;
        hu_error_t ferr =
            memory->vtable->forget(memory->ctx, entries[i].key, entries[i].key_len, &deleted);
        if (ferr != HU_OK)
            fprintf(stderr, "[consolidation] evict key '%.*s' failed: %s\n",
                    (int)entries[i].key_len, entries[i].key, hu_error_string(ferr));
        else {
            evicted[i] = true;
            removed++;
        }
    }
    alloc->free(alloc->ctx, evicted, count * sizeof(bool));

    for (size_t i = 0; i < count; i++)
        hu_memory_entry_free_fields(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));

    return HU_OK;
}
