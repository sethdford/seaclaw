#include "human/memory/vector_math.h"
#include "human/accel.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

float hu_vector_cosine_similarity(const float *a, const float *b, size_t len) {
    if (!a || !b || len == 0)
        return 0.0f;

#if !defined(HU_IS_TEST)
    float dot = hu_dot_f32(a, b, len);
    float na = hu_sum_sq_f32(a, len);
    float nb = hu_sum_sq_f32(b, len);
    double denom = sqrt((double)na) * sqrt((double)nb);
    if (!isfinite(denom) || denom < 1e-300)
        return 0.0f;
    double raw = (double)dot / denom;
    if (!isfinite(raw))
        return 0.0f;
    if (raw < 0.0)
        raw = 0.0;
    if (raw > 1.0)
        raw = 1.0;
    return (float)raw;
#else
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < len; i++) {
        double x = (double)a[i];
        double y = (double)b[i];
        if (x != x || y != y)
            return 0.0f;
        if (x > 1e38 || x < -1e38 || y > 1e38 || y < -1e38)
            return 0.0f;
        dot += x * y;
        norm_a += x * x;
        norm_b += y * y;
    }
    double denom = sqrt(norm_a) * sqrt(norm_b);
    if (!isfinite(denom) || denom < 1e-300)
        return 0.0f;
    double raw = dot / denom;
    if (!isfinite(raw))
        return 0.0f;
    if (raw < 0.0)
        raw = 0.0;
    if (raw > 1.0)
        raw = 1.0;
    return (float)raw;
#endif
}

unsigned char *hu_vector_to_bytes(hu_allocator_t *alloc, const float *v, size_t len) {
    if (!alloc || !v)
        return NULL;
    if (len > SIZE_MAX / sizeof(float))
        return NULL;
    size_t byte_len = len * sizeof(float);
    unsigned char *bytes = (unsigned char *)alloc->alloc(alloc->ctx, byte_len);
    if (!bytes)
        return NULL;
    for (size_t i = 0; i < len; i++) {
        float f = v[i];
        unsigned char *p = bytes + i * 4;
        memcpy(p, &f, 4);
    }
    return bytes;
}

float *hu_vector_from_bytes(hu_allocator_t *alloc, const unsigned char *bytes, size_t byte_len) {
    if (!alloc || !bytes)
        return NULL;
    size_t count = byte_len / 4;
    if (count == 0)
        return NULL;
    float *v = (float *)alloc->alloc(alloc->ctx, count * sizeof(float));
    if (!v)
        return NULL;
    for (size_t i = 0; i < count; i++) {
        memcpy(&v[i], bytes + i * 4, 4);
    }
    return v;
}
