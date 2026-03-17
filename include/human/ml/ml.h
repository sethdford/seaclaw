#ifndef HU_ML_H
#define HU_ML_H

#include "human/core/error.h"
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────────────────
 * ML subsystem — top-level types and configuration
 * ────────────────────────────────────────────────────────────────────────── */

typedef enum hu_ml_dtype {
    HU_ML_DTYPE_F32,
    HU_ML_DTYPE_F16,
    HU_ML_DTYPE_BF16,
    HU_ML_DTYPE_I32,
} hu_ml_dtype_t;

typedef enum hu_ml_backend {
    HU_ML_BACKEND_CPU,
    HU_ML_BACKEND_CUDA,
    HU_ML_BACKEND_METAL,
    HU_ML_BACKEND_VULKAN,
} hu_ml_backend_t;

typedef enum hu_ml_activation {
    HU_ML_ACT_RELU_SQ,
    HU_ML_ACT_GELU,
    HU_ML_ACT_SWIGLU,
} hu_ml_activation_t;

typedef struct hu_gpt_config {
    size_t sequence_len;
    size_t vocab_size;
    size_t n_layer;
    size_t n_head;
    size_t n_kv_head;
    size_t n_embd;
    size_t head_dim;
    char window_pattern[8];
    hu_ml_activation_t activation;
    float rope_theta;
    float logit_soft_cap;       /* Soft-cap for logits (0 = default 30.0) */
    int use_value_embeds;       /* Per-layer value embeddings for x0 (0 = shared) */
} hu_gpt_config_t;

typedef struct hu_optimizer_config {
    float embedding_lr;
    float unembedding_lr;
    float matrix_lr;
    float scalar_lr;
    float weight_decay;
    float adam_beta1;
    float adam_beta2;
    float warmup_ratio;
    float warmdown_ratio;
    float final_lr_frac;
    float muon_beta_start;    /* Momentum schedule start (default 0.85) */
    float muon_beta_end;      /* Momentum schedule end (default 0.95) */
    int muon_beta_ramp_steps; /* Steps to ramp from start to end (default 300) */
    float grad_clip_norm;     /* Max gradient norm (0 = disabled) */
    size_t n_embd;            /* For dmodel LR scaling; 0 = disabled */
} hu_optimizer_config_t;

typedef struct hu_training_config {
    size_t device_batch_size;
    int time_budget_secs;
    size_t max_steps;      /* Step-based training limit (0 = use time budget) */
    size_t eval_tokens;
    size_t grad_accum_steps;
    float warmup_ratio;
    float warmdown_ratio;
    float final_lr_frac;
    const char *checkpoint_path; /* Optional path for save/resume */
} hu_training_config_t;

typedef struct hu_ml_train_result {
    double val_bpb;
    double training_seconds;
    double total_seconds;
    double peak_memory_mb;
    double mfu_percent;
    size_t total_tokens;
    size_t num_steps;
    size_t num_params;
    int converged;
} hu_ml_train_result_t;

typedef struct hu_experiment_config {
    hu_gpt_config_t gpt;
    hu_optimizer_config_t optimizer;
    hu_training_config_t training;
    hu_ml_backend_t backend;
} hu_experiment_config_t;

hu_experiment_config_t hu_experiment_config_default(void);

#endif
