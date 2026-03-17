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

/* Training triple: (prompt, response, reward) extracted from trajectories */
typedef struct hu_training_triple {
    char prompt[2048];
    size_t prompt_len;
    char response[2048];
    size_t response_len;
    double reward;
} hu_training_triple_t;

/* Experience replay buffer */
#define HU_REPLAY_BUFFER_MAX 2048

typedef struct hu_replay_buffer {
    hu_training_triple_t *entries;
    size_t capacity;
    size_t count;
    size_t write_pos;
} hu_replay_buffer_t;

/* Checkpoint state */
typedef struct hu_training_checkpoint {
    size_t step;
    double loss;
    double avg_reward;
    size_t trajectories_seen;
    char model_path[256];
    bool valid;
} hu_training_checkpoint_t;

hu_agent_training_config_t hu_training_config_default(void);
hu_error_t hu_agent_train_step(hu_allocator_t *alloc, const hu_agent_training_config_t *config,
                               const char *trajectory_json, size_t json_len,
                               hu_training_metrics_t *metrics);
hu_error_t hu_training_metrics_report(hu_allocator_t *alloc, const hu_training_metrics_t *m,
                                      char *buf, size_t buf_size, size_t *out_len);

hu_error_t hu_training_convert_trajectory(hu_allocator_t *alloc,
                                          const char *trajectory_json, size_t json_len,
                                          hu_training_triple_t *triples, size_t max_triples,
                                          size_t *out_count);

hu_error_t hu_replay_buffer_create(hu_allocator_t *alloc, size_t capacity,
                                   hu_replay_buffer_t *buf);
hu_error_t hu_replay_buffer_add(hu_replay_buffer_t *buf, const hu_training_triple_t *triple);
hu_error_t hu_replay_buffer_sample(const hu_replay_buffer_t *buf, size_t batch_size,
                                    double reward_weight,
                                    hu_training_triple_t *out, size_t *out_count);
void hu_replay_buffer_destroy(hu_allocator_t *alloc, hu_replay_buffer_t *buf);

hu_error_t hu_training_save_checkpoint(hu_allocator_t *alloc,
                                       const hu_training_checkpoint_t *ckpt,
                                       char *buf, size_t buf_size, size_t *out_len);
hu_error_t hu_training_load_checkpoint(hu_allocator_t *alloc,
                                       const char *buf, size_t buf_len,
                                       hu_training_checkpoint_t *ckpt);

/* Training data collector: records (state, action, reward) from agent turns */
typedef struct hu_training_collector {
    hu_training_triple_t *buffer;
    size_t capacity;
    size_t count;
    bool enabled;
} hu_training_collector_t;

hu_error_t hu_training_collector_init(hu_allocator_t *alloc, hu_training_collector_t *tc,
                                       size_t capacity);
hu_error_t hu_training_collector_record(hu_training_collector_t *tc,
                                         const char *state, size_t state_len,
                                         const char *action, size_t action_len,
                                         double reward);
hu_error_t hu_training_collector_export_json(hu_allocator_t *alloc,
                                              const hu_training_collector_t *tc,
                                              char *buf, size_t buf_size, size_t *out_len);
void hu_training_collector_destroy(hu_allocator_t *alloc, hu_training_collector_t *tc);

#endif
