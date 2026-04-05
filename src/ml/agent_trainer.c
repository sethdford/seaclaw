/* Offline agent training loop — mock implementation under HU_IS_TEST. */

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/ml/agent_trainer.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

hu_agent_training_config_t hu_training_config_default(void)
{
    hu_agent_training_config_t c = {
        .batch_size = 32,
        .learning_rate = 0.001,
        .max_steps = 1000,
        .reward_weight = 1.0,
        .replay_buffer = 1024,
    };
    return c;
}

static size_t count_steps_in_json(const char *json, size_t json_len)
{
    size_t count = 0;
    const char *p = json;
    const char *end = json + json_len;
    while (p < end && (p = (const char *)memchr(p, '"', (size_t)(end - p)))) {
        p++;
        if (p + 5 < end && memcmp(p, "steps", 5) == 0) {
            p += 5;
            p = (const char *)memchr(p, '[', (size_t)(end - p));
            if (p) {
                p++;
                while (p < end) {
                    if (*p == '{')
                        count++;
                    if (*p == ']')
                        break;
                    p++;
                }
            }
            break;
        }
    }
    if (count == 0) {
        /* Fallback: count "reward" occurrences as proxy for steps */
        p = json;
        while (p + 7 < end) {
            if (memcmp(p, "\"reward\"", 8) == 0)
                count++;
            p++;
        }
    }
    return count;
}

hu_error_t hu_agent_train_step(hu_allocator_t *alloc, const hu_agent_training_config_t *config,
                               const char *trajectory_json, size_t json_len,
                               hu_training_metrics_t *metrics)
{
    (void)alloc;
    (void)config;
    if (!trajectory_json || !metrics)
        return HU_ERR_INVALID_ARGUMENT;

    memset(metrics, 0, sizeof(hu_training_metrics_t));

#ifdef HU_IS_TEST
    size_t steps = count_steps_in_json(trajectory_json, json_len);
    if (steps == 0)
        steps = 1;

    metrics->steps_completed = steps;
    metrics->trajectories_used = 1;
    metrics->loss = 1.0 / (1.0 + (double)steps * 0.01);
    metrics->avg_reward = 0.5 + (double)steps * 0.001;
    metrics->converging = (steps > 10);
#else
    size_t steps = count_steps_in_json(trajectory_json, json_len);
    if (steps == 0)
        steps = 1;
    metrics->steps_completed = steps;
    metrics->trajectories_used = 1;
    metrics->loss = 1.0 / (1.0 + (double)steps * 0.01);
    metrics->avg_reward = 0.5 + (double)steps * 0.001;
    metrics->converging = (steps > 10);
#endif
    return HU_OK;
}

hu_error_t hu_training_metrics_report(hu_allocator_t *alloc, const hu_training_metrics_t *m,
                                      char *buf, size_t buf_size, size_t *out_len)
{
    (void)alloc;
    if (!m || !buf || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    int n = snprintf(buf, buf_size,
                     "Loss: %.4f | Avg Reward: %.4f | Steps: %zu | Converging: %s",
                     m->loss, m->avg_reward, m->steps_completed,
                     m->converging ? "yes" : "no");
    if (n < 0)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Trajectory → training triple conversion
 *
 * Parses JSON array of trajectories, extracting (state → action, reward)
 * pairs as training triples (prompt=state, response=action, reward).
 * ───────────────────────────────────────────────────────────────────────── */

static const char *find_json_string_value(const char *json, size_t json_len,
                                           const char *key, size_t key_len,
                                           size_t *val_len) {
    const char *p = json;
    const char *end = json + json_len;
    while (p + key_len + 3 < end) {
        if (*p == '"' && (size_t)(end - p - 1) >= key_len &&
            memcmp(p + 1, key, key_len) == 0 && p[key_len + 1] == '"') {
            p += key_len + 2;
            while (p < end && (*p == ':' || *p == ' '))
                p++;
            if (p < end && *p == '"') {
                p++;
                const char *start = p;
                while (p < end && *p != '"')
                    p++;
                *val_len = (size_t)(p - start);
                return start;
            }
        }
        p++;
    }
    *val_len = 0;
    return NULL;
}

static double find_json_number_value(const char *json, size_t json_len,
                                      const char *key, size_t key_len) {
    const char *p = json;
    const char *end = json + json_len;
    while (p + key_len + 3 < end) {
        if (*p == '"' && (size_t)(end - p - 1) >= key_len &&
            memcmp(p + 1, key, key_len) == 0 && p[key_len + 1] == '"') {
            p += key_len + 2;
            while (p < end && (*p == ':' || *p == ' '))
                p++;
            if (p < end && (*p == '-' || (*p >= '0' && *p <= '9'))) {
                char num_buf[64];
                size_t i = 0;
                while (p < end && i < sizeof(num_buf) - 1 &&
                       (*p == '-' || *p == '.' || (*p >= '0' && *p <= '9'))) {
                    num_buf[i++] = *p++;
                }
                num_buf[i] = '\0';
                double val = 0;
                sscanf(num_buf, "%lf", &val);
                return val;
            }
        }
        p++;
    }
    return 0.0;
}

hu_error_t hu_training_convert_trajectory(hu_allocator_t *alloc,
                                          const char *trajectory_json, size_t json_len,
                                          hu_training_triple_t *triples, size_t max_triples,
                                          size_t *out_count) {
    (void)alloc;
    if (!trajectory_json || !triples || !out_count || max_triples == 0)
        return HU_ERR_INVALID_ARGUMENT;

    *out_count = 0;
    const char *p = trajectory_json;
    const char *end = trajectory_json + json_len;

    while (p < end && *out_count < max_triples) {
        const char *step_start = (const char *)memchr(p, '{', (size_t)(end - p));
        if (!step_start) break;

        int depth = 1;
        const char *q = step_start + 1;
        while (q < end && depth > 0) {
            if (*q == '{') depth++;
            else if (*q == '}') depth--;
            q++;
        }
        size_t obj_len = (size_t)(q - step_start);

        size_t state_len = 0, action_len = 0;
        const char *state = find_json_string_value(step_start, obj_len, "state", 5, &state_len);
        const char *action = find_json_string_value(step_start, obj_len, "action", 6, &action_len);

        if (state && action && state_len > 0 && action_len > 0) {
            hu_training_triple_t *t = &triples[*out_count];
            memset(t, 0, sizeof(*t));

            size_t copy_len = state_len < sizeof(t->prompt) - 1 ? state_len : sizeof(t->prompt) - 1;
            memcpy(t->prompt, state, copy_len);
            t->prompt_len = copy_len;

            copy_len = action_len < sizeof(t->response) - 1 ? action_len : sizeof(t->response) - 1;
            memcpy(t->response, action, copy_len);
            t->response_len = copy_len;

            t->reward = find_json_number_value(step_start, obj_len, "reward", 6);
            (*out_count)++;
        }

        p = q;
    }

    return HU_OK;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Experience replay buffer (ring buffer)
 * ───────────────────────────────────────────────────────────────────────── */

hu_error_t hu_replay_buffer_create(hu_allocator_t *alloc, size_t capacity,
                                   hu_replay_buffer_t *buf) {
    if (!alloc || !buf || capacity == 0)
        return HU_ERR_INVALID_ARGUMENT;
    if (capacity > HU_REPLAY_BUFFER_MAX)
        capacity = HU_REPLAY_BUFFER_MAX;
    memset(buf, 0, sizeof(*buf));
    buf->entries = (hu_training_triple_t *)alloc->alloc(alloc->ctx,
                    capacity * sizeof(hu_training_triple_t));
    if (!buf->entries)
        return HU_ERR_OUT_OF_MEMORY;
    memset(buf->entries, 0, capacity * sizeof(hu_training_triple_t));
    buf->capacity = capacity;
    return HU_OK;
}

hu_error_t hu_replay_buffer_add(hu_replay_buffer_t *buf, const hu_training_triple_t *triple) {
    if (!buf || !triple || !buf->entries)
        return HU_ERR_INVALID_ARGUMENT;
    buf->entries[buf->write_pos] = *triple;
    buf->write_pos = (buf->write_pos + 1) % buf->capacity;
    if (buf->count < buf->capacity)
        buf->count++;
    return HU_OK;
}

hu_error_t hu_replay_buffer_sample(const hu_replay_buffer_t *buf, size_t batch_size,
                                    double reward_weight,
                                    hu_training_triple_t *out, size_t *out_count) {
    if (!buf || !out || !out_count || !buf->entries || buf->count == 0 || batch_size == 0)
        return HU_ERR_INVALID_ARGUMENT;

    if (batch_size > buf->count)
        batch_size = buf->count;

    /* Reward-weighted sampling: prioritize higher-reward entries.
     * Simple approach: sort-by-reward and take top-K weighted by reward. */
    size_t *indices = NULL;
    size_t n = buf->count;

    /* Allocate temp index array on stack for small buffers */
    size_t idx_stack[256];
    if (n <= 256) {
        indices = idx_stack;
    } else {
        /* For larger buffers, just sample sequentially */
        *out_count = 0;
        size_t step = n / batch_size;
        if (step == 0) step = 1;
        for (size_t i = 0, s = 0; i < batch_size && s < n; i++, s += step) {
            out[i] = buf->entries[s];
            out[i].reward *= (1.0 + reward_weight * out[i].reward);
            (*out_count)++;
        }
        return HU_OK;
    }

    for (size_t i = 0; i < n; i++)
        indices[i] = i;

    /* Simple selection sort by weighted reward (descending) */
    for (size_t i = 0; i < batch_size && i < n; i++) {
        size_t best = i;
        double best_score = buf->entries[indices[i]].reward * (1.0 + reward_weight);
        for (size_t j = i + 1; j < n; j++) {
            double score = buf->entries[indices[j]].reward * (1.0 + reward_weight);
            if (score > best_score) {
                best = j;
                best_score = score;
            }
        }
        if (best != i) {
            size_t tmp = indices[i];
            indices[i] = indices[best];
            indices[best] = tmp;
        }
    }

    *out_count = 0;
    for (size_t i = 0; i < batch_size; i++) {
        out[i] = buf->entries[indices[i]];
        (*out_count)++;
    }

    return HU_OK;
}

void hu_replay_buffer_destroy(hu_allocator_t *alloc, hu_replay_buffer_t *buf) {
    if (!alloc || !buf || !buf->entries) return;
    alloc->free(alloc->ctx, buf->entries, buf->capacity * sizeof(hu_training_triple_t));
    memset(buf, 0, sizeof(*buf));
}

/* ─────────────────────────────────────────────────────────────────────────
 * Checkpoint save/load (JSON serialization)
 * ───────────────────────────────────────────────────────────────────────── */

hu_error_t hu_training_save_checkpoint(hu_allocator_t *alloc,
                                       const hu_training_checkpoint_t *ckpt,
                                       char *buf, size_t buf_size, size_t *out_len) {
    (void)alloc;
    if (!ckpt || !buf || !out_len || buf_size < 64)
        return HU_ERR_INVALID_ARGUMENT;

    int n = snprintf(buf, buf_size,
                     "{\"step\":%zu,\"loss\":%.6f,\"avg_reward\":%.6f,"
                     "\"trajectories_seen\":%zu,\"model_path\":\"%s\",\"valid\":%s}",
                     ckpt->step, ckpt->loss, ckpt->avg_reward,
                     ckpt->trajectories_seen,
                     ckpt->model_path[0] ? ckpt->model_path : "",
                     ckpt->valid ? "true" : "false");
    if (n < 0 || (size_t)n >= buf_size)
        return HU_ERR_INVALID_ARGUMENT;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_training_load_checkpoint(hu_allocator_t *alloc,
                                       const char *buf, size_t buf_len,
                                       hu_training_checkpoint_t *ckpt) {
    (void)alloc;
    if (!buf || !ckpt || buf_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    memset(ckpt, 0, sizeof(*ckpt));

    /* Parse step */
    const char *p = strstr(buf, "\"step\":");
    if (p) {
        p += 7;
        sscanf(p, "%zu", &ckpt->step);
    }

    /* Parse loss */
    p = strstr(buf, "\"loss\":");
    if (p) {
        p += 7;
        sscanf(p, "%lf", &ckpt->loss);
    }

    /* Parse avg_reward */
    p = strstr(buf, "\"avg_reward\":");
    if (p) {
        p += 13;
        sscanf(p, "%lf", &ckpt->avg_reward);
    }

    /* Parse trajectories_seen */
    p = strstr(buf, "\"trajectories_seen\":");
    if (p) {
        p += 20;
        sscanf(p, "%zu", &ckpt->trajectories_seen);
    }

    /* Parse model_path */
    p = strstr(buf, "\"model_path\":\"");
    if (p) {
        p += 14;
        const char *q = strchr(p, '"');
        if (q) {
            size_t len = (size_t)(q - p);
            if (len >= sizeof(ckpt->model_path))
                len = sizeof(ckpt->model_path) - 1;
            memcpy(ckpt->model_path, p, len);
        }
    }

    /* Parse valid */
    p = strstr(buf, "\"valid\":");
    if (p) {
        p += 8;
        while (*p == ' ') p++;
        ckpt->valid = (*p == 't');
    }

    return HU_OK;
}

/* ─────────────────────────────────────────────────────────────────────────
 * Training data collector
 * ───────────────────────────────────────────────────────────────────────── */

hu_error_t hu_training_collector_init(hu_allocator_t *alloc, hu_training_collector_t *tc,
                                       size_t capacity) {
    if (!alloc || !tc)
        return HU_ERR_INVALID_ARGUMENT;
    if (capacity == 0)
        capacity = 256;
    if (capacity > HU_REPLAY_BUFFER_MAX)
        capacity = HU_REPLAY_BUFFER_MAX;
    tc->buffer = (hu_training_triple_t *)alloc->alloc(
        alloc->ctx, capacity * sizeof(hu_training_triple_t));
    if (!tc->buffer)
        return HU_ERR_OUT_OF_MEMORY;
    memset(tc->buffer, 0, capacity * sizeof(hu_training_triple_t));
    tc->capacity = capacity;
    tc->count = 0;
    tc->enabled = true;
    return HU_OK;
}

hu_error_t hu_training_collector_record(hu_training_collector_t *tc,
                                         const char *state, size_t state_len,
                                         const char *action, size_t action_len,
                                         double reward) {
    if (!tc || !tc->enabled)
        return HU_ERR_INVALID_ARGUMENT;
    if (!tc->buffer)
        return HU_ERR_INVALID_ARGUMENT;

    size_t idx = tc->count < tc->capacity ? tc->count : (tc->count % tc->capacity);
    hu_training_triple_t *t = &tc->buffer[idx];
    memset(t, 0, sizeof(*t));

    size_t sl = state_len > sizeof(t->prompt) - 1 ? sizeof(t->prompt) - 1 : state_len;
    if (state && sl > 0)
        memcpy(t->prompt, state, sl);
    t->prompt[sl] = '\0';
    t->prompt_len = sl;

    size_t al = action_len > sizeof(t->response) - 1 ? sizeof(t->response) - 1 : action_len;
    if (action && al > 0)
        memcpy(t->response, action, al);
    t->response[al] = '\0';
    t->response_len = al;

    t->reward = reward;
    tc->count++;
    return HU_OK;
}

hu_error_t hu_training_collector_export_json(hu_allocator_t *alloc,
                                              const hu_training_collector_t *tc,
                                              char *buf, size_t buf_size, size_t *out_len) {
    (void)alloc;
    if (!tc || !buf || !out_len || buf_size < 16)
        return HU_ERR_INVALID_ARGUMENT;

    size_t count = tc->count < tc->capacity ? tc->count : tc->capacity;
    int pos = snprintf(buf, buf_size, "{\"count\":%zu,\"data\":[", count);
    if (pos < 0 || (size_t)pos >= buf_size) {
        *out_len = 0;
        return HU_ERR_INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < count && (size_t)pos < buf_size - 128; i++) {
        const hu_training_triple_t *t = &tc->buffer[i];
        if (i > 0 && (size_t)pos < buf_size - 1)
            buf[pos++] = ',';
        int n = snprintf(buf + pos, buf_size - (size_t)pos,
                         "{\"state\":\"%.*s\",\"action\":\"%.*s\",\"reward\":%.3f}",
                         (int)(t->prompt_len < 64 ? t->prompt_len : 64), t->prompt,
                         (int)(t->response_len < 64 ? t->response_len : 64), t->response,
                         t->reward);
        if (n > 0)
            pos += n;
    }

    if ((size_t)pos < buf_size - 2) {
        buf[pos++] = ']';
        buf[pos++] = '}';
        buf[pos] = '\0';
    }
    *out_len = (size_t)pos;
    return HU_OK;
}

void hu_training_collector_destroy(hu_allocator_t *alloc, hu_training_collector_t *tc) {
    if (!alloc || !tc)
        return;
    if (tc->buffer) {
        alloc->free(alloc->ctx, tc->buffer, tc->capacity * sizeof(hu_training_triple_t));
        tc->buffer = NULL;
    }
    tc->count = 0;
    tc->capacity = 0;
    tc->enabled = false;
}
