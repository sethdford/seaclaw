---
title: ML Subsystem Audit — March 2026
---

# ML Subsystem Audit — March 2026

Audit of the ML subsystem for remaining gaps, bugs, and non-SOTA patterns. Excludes items already fixed (soft-cap, LoRA integration, weight decay, etc.).

---

## 1. LoRA backward input gradients

**Severity: CRITICAL — Fix now**

### Finding

`hu_lora_backward` in `src/ml/lora.c` (lines 183–252) only accumulates param gradients (`grad_A`, `grad_B`). It does **not** return or accumulate the gradient w.r.t. the input.

For LoRA: `out = scale * input @ A^T @ B^T`, so the input gradient is:

```
d_input = scale * grad_output @ B @ A
```

In `gpt.c` backward (e.g. Q projection around lines 737–740):

```c
matmul_add_ab(d_xn, d_q, g->aqw[li], ...);   /* base path: d_xn += d_q @ aqw */
if (g->lora_q) hu_lora_backward(g->lora_q, li, lc->x_norm1, d_q, BS);  /* LoRA: only grad_A, grad_B */
```

`d_xn` receives the gradient from the base projection only. The LoRA contribution `d_q @ B @ A * scale` is never added to `d_xn`.

### Impact

- Combined training (base + LoRA): gradient flow through the LoRA path to the input is missing.
- Upstream layers (RMSNorm, residual, etc.) receive incomplete gradients.
- Training dynamics are wrong; LoRA fine-tuning with base unfrozen is incorrect.

### Fix

Extend `hu_lora_backward` to accept an optional `float *grad_input` buffer and accumulate:

```c
/* grad_input += scale * grad_output @ B @ A */
```

Or add a separate `hu_lora_backward_input` that computes and adds to `grad_input`. GPT backward must call this and add the result into `d_xn` (and analogous buffers for K, V, O, up, down).

---

## 2. LoRA freeze base weights

**Severity: MEDIUM — Acceptable technical debt**

### Finding

There is no explicit “freeze base” flag. Registration is additive:

- `hu_gpt_register_params` registers all base params.
- `hu_lora_register_params` registers LoRA params (A, B per layer).

If both are called, both base and LoRA params are updated.

### Current workaround

For LoRA-only fine-tuning, call **only** `hu_lora_register_params` and **not** `hu_gpt_register_params`. Base params are then never passed to the optimizer and effectively frozen.

### Recommendation

- Document this in `src/ml/CLAUDE.md` and `include/human/ml/lora.h`.
- Optionally add `hu_gpt_register_params_ex(model, opt, HU_GPT_REGISTER_BASE | HU_GPT_REGISTER_LORA)` with a mask to make the intent explicit.

---

## 3. Dead code in `lora_apply`

**Severity: MEDIUM — Fix now**

### Finding

Lines 138–149 of `src/ml/lora.c` contain a dead loop that computes `input @ A^T` and discards the result with `(void)sum`:

```c
for (size_t i = 0; i < batch_tokens; i++) {
    for (size_t j = 0; j < rank; j++) {
        float sum = 0.0f;
        for (size_t k = 0; k < in_dim; k++)
            sum += input[i * in_dim + k] * layer->A[j * in_dim + k];
        (void)sum; /* will use below */
    }
}
```

The same computation is done again in lines 157–165 after allocating `temp`. The first loop is redundant and roughly doubles the cost of the first matmul.

### Fix

Remove lines 138–149 (the dead loop). Keep only the loop that writes into `temp` after allocation.

---

## 4. `train.c` byte-weighted loss

**Severity: LOW — No divide-by-zero; clarify semantics**

### Finding

**Divide-by-zero:** None. When `token_bytes[target] == 0`, tokens are skipped and `n_valid` is incremented only for non-skipped tokens. If all tokens are skipped, `n_valid == 0` and:

- `batch_loss = 0.0` (line 122)
- `grad_scale` is only applied when `n_valid > 0` (line 132)
- `d_logits` remains zero from `memset`; backward runs with zero gradients

**Byte weighting:** The training loss is **not** byte-weighted. It uses a per-token average over non-zero-byte tokens:

- `batch_loss = batch_nats / n_valid`
- Each token contributes equally; there is no weighting by `token_bytes[target]`

The evaluator (`evaluator.c`) correctly computes BPB as `total_nats / (LN2 * total_bytes)` with byte weighting.

### Recommendation

- If byte-weighted training loss is desired: weight each token’s CE by `token_bytes[target]` and normalize by `sum(token_bytes)`.
- Otherwise, document that training uses equal weighting over non-zero-byte tokens.

---

## 5. Experiment config mutation bounds

**Severity: LOW — Acceptable**

### Finding

`mutate_config` in `src/ml/experiment.c` (lines 141–195) has reasonable bounds:

| Field            | Bounds / checks                              |
| ---------------- | -------------------------------------------- |
| `n_layer`        | Only mutated if `> 2`; ±2                    |
| `matrix_lr`      | Clamped to [0.001, 0.2]                      |
| `embedding_lr`   | Clamped to [0.01, 2.0]                       |
| `weight_decay`   | Set to 0.1 or 0.3                            |
| `warmdown_ratio` | 0, 0.25, or 0.5                              |
| `n_embd`         | `hd <= new_dim <= 2048`, `new_dim % hd == 0` |
| `activation`     | Enum 0, 1, 2                                 |
| `adam_beta1`     | 0.8 or 0.9                                   |

- `n_embd` cannot go to 0 when `hd > 0` because of `new_dim >= hd`.
- Learning rates stay positive.
- Invalid configs (e.g. `n_layer == 0`) are rejected by `hu_gpt_create`.

### Recommendation

No change required. Optionally add a post-mutation validation pass before `run_single_experiment`.

---

## 6. Missing error checks for LoRA calls

**Severity: HIGH — Fix now**

### Finding

All `hu_lora_apply` and `hu_lora_backward` call sites in `src/ml/gpt.c` ignore return values:

**Forward (6 call sites):**

- 328–330: Q, K, V
- 361: O
- 375–376: up, down

**Backward (6 call sites):**

- 668, 677, 690: down, up, O
- 740, 744, 748: Q, K, V

If `hu_lora_apply` returns `HU_ERR_OUT_OF_MEMORY` (e.g. when allocating the temp buffer), the error is ignored and forward continues with partially updated buffers. If `hu_lora_backward` fails, param gradients may be incomplete.

### Fix

Check return values and propagate errors:

```c
if (g->lora_q) {
    hu_error_t err = hu_lora_apply(g->lora_q, li, xn, B * S, q_buf);
    if (err != HU_OK) { /* cleanup; return err; */ }
}
```

Same pattern for all LoRA apply/backward calls.

---

## 7. Param registration order

**Severity: LOW — No ordering issues**

### Finding

`hu_gpt_register_params` and `hu_lora_register_params` are independent. Both append params to the optimizer. The optimizer applies updates to all registered params regardless of order.

### Recommendation

No change. Document that either order is fine.

---

## Summary

| #   | Issue                         | Severity | Action                 |
| --- | ----------------------------- | -------- | ---------------------- |
| 1   | LoRA backward missing d_input | CRITICAL | Fix now                |
| 2   | No explicit base freeze       | MEDIUM   | Document; optional API |
| 3   | Dead loop in `lora_apply`     | MEDIUM   | Fix now                |
| 4   | Byte-weighted loss semantics  | LOW      | Clarify / document     |
| 5   | Experiment mutation bounds    | LOW      | Acceptable             |
| 6   | LoRA call sites ignore errors | HIGH     | Fix now                |
| 7   | Param registration order      | LOW      | Document only          |
