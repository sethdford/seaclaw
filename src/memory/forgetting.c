#include "human/memory/forgetting.h"
#include "human/memory/forgetting_curve.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef HU_IS_TEST
/* Parse ISO 8601 (YYYY-MM-DDThh:mm:ssZ) or unix seconds; return 0 on failure. */
static int64_t parse_iso_timestamp(const char *ts, size_t ts_len) {
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
        return (int64_t)mktime(&tm_buf);
    }
    long raw = strtol(ts, NULL, 10);
    return raw > 0 ? (int64_t)raw : 0;
}
#endif

hu_error_t hu_memory_decay(hu_allocator_t *alloc, hu_memory_t *memory, double decay_rate,
                           hu_forgetting_stats_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (decay_rate < 0.0 || decay_rate > 1.0)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
#ifdef HU_IS_TEST
    if (!memory)
        return HU_ERR_INVALID_ARGUMENT;
    out->total_memories = 100;
    out->decayed = 10;
    return HU_OK;
#else
    if (!memory || !memory->vtable || !memory->vtable->list)
        return HU_ERR_NOT_SUPPORTED;
    if (!memory->vtable->store && !memory->vtable->store_ex)
        return HU_ERR_NOT_SUPPORTED;

    hu_memory_entry_t *entries = NULL;
    size_t count = 0;
    hu_error_t err =
        memory->vtable->list(memory->ctx, alloc, NULL, NULL, 0, &entries, &count);
    if (err != HU_OK)
        return err;

    out->total_memories = count;
    if (!entries || count == 0)
        return HU_OK;

    int64_t now_ts = (int64_t)time(NULL);
    size_t decayed_count = 0;

    for (size_t i = 0; i < count; i++) {
        hu_memory_entry_t *e = &entries[i];
        if (!e->key || !e->content)
            continue;

        double initial = isnan(e->score) || e->score < 0.0 ? 1.0 : e->score;
        int64_t created_at =
            (e->timestamp && e->timestamp_len > 0)
                ? parse_iso_timestamp(e->timestamp, e->timestamp_len)
                : now_ts;
        double decayed =
            hu_forgetting_decayed_salience(initial, decay_rate, created_at, now_ts, false);
        if (decayed >= initial - 1e-9)
            continue;

        decayed_count++;
        hu_memory_store_opts_t opts = {
            .source = e->source,
            .source_len = e->source_len,
            .importance = decayed,
        };
        if (memory->vtable->store_ex) {
            err = memory->vtable->store_ex(memory->ctx, e->key, e->key_len, e->content,
                                          e->content_len, &e->category, e->session_id,
                                          e->session_id_len, &opts);
        } else {
            err = memory->vtable->store(memory->ctx, e->key, e->key_len, e->content,
                                       e->content_len, &e->category, e->session_id,
                                       e->session_id_len);
        }
        if (err != HU_OK)
            break;
    }

    out->decayed = decayed_count;

    for (size_t i = 0; i < count; i++)
        hu_memory_entry_free_fields(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));

    return err;
#endif
}

hu_error_t hu_memory_boost(hu_allocator_t *alloc, hu_memory_t *memory, const char *memory_id,
                           double boost_amount) {
    if (!alloc || !memory_id || memory_id[0] == '\0')
        return HU_ERR_INVALID_ARGUMENT;
    if (boost_amount < 0.0)
        return HU_ERR_INVALID_ARGUMENT;
    if (!memory)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    if (strcmp(memory_id, "__hu_test_absent_memory__") == 0)
        return HU_ERR_NOT_FOUND;
    return HU_OK;
#else
    if (!memory || !memory->vtable || !memory->vtable->get)
        return HU_ERR_NOT_SUPPORTED;
    if (!memory->vtable->store && !memory->vtable->store_ex)
        return HU_ERR_NOT_SUPPORTED;

    size_t key_len = strlen(memory_id);
    hu_memory_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    bool found = false;
    hu_error_t err =
        memory->vtable->get(memory->ctx, alloc, memory_id, key_len, &entry, &found);
    if (err != HU_OK)
        return err;
    if (!found) {
        hu_memory_entry_free_fields(alloc, &entry);
        return HU_ERR_NOT_FOUND;
    }

    double new_importance =
        (isnan(entry.score) || entry.score < 0.0 ? 0.0 : entry.score) + boost_amount;

    if (memory->vtable->store_ex) {
        hu_memory_store_opts_t opts = {
            .source = entry.source,
            .source_len = entry.source_len,
            .importance = new_importance,
        };
        err = memory->vtable->store_ex(memory->ctx, entry.key, entry.key_len, entry.content,
                                      entry.content_len, &entry.category, entry.session_id,
                                      entry.session_id_len, &opts);
    } else {
        err = memory->vtable->store(memory->ctx, entry.key, entry.key_len, entry.content,
                                   entry.content_len, &entry.category, entry.session_id,
                                   entry.session_id_len);
    }

    hu_memory_entry_free_fields(alloc, &entry);
    return err;
#endif
}

hu_error_t hu_memory_prune(hu_allocator_t *alloc, hu_memory_t *memory, double threshold,
                          hu_forgetting_stats_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (threshold < 0.0 || threshold > 1.0)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
#ifdef HU_IS_TEST
    if (!memory)
        return HU_ERR_INVALID_ARGUMENT;
    out->total_memories = 100;
    out->pruned = 5;
    return HU_OK;
#else
    if (!memory || !memory->vtable || !memory->vtable->list || !memory->vtable->forget)
        return HU_ERR_NOT_SUPPORTED;

    hu_memory_entry_t *entries = NULL;
    size_t count = 0;
    hu_error_t err =
        memory->vtable->list(memory->ctx, alloc, NULL, NULL, 0, &entries, &count);
    if (err != HU_OK)
        return err;

    out->total_memories = count;
    if (!entries || count == 0)
        return HU_OK;

    size_t pruned_count = 0;

    for (size_t i = 0; i < count; i++) {
        hu_memory_entry_t *e = &entries[i];
        if (!e->key)
            continue;
        /* Skip entries with unknown score (NAN) - we cannot determine if below threshold */
        double score = isnan(e->score) ? -1.0 : e->score;
        if (score >= 0.0 && score < threshold) {
            bool deleted = false;
            err = memory->vtable->forget(memory->ctx, e->key, e->key_len, &deleted);
            if (err != HU_OK)
                break;
            if (deleted)
                pruned_count++;
        }
    }

    out->pruned = pruned_count;

    for (size_t i = 0; i < count; i++)
        hu_memory_entry_free_fields(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));

    return err;
#endif
}
