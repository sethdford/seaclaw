/*
 * Hardware-accelerated linear algebra.
 * Apple: Accelerate framework (cblas_sgemm → AMX coprocessor on Apple Silicon,
 *        vDSP for vector ops).
 * aarch64: NEON intrinsics for vector ops when Accelerate unavailable.
 * Other: scalar C fallback.
 */
#include "human/accel.h"

#if defined(__APPLE__) && !defined(HU_IS_TEST)
#include <Accelerate/Accelerate.h>
#define HU_HAS_ACCELERATE 1
#elif (defined(__aarch64__) || defined(__arm64__)) && !defined(HU_IS_TEST)
#include <arm_neon.h>
#define HU_HAS_NEON 1
#endif

void hu_matmul_atb(float *c, const float *a, const float *b, int m, int n, int k)
{
#ifdef HU_HAS_ACCELERATE
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans,
                m, n, k, 1.0f, a, k, b, k, 0.0f, c, n);
#else
    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++) {
            float s = 0.0f;
            for (int t = 0; t < k; t++) s += a[i * k + t] * b[j * k + t];
            c[i * n + j] = s;
        }
#endif
}

void hu_matmul_add_ab(float *c, const float *a, const float *b, int m, int n, int k)
{
#ifdef HU_HAS_ACCELERATE
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                m, k, n, 1.0f, a, n, b, k, 1.0f, c, k);
#else
    for (int i = 0; i < m; i++)
        for (int j = 0; j < k; j++) {
            float s = 0.0f;
            for (int t = 0; t < n; t++) s += a[i * n + t] * b[t * k + j];
            c[i * k + j] += s;
        }
#endif
}

void hu_matmul_add_tab(float *c, const float *a, const float *b, int m, int n, int k)
{
#ifdef HU_HAS_ACCELERATE
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans,
                n, k, m, 1.0f, a, n, b, k, 1.0f, c, k);
#else
    for (int i = 0; i < n; i++)
        for (int j = 0; j < k; j++) {
            float s = 0.0f;
            for (int t = 0; t < m; t++) s += a[t * n + i] * b[t * k + j];
            c[i * k + j] += s;
        }
#endif
}

float hu_dot_f32(const float *a, const float *b, size_t len)
{
#ifdef HU_HAS_ACCELERATE
    float result = 0.0f;
    vDSP_dotpr(a, 1, b, 1, &result, (vDSP_Length)len);
    return result;
#elif defined(HU_HAS_NEON)
    float32x4_t acc = vdupq_n_f32(0.0f);
    size_t i = 0, end4 = len & ~(size_t)3;
    for (; i < end4; i += 4)
        acc = vfmaq_f32(acc, vld1q_f32(a + i), vld1q_f32(b + i));
    float s = vaddvq_f32(acc);
    for (; i < len; i++) s += a[i] * b[i];
    return s;
#else
    float s = 0.0f;
    for (size_t i = 0; i < len; i++) s += a[i] * b[i];
    return s;
#endif
}

float hu_sum_sq_f32(const float *a, size_t len)
{
#ifdef HU_HAS_ACCELERATE
    float result = 0.0f;
    vDSP_svesq(a, 1, &result, (vDSP_Length)len);
    return result;
#elif defined(HU_HAS_NEON)
    float32x4_t acc = vdupq_n_f32(0.0f);
    size_t i = 0, end4 = len & ~(size_t)3;
    for (; i < end4; i += 4) {
        float32x4_t v = vld1q_f32(a + i);
        acc = vfmaq_f32(acc, v, v);
    }
    float s = vaddvq_f32(acc);
    for (; i < len; i++) s += a[i] * a[i];
    return s;
#else
    float s = 0.0f;
    for (size_t i = 0; i < len; i++) s += a[i] * a[i];
    return s;
#endif
}
