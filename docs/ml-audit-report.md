---
title: ML Subsystem Audit Report
---

# ML Subsystem Audit Report

**Scope:** C implementation in `src/ml/`, `include/human/ml/`, `tests/test_ml.c`  
**Reference:** [autoresearch](https://github.com/sethdford/autoresearch) (Python LLM training framework)  
**Date:** 2025-03-14

---

## Executive Summary

The C port implements the core autoresearch architecture (RMSNorm, RoPE, residual lambdas, logit soft-cap, Muon optimizer) with generally correct backward passes. Several issues require fixes or additional tests. The most critical are: **head_norm backward** omits the 1/||x|| scaling factor, **window attention** uses the wrong short-window size and does not force the last layer to full attention, and **Value Embeddings** are not implemented (autoresearch feature).

---

## 1. Mathematical Correctness of Backward Passes

### 1.1 RMS Norm Backward — **Correct**

**File:** `src/ml/gpt.c` lines 256–266

Standard formula: `y = x / sqrt(mean(x²) + eps)`, so  
`dx_i = (dy_i - y_i * dot(dy,y)/n) / rms`

The implementation uses `inv = 1/sqrt(sq/n + eps)` and `dx[i] += inv * (dy[i] - y[i] * dot)` with `dot = sum(dy*y)/n`. This matches the correct Jacobian.

### 1.2 RoPE Backward — **Correct**

**File:** `src/ml/gpt.c` lines 269–294

Forward: `[y1; y2] = R @ [x1; x2]` with `R = [cos -sin; sin cos]`.  
Backward uses `R^T`: `dx1 = dy1*cos + dy2*sin`, `dx2 = -dy1*sin + dy2*cos`. Implementation matches.

### 1.3 Attention Backward — **Correct**

**File:** `src/ml/gpt.c` lines 669–711

- **dQ, dK, dV:** Correct accumulation via `d_wbuf`, `d_sbuf`, and softmax Jacobian `scale * w * (dw - dot_wdw)`.
- **dW_q, dW_k, dW_v, dW_o:** Correct `matmul_add_tab` for weight gradients.

### 1.4 Softmax Backward — **Correct**

**File:** `src/ml/gpt.c` lines 690–694

Uses the correct softmax Jacobian: `d_sbuf[j] = scale * wi[j] * (d_wbuf[j] - dot_wdw)` where `dot_wdw = sum_j w[j]*d_wbuf[j]`.

### 1.5 Residual Lambda Backward — **Correct**

**File:** `src/ml/gpt.c` lines 734–741

For `x_mixed = rl*x_pre + x0l*x0`:

- `grad_rl += dx * x_pre` ✓
- `grad_x0l += dx * x0` ✓
- `d_x0 += x0l * dx` ✓
- `dx *= rlv` (propagate to x_pre) ✓

### 1.6 Soft-Capping Backward — **Correct**

**File:** `src/ml/gpt.c` lines 601–606

Forward: `y = 15*tanh(x/15)`. Derivative: `dy/dx = 1 - tanh²(x/15)`.  
Implementation uses `t = c->logits[i]/15 = tanh(x/15)`, so `d_raw = d_capped * (1 - t²)` ✓

### 1.7 Head Normalization Backward — **Bug (approximation)**

**File:** `src/ml/gpt.c` lines 213–229

Correct L2-norm backward: `dx = (dy - y*(y·dy)) / ||x||`.

The code uses `dv[d] -= yv[d] * dot` (i.e. `dx = dy - y*(y·dy)`) and **omits the 1/||x|| factor**. The comment states this is intentional (“approximate inv ≈ 1”), but it introduces a magnitude error that scales with the pre-norm vector length.

**Severity:** Important  
**Fix:** Cache `||x_pre||` in the forward pass and use it in the backward, or document the approximation and its impact.

---

## 2. SwiGLU Backward Correctness — **Correct**

**File:** `src/ml/gpt.c` lines 107–138, 639–653

- Forward: `up_buf[0:n/2]` = `silu(gate) * value`; `up_buf[n/2:n]` is unused.
- Down projection: `matmul_add_ab(d_up, d_mlp, W_down, BS, E, nd)` fills `d_up[0:nd]` = `d_up[0:n/2]`.
- `apply_activation_bw` reads `d_out[i]` for `i < n/2` and writes:
  - `d_out[i] = d * x_val * silu_d` (gate gradient)
  - `d_out[i+n/2] = d * silu` (value gradient)
- Both halves of `d_up` are populated before `matmul_add_ab(d_xn, d_up, W_up, BS, nm, E)`.

The chain is correct.

---

## 3. Logit Soft-Capping — **Implemented Correctly**

**File:** `src/ml/gpt.c` lines 522–523 (forward), 601–606 (backward)

- Forward: `logits[i] = 15.0f * tanhf(logits[i] / 15.0f)` ✓
- Backward: `d_raw = d_capped * (1 - t²)` with `t = logits/15 = tanh(raw/15)` ✓

---

## 4. Residual Stream — **Correct**

**File:** `src/ml/gpt.c` lines 404–405, 422–424, 734–741

- Forward: `x = rl*x + x0l*x0` per layer ✓
- Backward: `grad_rl`, `grad_x0l`, `d_x0`, and `dx` propagation are correct ✓

---

## 5. Weight Initialization — **Minor Mismatches**

**File:** `src/ml/gpt.c` lines 899–937

| Component        | Autoresearch         | C Implementation              | Status                                                                                         |
| ---------------- | -------------------- | ----------------------------- | ---------------------------------------------------------------------------------------------- |
| wte              | N(0, 1)              | prng\*2 → U[-1,1] ≈ N(0,1)    | OK                                                                                             |
| lm_head          | N(0, 0.001)          | prng\*0.002 → U[-0.001,0.001] | OK                                                                                             |
| Transformer      | U(-s,s), s=√3·E^-0.5 | tf_sc = √3/√E \* 2            | **Bug:** C uses `*2` (U[-s,s]) but `prng_next` is [-0.5,0.5], so range is [-s,s] ✓ Actually OK |
| c_proj, mlp_down | zeros                | zeros                         | OK                                                                                             |
| resid_lambdas    | fill\_(1.0)          | 1.0f                          | OK                                                                                             |
| x0_lambdas       | fill\_(0.1)          | 0.1f                          | OK                                                                                             |

**Note:** Autoresearch uses `x0_lambdas = nn.Parameter(torch.zeros(...))` then `fill_(0.1)`, so init is 0.1. C matches.

---

## 6. Optimizer (Muon Nesterov) — **Correct**

**File:** `src/ml/muon_adamw.c` lines 211–221

Reference: `buf = momentum*buf + (1-momentum)*grad`, `g_nesterov = grad + momentum*buf`

C code:

```c
g->momentum_buf[i] = beta * g->momentum_buf[i] + (1.0f - beta) * grad_val;
g_buf[i] = g->grad[i] + beta * g->momentum_buf[i];
```

This matches the Nesterov formula.

**Note:** Autoresearch uses `g = grad.lerp_(buf, momentum)` = `(1-momentum)*grad + momentum*buf`, which is a different Nesterov variant. The C implementation follows the standard `grad + momentum*buf` form.

---

## 7. Architecture Mismatches vs. Autoresearch

### 7.1 Value Embeddings — **Missing**

**Reference:** `train.py` lines 77–88, 134–136, 169–170

Autoresearch has per-layer value embeddings with input-dependent gates. The C implementation has no value embeddings.

**Severity:** Important (feature gap)

### 7.2 Window Attention — **Wrong Size and Last-Layer Rule**

**File:** `src/ml/gpt.c` lines 28–38, 444–456

| Aspect           | Autoresearch          | C Implementation            |
| ---------------- | --------------------- | --------------------------- |
| Short window (S) | `sequence_len // 2`   | `sequence_len / 4`          |
| Last layer       | Always full attention | Uses pattern for all layers |

**Severity:** Important  
**Fix:** Use `sequence_len / 2` for S and force the last layer to full attention.

### 7.3 MLP Activation — **Extension**

Autoresearch uses ReLU². The C implementation supports ReLU², GELU, and SwiGLU. This is an extension, not a bug.

---

## 8. Test Gaps

| Code Path                     | Test Coverage                               | Recommendation                 |
| ----------------------------- | ------------------------------------------- | ------------------------------ |
| Soft-capping forward/backward | None                                        | Add finite-diff or unit test   |
| Residual lambda fwd/bwd       | None                                        | Add unit test                  |
| RoPE backward                 | Indirect (full model)                       | Add isolated unit test         |
| Attention backward            | Indirect (finite-diff on lm_head)           | Add isolated unit test         |
| Multi-layer (2+ layers)       | `test_gpt_forward_logits_finite` (2 layers) | Add backward + finite-diff     |
| GQA (n_kv_head < n_head)      | **Zero**                                    | All tests use n_kv_head=n_head |
| Head norm backward            | None                                        | Add unit test                  |
| Window attention backward     | `test_gpt_window_attention` (forward only)  | Add backward test              |
| SwiGLU backward               | `test_gpt_swiglu_activation` (runs only)    | Add finite-diff                |

---

## Prioritized Issue List

| #   | Severity      | File:Line     | Issue                                        | Action                        |
| --- | ------------- | ------------- | -------------------------------------------- | ----------------------------- | --- | --- | --- | ---------------------------------- |
| 1   | **Critical**  | gpt.c:213–229 | Head norm backward omits 1/                  |                               | x   |     |     | Code fix: cache pre-norm magnitude |
| 2   | **Important** | gpt.c:36      | Window S uses seq_len/4, should be seq_len/2 | Code fix                      |
| 3   | **Important** | gpt.c:29–38   | Last layer not forced to full attention      | Code fix                      |
| 4   | **Important** | —             | Value embeddings not implemented             | Design decision / future work |
| 5   | **Important** | test_ml.c     | GQA (n_kv_head < n_head) has zero coverage   | Add test                      |
| 6   | **Medium**    | test_ml.c     | Soft-capping not tested                      | Add test                      |
| 7   | **Medium**    | test_ml.c     | Residual lambda not tested                   | Add test                      |
| 8   | **Medium**    | test_ml.c     | RoPE backward not tested in isolation        | Add test                      |
| 9   | **Medium**    | test_ml.c     | Attention backward not tested in isolation   | Add test                      |
| 10  | **Low**       | test_ml.c     | Multi-layer backward finite-diff             | Add test                      |
| 11  | **Low**       | test_ml.c     | SwiGLU finite-diff gradient check            | Add test                      |

---

## Recommendations

1. **Fix head_norm_bw** by caching `||x_pre||` in the forward pass (e.g. in `gpt_lcache_t`) and using it in the backward.
2. **Align window attention** with autoresearch: `S → seq_len/2`, and force the last layer to full attention.
3. **Add targeted tests** for soft-capping, residual lambda, RoPE backward, attention backward, and GQA.
4. **Document** the head_norm approximation if keeping it, including when it is acceptable.
5. **Consider** implementing value embeddings if full parity with autoresearch is required.
