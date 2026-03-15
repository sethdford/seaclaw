#ifndef HU_ML_MODEL_H
#define HU_ML_MODEL_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/ml/ml.h"
#include <stddef.h>

/* ──────────────────────────────────────────────────────────────────────────
 * Model vtable interface
 * ────────────────────────────────────────────────────────────────────────── */

typedef struct hu_ml_tensor {
    void *data;
    size_t shape[4];
    int ndim;
    hu_ml_dtype_t dtype;
    size_t size_bytes;
} hu_ml_tensor_t;

typedef struct hu_model_vtable hu_model_vtable_t;

typedef struct hu_model {
    void *ctx;
    const hu_model_vtable_t *vtable;
} hu_model_t;

typedef struct hu_model_vtable {
    hu_error_t (*forward)(void *ctx, const hu_ml_tensor_t *input,
                          hu_ml_tensor_t *output);
    hu_error_t (*backward)(void *ctx, const hu_ml_tensor_t *grad_output);
    hu_error_t (*get_params)(void *ctx, hu_ml_tensor_t **params, size_t *count);
    size_t (*num_params)(void *ctx);
    void (*deinit)(void *ctx, hu_allocator_t *alloc);
} hu_model_vtable_t;

/* GPT model factory (CPU reference implementation). */
hu_error_t hu_gpt_create(hu_allocator_t *alloc, const hu_gpt_config_t *config,
                          hu_model_t *out);

/* Register all GPT parameters with the optimizer (param + grad pairs).
 * Forward-declared to avoid circular include with optimizer.h. */
struct hu_ml_optimizer;
hu_error_t hu_gpt_register_params(hu_model_t *model, struct hu_ml_optimizer *opt);

/* Attach LoRA adapters to a GPT model. Pass NULL for projections without LoRA.
 * Adapters are NOT owned by the model — caller manages their lifetime. */
struct hu_lora_adapter;
hu_error_t hu_gpt_attach_lora(hu_model_t *model,
                               struct hu_lora_adapter *q, struct hu_lora_adapter *k,
                               struct hu_lora_adapter *v, struct hu_lora_adapter *o,
                               struct hu_lora_adapter *up, struct hu_lora_adapter *down);

#endif
