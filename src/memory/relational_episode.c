#include "human/memory/relational_episode.h"
#include "human/core/string.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HU_REL_SQL_ESC 768

/* Returns true if the full input was written (well-formed SQL literal); false if truncated. */
static bool escape_sql_lit(const char *s, size_t len, char *buf, size_t cap, size_t *out_len) {
    size_t pos = 0;
    size_t i = 0;
    for (; i < len && pos + 2 < cap; i++) {
        if (s[i] == '\'') {
            buf[pos++] = '\'';
            buf[pos++] = '\'';
        } else {
            buf[pos++] = s[i];
        }
    }
    buf[pos] = '\0';
    *out_len = pos;
    return i == len;
}

void hu_relational_episode_init(hu_relational_episode_t *ep) {
    if (!ep)
        return;
    memset(ep, 0, sizeof(*ep));
}

void hu_relational_episode_free(hu_allocator_t *alloc, hu_relational_episode_t *ep) {
    if (!alloc || !ep)
        return;
    hu_str_free(alloc, ep->contact_id);
    hu_str_free(alloc, ep->summary);
    hu_str_free(alloc, ep->felt_sense);
    hu_str_free(alloc, ep->relational_meaning);
    for (size_t i = 0; i < ep->tag_count; i++)
        hu_str_free(alloc, ep->tags[i]);
    hu_relational_episode_init(ep);
}

hu_error_t hu_relational_episode_set(hu_allocator_t *alloc, hu_relational_episode_t *ep,
                                     const char *contact_id, const char *summary,
                                     const char *felt_sense, const char *relational_meaning,
                                     float significance, float warmth, uint64_t timestamp) {
    if (!alloc || !ep)
        return HU_ERR_INVALID_ARGUMENT;

    const char *cid = contact_id ? contact_id : "";
    size_t cid_len = strlen(cid);
    char *new_cid = hu_strndup(alloc, cid, cid_len);
    if (!new_cid)
        return HU_ERR_OUT_OF_MEMORY;

    const char *sum = summary ? summary : "";
    size_t sum_len = strlen(sum);
    char *new_sum = hu_strndup(alloc, sum, sum_len);
    if (!new_sum) {
        hu_str_free(alloc, new_cid);
        return HU_ERR_OUT_OF_MEMORY;
    }

    const char *felt = felt_sense ? felt_sense : "";
    size_t felt_len = strlen(felt);
    char *new_felt = hu_strndup(alloc, felt, felt_len);
    if (!new_felt) {
        hu_str_free(alloc, new_cid);
        hu_str_free(alloc, new_sum);
        return HU_ERR_OUT_OF_MEMORY;
    }

    const char *rel = relational_meaning ? relational_meaning : "";
    size_t rel_len = strlen(rel);
    char *new_rel = hu_strndup(alloc, rel, rel_len);
    if (!new_rel) {
        hu_str_free(alloc, new_cid);
        hu_str_free(alloc, new_sum);
        hu_str_free(alloc, new_felt);
        return HU_ERR_OUT_OF_MEMORY;
    }

    hu_str_free(alloc, ep->contact_id);
    hu_str_free(alloc, ep->summary);
    hu_str_free(alloc, ep->felt_sense);
    hu_str_free(alloc, ep->relational_meaning);
    ep->contact_id = new_cid;
    ep->summary = new_sum;
    ep->felt_sense = new_felt;
    ep->relational_meaning = new_rel;
    ep->significance = significance;
    ep->warmth = warmth;
    ep->timestamp = timestamp;
    return HU_OK;
}

hu_error_t hu_relational_episode_add_tag(hu_allocator_t *alloc, hu_relational_episode_t *ep,
                                         const char *tag) {
    if (!alloc || !ep || !tag)
        return HU_ERR_INVALID_ARGUMENT;
    if (ep->tag_count >= HU_RELATIONAL_EPISODE_MAX_TAGS)
        return HU_ERR_LIMIT_REACHED;
    size_t tl = strlen(tag);
    char *copy = hu_strndup(alloc, tag, tl);
    if (!copy)
        return HU_ERR_OUT_OF_MEMORY;
    ep->tags[ep->tag_count++] = copy;
    return HU_OK;
}

static size_t pick_next_best(const hu_relational_episode_t *episodes, size_t count,
                             const size_t *picked, size_t picked_n) {
    size_t best_i = SIZE_MAX;
    float best_s = -1.0f;
    for (size_t i = 0; i < count; i++) {
        bool skip = false;
        for (size_t p = 0; p < picked_n; p++) {
            if (picked[p] == i) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;
        if (episodes[i].summary == NULL)
            continue;
        if (episodes[i].significance > best_s) {
            best_s = episodes[i].significance;
            best_i = i;
        }
    }
    return best_i;
}

hu_error_t hu_relational_episode_build_context(hu_allocator_t *alloc,
                                               const hu_relational_episode_t *episodes,
                                               size_t count, char **out, size_t *out_len) {
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;
    if (!episodes || count == 0)
        return HU_OK;

    size_t n_take = count > 3u ? 3u : count;
    size_t picked[3];
    size_t picked_n = 0;

    char chunk[3][384];
    size_t chunk_len[3];
    size_t n_chunks = 0;

    for (size_t k = 0; k < n_take; k++) {
        size_t idx = pick_next_best(episodes, count, picked, picked_n);
        if (idx == SIZE_MAX)
            break;
        picked[picked_n++] = idx;
        const hu_relational_episode_t *ep = &episodes[idx];
        float sig_pct = ep->significance * 100.0f;
        if (sig_pct > 100.0f)
            sig_pct = 100.0f;
        if (sig_pct < 0.0f)
            sig_pct = 0.0f;
        int n = snprintf(chunk[n_chunks], sizeof(chunk[n_chunks]),
                           "[RELATIONAL MEMORY: %s — it felt %s. %s. (Significance: %.0f%%)]",
                           ep->summary, ep->felt_sense ? ep->felt_sense : "",
                           ep->relational_meaning ? ep->relational_meaning : "", (double)sig_pct);
        if (n <= 0 || (size_t)n >= sizeof(chunk[n_chunks]))
            return HU_ERR_INVALID_ARGUMENT;
        chunk_len[n_chunks] = (size_t)n;
        n_chunks++;
    }

    if (n_chunks == 0)
        return HU_OK;

    size_t total = 0;
    for (size_t i = 0; i < n_chunks; i++) {
        total += chunk_len[i];
        if (i + 1 < n_chunks)
            total++;
    }

    char stack_buf[1200];
    if (total + 1 > sizeof(stack_buf))
        return HU_ERR_INVALID_ARGUMENT;
    size_t pos = 0;
    for (size_t i = 0; i < n_chunks; i++) {
        memcpy(stack_buf + pos, chunk[i], chunk_len[i]);
        pos += chunk_len[i];
        if (i + 1 < n_chunks)
            stack_buf[pos++] = '\n';
    }
    stack_buf[pos] = '\0';
    char *dup = hu_strndup(alloc, stack_buf, pos);
    if (!dup)
        return HU_ERR_OUT_OF_MEMORY;
    *out = dup;
    *out_len = pos;
    return HU_OK;
}

hu_error_t hu_relational_episode_create_table_sql(char *buf, size_t cap, size_t *out_len) {
    if (!buf || !out_len || cap < 128)
        return HU_ERR_INVALID_ARGUMENT;
    static const char sql[] =
        "CREATE TABLE IF NOT EXISTS relational_episodes (id INTEGER PRIMARY KEY, contact_id TEXT "
        "NOT NULL, summary TEXT, felt_sense TEXT, relational_meaning TEXT, significance REAL, "
        "warmth REAL, timestamp INTEGER, tags TEXT)";
    size_t slen = sizeof(sql) - 1;
    if (slen >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(buf, sql, slen + 1);
    *out_len = slen;
    return HU_OK;
}

hu_error_t hu_relational_episode_insert_sql(const hu_relational_episode_t *ep,
                                            char *buf, size_t cap, size_t *out_len) {
    if (!ep || !buf || !out_len || cap < 256)
        return HU_ERR_INVALID_ARGUMENT;
    if (!ep->contact_id)
        return HU_ERR_INVALID_ARGUMENT;

    char cid_esc[HU_REL_SQL_ESC];
    char sum_esc[HU_REL_SQL_ESC];
    char felt_esc[HU_REL_SQL_ESC];
    char rel_esc[HU_REL_SQL_ESC];
    char tags_esc[HU_REL_SQL_ESC * 2];
    size_t cid_l, sum_l, felt_l, rel_l;

    if (!escape_sql_lit(ep->contact_id, strlen(ep->contact_id), cid_esc, sizeof(cid_esc), &cid_l))
        return HU_ERR_INVALID_ARGUMENT;
    if (!escape_sql_lit(ep->summary ? ep->summary : "", ep->summary ? strlen(ep->summary) : 0,
                        sum_esc, sizeof(sum_esc), &sum_l))
        return HU_ERR_INVALID_ARGUMENT;
    if (!escape_sql_lit(ep->felt_sense ? ep->felt_sense : "",
                        ep->felt_sense ? strlen(ep->felt_sense) : 0, felt_esc, sizeof(felt_esc),
                        &felt_l))
        return HU_ERR_INVALID_ARGUMENT;
    if (!escape_sql_lit(ep->relational_meaning ? ep->relational_meaning : "",
                        ep->relational_meaning ? strlen(ep->relational_meaning) : 0, rel_esc,
                        sizeof(rel_esc), &rel_l))
        return HU_ERR_INVALID_ARGUMENT;

    size_t tg_pos = 0;
    tags_esc[0] = '\0';
    for (size_t i = 0; i < ep->tag_count; i++) {
        const char *t = ep->tags[i] ? ep->tags[i] : "";
        char one[HU_REL_SQL_ESC];
        size_t ol;
        if (!escape_sql_lit(t, strlen(t), one, sizeof(one), &ol))
            return HU_ERR_INVALID_ARGUMENT;
        if (i > 0) {
            if (tg_pos + 1 >= sizeof(tags_esc))
                return HU_ERR_INVALID_ARGUMENT;
            tags_esc[tg_pos++] = ',';
        }
        if (tg_pos + ol >= sizeof(tags_esc))
            return HU_ERR_INVALID_ARGUMENT;
        memcpy(tags_esc + tg_pos, one, ol);
        tg_pos += ol;
        tags_esc[tg_pos] = '\0';
    }

    /* %f assumes C/POSIX decimal point (LC_NUMERIC "C"); this process does not change locale. */
    int n =
        snprintf(buf, cap,
                 "INSERT INTO relational_episodes (contact_id, summary, felt_sense, "
                 "relational_meaning, significance, warmth, timestamp, tags) VALUES "
                 "('%.*s', '%.*s', '%.*s', '%.*s', %f, %f, %llu, '%.*s')",
                 (int)cid_l, cid_esc, (int)sum_l, sum_esc, (int)felt_l, felt_esc, (int)rel_l,
                 rel_esc, (double)ep->significance, (double)ep->warmth,
                 (unsigned long long)ep->timestamp, (int)tg_pos, tags_esc);
    if (n < 0 || (size_t)n >= cap)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}
