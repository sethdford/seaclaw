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
    /* Sequential execution under HU_IS_TEST — no real threads in tests */
    (void)cfg;
    int64_t total_ms = 0;
    for (size_t i = 0; i < task_count; i++) {
        hu_swarm_task_t *t = &result->tasks[i];
        *t = tasks[i];
        t->completed = true;
        t->failed = false;
        t->elapsed_ms = 1;
        total_ms += t->elapsed_ms;
        strncpy(t->result, "mock result", sizeof(t->result) - 1);
        t->result[sizeof(t->result) - 1] = '\0';
        t->result_len = 11;
        result->completed++;
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
