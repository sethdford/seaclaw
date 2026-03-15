#ifndef HU_ML_AGENT_TRAINER_H
#define HU_ML_AGENT_TRAINER_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct hu_agent_training_config {
    size_t batch_size;      /* default 32 */
    double learning_rate;   /* default 0.001 */
    size_t max_steps;       /* default 1000 */
    double reward_weight;   /* default 1.0 — higher-reward trajectories weighted more */
    size_t replay_buffer;   /* default 1024 — max trajectories in buffer */
} hu_agent_training_config_t;

typedef struct hu_training_metrics {
    double loss;
    double avg_reward;
    size_t steps_completed;
    size_t trajectories_used;
    bool converging;        /* loss is decreasing */
} hu_training_metrics_t;

hu_agent_training_config_t hu_training_config_default(void);
hu_error_t hu_agent_train_step(hu_allocator_t *alloc, const hu_agent_training_config_t *config,
                               const char *trajectory_json, size_t json_len,
                               hu_training_metrics_t *metrics);
hu_error_t hu_training_metrics_report(hu_allocator_t *alloc, const hu_training_metrics_t *m,
                                      char *buf, size_t buf_size, size_t *out_len);

#endif
