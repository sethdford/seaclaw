#ifndef HU_ACCEL_H
#define HU_ACCEL_H

#include <stddef.h>

/*
 * Hardware-accelerated linear algebra operations.
 * On Apple Silicon, routes through Accelerate framework (→ AMX coprocessor).
 * On other platforms, falls back to scalar C implementations.
 */

/* C = A[m×k] @ B[n×k]^T — used for weight projections (Q/K/V, MLP). */
void hu_matmul_atb(float *c, const float *a, const float *b, int m, int n, int k);

/* C[m×k] += A[m×n] @ B[n×k] — used for gradient backprop. */
void hu_matmul_add_ab(float *c, const float *a, const float *b, int m, int n, int k);

/* C[n×k] += A[m×n]^T @ B[m×k] — used for weight gradient accumulation. */
void hu_matmul_add_tab(float *c, const float *a, const float *b, int m, int n, int k);

/* Dot product of two f32 vectors. */
float hu_dot_f32(const float *a, const float *b, size_t len);

/* Sum of squares of f32 vector. */
float hu_sum_sq_f32(const float *a, size_t len);

#endif
