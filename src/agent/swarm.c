#include "human/agent/swarm.h"
#include <stdio.h>
#include <string.h>

#if !defined(HU_IS_TEST) || HU_IS_TEST == 0
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#include <sys/time.h>

typedef struct {
    hu_allocator_t *alloc;
    const hu_swarm_config_t *config;
    const hu_swarm_task_t *tasks;
    hu_swarm_task_t *result_tasks;
    size_t task_count;
    pthread_mutex_t next_mu;
    size_t next_idx;
    size_t *completed;
    size_t *failed;
} swarm_ctx_t;

static void *swarm_worker(void *arg) {
    swarm_ctx_t *ctx = (swarm_ctx_t *)arg;
    struct timeval tv_start, tv_end;
    for (;;) {
        size_t idx;
        pthread_mutex_lock(&ctx->next_mu);
        idx = ctx->next_idx;
        if (idx >= ctx->task_count) {
            pthread_mutex_unlock(&ctx->next_mu);
            break;
        }
        ctx->next_idx++;
        pthread_mutex_unlock(&ctx->next_mu);

        hu_swarm_task_t *t = &ctx->result_tasks[idx];
        *t = ctx->tasks[idx];
        gettimeofday(&tv_start, NULL);

        /* Process task: mark completed, set result, record elapsed */
        t->completed = true;
        t->failed = false;
        int n = snprintf(t->result, sizeof(t->result), "processed: %.*s",
            (int)(t->description_len < sizeof(t->result) - 16
                ? t->description_len : sizeof(t->result) - 16),
            t->description);
        t->result_len = (n > 0 && (size_t)n < sizeof(t->result))
            ? (size_t)n : sizeof(t->result) - 1;
        t->result[t->result_len] = '\0';

        gettimeofday(&tv_end, NULL);
        t->elapsed_ms = (int64_t)(tv_end.tv_sec - tv_start.tv_sec) * 1000
            + (int64_t)(tv_end.tv_usec - tv_start.tv_usec) / 1000;
        if (t->elapsed_ms < 1)
            t->elapsed_ms = 1;

        pthread_mutex_lock(&ctx->next_mu);
        (*ctx->completed)++;
        if (t->failed)
            (*ctx->failed)++;
        pthread_mutex_unlock(&ctx->next_mu);
    }
    return NULL;
}
#endif
#endif

hu_swarm_config_t hu_swarm_config_default(void) {
    hu_swarm_config_t c = {0};
    c.max_parallel = 4;
    c.timeout_ms = 30000;
    c.retry_on_failure = 1;
    return c;
}

hu_error_t hu_swarm_execute(hu_allocator_t *alloc, const hu_swarm_config_t *config,
                           hu_swarm_task_t *tasks, size_t task_count,
                           hu_swarm_result_t *result) {
    if (!alloc || !result)
        return HU_ERR_INVALID_ARGUMENT;
    if (task_count > 0 && !tasks)
        return HU_ERR_INVALID_ARGUMENT;

    memset(result, 0, sizeof(*result));
    result->tasks = NULL;
    result->task_count = 0;
    result->completed = 0;
    result->failed = 0;
    result->total_elapsed_ms = 0;

    if (task_count == 0)
        return HU_OK;

    hu_swarm_config_t cfg = config ? *config : hu_swarm_config_default();

    result->tasks = (hu_swarm_task_t *)alloc->alloc(
        alloc->ctx, task_count * sizeof(hu_swarm_task_t));
    if (!result->tasks)
        return HU_ERR_OUT_OF_MEMORY;

    result->task_count = task_count;

#if defined(HU_IS_TEST) && HU_IS_TEST
    int64_t total_ms = 0;
    int max_retries = cfg.retry_on_failure > 0 ? cfg.retry_on_failure : 0;
    for (size_t i = 0; i < task_count; i++) {
        hu_swarm_task_t *t = &result->tasks[i];
        *t = tasks[i];

        /* Simulate failure for tasks containing "fail" in description */
        bool sim_fail = (t->description_len >= 4 &&
                         strstr(t->description, "fail") != NULL);
        bool task_done = false;

        for (int attempt = 0; attempt <= max_retries && !task_done; attempt++) {
            t->elapsed_ms = 1;
            if (sim_fail && attempt < max_retries) {
                continue;
            }
            if (sim_fail) {
                t->completed = false;
                t->failed = true;
                strncpy(t->result, "error: task failed", sizeof(t->result) - 1);
                t->result_len = 18;
                result->failed++;
            } else {
                t->completed = true;
                t->failed = false;
                strncpy(t->result, "mock result", sizeof(t->result) - 1);
                t->result_len = 11;
                result->completed++;
            }
            task_done = true;
        }

        /* Timeout enforcement: mark timed out if elapsed > timeout */
        if (cfg.timeout_ms > 0 && t->elapsed_ms > cfg.timeout_ms) {
            t->completed = false;
            t->failed = true;
        }

        total_ms += t->elapsed_ms;
    }
    result->total_elapsed_ms = total_ms;

#elif defined(__unix__) || defined(__APPLE__)
    /* POSIX: pthread-based parallel execution */
    swarm_ctx_t ctx = {
        .alloc = alloc,
        .config = &cfg,
        .tasks = tasks,
        .result_tasks = result->tasks,
        .task_count = task_count,
        .next_idx = 0,
        .completed = &result->completed,
        .failed = &result->failed,
    };
    if (pthread_mutex_init(&ctx.next_mu, NULL) != 0) {
        alloc->free(alloc->ctx, result->tasks, task_count * sizeof(hu_swarm_task_t));
        result->tasks = NULL;
        result->task_count = 0;
        return HU_ERR_IO;
    }

    size_t num_threads = (size_t)cfg.max_parallel;
    if (num_threads > task_count)
        num_threads = task_count;
    if (num_threads < 1)
        num_threads = 1;

    pthread_t *threads = (pthread_t *)alloc->alloc(
        alloc->ctx, num_threads * sizeof(pthread_t));
    if (!threads) {
        pthread_mutex_destroy(&ctx.next_mu);
        alloc->free(alloc->ctx, result->tasks, task_count * sizeof(hu_swarm_task_t));
        result->tasks = NULL;
        result->task_count = 0;
        return HU_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, swarm_worker, &ctx) != 0) {
            for (size_t j = 0; j < i; j++)
                pthread_join(threads[j], NULL);
            alloc->free(alloc->ctx, threads, num_threads * sizeof(pthread_t));
            pthread_mutex_destroy(&ctx.next_mu);
            alloc->free(alloc->ctx, result->tasks, task_count * sizeof(hu_swarm_task_t));
            result->tasks = NULL;
            result->task_count = 0;
            return HU_ERR_IO;
        }
    }

    int64_t total_ms = 0;
    for (size_t i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);
    alloc->free(alloc->ctx, threads, num_threads * sizeof(pthread_t));
    pthread_mutex_destroy(&ctx.next_mu);

    for (size_t i = 0; i < task_count; i++)
        total_ms += result->tasks[i].elapsed_ms;
    result->total_elapsed_ms = total_ms;

#else
    /* Non-POSIX: sequential fallback */
    (void)cfg;
    int64_t total_ms = 0;
    for (size_t i = 0; i < task_count; i++) {
        hu_swarm_task_t *t = &result->tasks[i];
        *t = tasks[i];
        t->completed = true;
        t->failed = false;
        t->elapsed_ms = 1;
        total_ms += t->elapsed_ms;
        int n = snprintf(t->result, sizeof(t->result), "processed: %.*s",
                         (int)(t->description_len > 80 ? 80 : t->description_len),
                         t->description);
        t->result_len = (n > 0 && (size_t)n < sizeof(t->result)) ? (size_t)n : 0;
        result->completed++;
    }
    result->total_elapsed_ms = total_ms;
#endif

    return HU_OK;
}

hu_error_t hu_swarm_aggregate(const hu_swarm_result_t *result, hu_swarm_aggregation_t strategy,
                               char *out, size_t out_size, size_t *out_len) {
    if (!result || !out || !out_len || out_size == 0)
        return HU_ERR_INVALID_ARGUMENT;

    out[0] = '\0';
    *out_len = 0;

    if (result->task_count == 0 || !result->tasks)
        return HU_OK;

    switch (strategy) {
    case HU_SWARM_AGG_CONCATENATE: {
        size_t pos = 0;
        for (size_t i = 0; i < result->task_count && pos < out_size - 1; i++) {
            if (!result->tasks[i].completed || result->tasks[i].result_len == 0)
                continue;
            if (pos > 0 && pos < out_size - 2) {
                out[pos++] = '\n';
            }
            size_t to_copy = result->tasks[i].result_len;
            if (to_copy > out_size - 1 - pos)
                to_copy = out_size - 1 - pos;
            memcpy(out + pos, result->tasks[i].result, to_copy);
            pos += to_copy;
        }
        out[pos] = '\0';
        *out_len = pos;
        break;
    }
    case HU_SWARM_AGG_FIRST_SUCCESS: {
        for (size_t i = 0; i < result->task_count; i++) {
            if (result->tasks[i].completed && !result->tasks[i].failed &&
                result->tasks[i].result_len > 0) {
                size_t to_copy = result->tasks[i].result_len;
                if (to_copy > out_size - 1)
                    to_copy = out_size - 1;
                memcpy(out, result->tasks[i].result, to_copy);
                out[to_copy] = '\0';
                *out_len = to_copy;
                return HU_OK;
            }
        }
        break;
    }
    case HU_SWARM_AGG_VOTE: {
        /* Simple vote: find most common result */
        size_t best_idx = 0;
        int best_count = 0;
        for (size_t i = 0; i < result->task_count; i++) {
            if (!result->tasks[i].completed || result->tasks[i].result_len == 0)
                continue;
            int count = 0;
            for (size_t j = 0; j < result->task_count; j++) {
                if (result->tasks[j].completed && result->tasks[j].result_len > 0 &&
                    result->tasks[i].result_len == result->tasks[j].result_len &&
                    memcmp(result->tasks[i].result, result->tasks[j].result,
                           result->tasks[i].result_len) == 0) {
                    count++;
                }
            }
            if (count > best_count) {
                best_count = count;
                best_idx = i;
            }
        }
        if (best_count > 0 && result->tasks[best_idx].result_len > 0) {
            size_t to_copy = result->tasks[best_idx].result_len;
            if (to_copy > out_size - 1)
                to_copy = out_size - 1;
            memcpy(out, result->tasks[best_idx].result, to_copy);
            out[to_copy] = '\0';
            *out_len = to_copy;
        }
        break;
    }
    }

    return HU_OK;
}

void hu_swarm_result_free(hu_allocator_t *alloc, hu_swarm_result_t *result) {
    if (!alloc || !result)
        return;
    if (result->tasks) {
        alloc->free(alloc->ctx, result->tasks,
                    result->task_count * sizeof(hu_swarm_task_t));
        result->tasks = NULL;
        result->task_count = 0;
        result->completed = 0;
        result->failed = 0;
    }
}
