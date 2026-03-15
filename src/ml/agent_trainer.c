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

#ifdef HU_IS_TEST

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

#endif /* HU_IS_TEST */

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
    (void)trajectory_json;
    (void)json_len;
    return HU_ERR_NOT_SUPPORTED;
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
