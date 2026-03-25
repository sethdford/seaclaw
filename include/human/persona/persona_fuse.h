#ifndef HU_PERSONA_FUSE_H
#define HU_PERSONA_FUSE_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * PersonaFuse — per-channel lightweight persona overlays.
 *
 * Instead of full persona switching per channel, PersonaFuse applies
 * thin adaptation layers that adjust formality, length, emoji usage,
 * and vocabulary without reloading the entire persona profile.
 *
 * Adapters are composable: multiple can stack (e.g., "professional" +
 * "brief" for Slack work channels).
 */

#define HU_PERSONA_FUSE_MAX_ADAPTERS 8
#define HU_PERSONA_FUSE_MAX_VOCAB    16

typedef struct hu_fuse_adapter {
    char *name;
    size_t name_len;
    float formality;     /* -1.0 (very casual) to +1.0 (very formal); 0 = neutral */
    float verbosity;     /* -1.0 (terse) to +1.0 (verbose); 0 = neutral */
    float emoji_factor;  /* 0.0 = strip all, 1.0 = normal, 2.0 = double emoji */
    float warmth_offset; /* added to persona base warmth (-0.5 to +0.5) */
    char *preferred_vocab[HU_PERSONA_FUSE_MAX_VOCAB];
    size_t preferred_vocab_count;
    char *avoided_vocab[HU_PERSONA_FUSE_MAX_VOCAB];
    size_t avoided_vocab_count;
} hu_fuse_adapter_t;

typedef struct hu_persona_fuse {
    hu_allocator_t *alloc;
    hu_fuse_adapter_t adapters[HU_PERSONA_FUSE_MAX_ADAPTERS];
    size_t adapter_count;
} hu_persona_fuse_t;

typedef struct hu_fuse_result {
    float formality;
    float verbosity;
    float emoji_factor;
    float warmth_offset;
    const char *const *preferred_vocab;
    size_t preferred_vocab_count;
    const char *const *avoided_vocab;
    size_t avoided_vocab_count;
} hu_fuse_result_t;

hu_error_t hu_persona_fuse_init(hu_persona_fuse_t *fuse, hu_allocator_t *alloc);
void hu_persona_fuse_deinit(hu_persona_fuse_t *fuse);

/* Register a named adapter. */
hu_error_t hu_persona_fuse_add_adapter(hu_persona_fuse_t *fuse, const char *name, size_t name_len,
                                       float formality, float verbosity, float emoji_factor,
                                       float warmth_offset);

/* Compose adapters by name into a fused result. adapter_names is an array of adapter name strings.
 */
hu_error_t hu_persona_fuse_compose(const hu_persona_fuse_t *fuse, const char *const *adapter_names,
                                   size_t adapter_count, hu_fuse_result_t *out);

/* Get a single adapter by name. Returns NULL if not found. */
const hu_fuse_adapter_t *hu_persona_fuse_get(const hu_persona_fuse_t *fuse, const char *name,
                                             size_t name_len);

/* Built-in adapter presets. */
hu_error_t hu_persona_fuse_add_builtin_adapters(hu_persona_fuse_t *fuse);

typedef hu_fuse_adapter_t hu_persona_fuse_adapter_t;

/** Geometric style vector in [-1,1] per dimension (PersonaFuse + prompt calibration). */
typedef struct hu_persona_vector {
    float formality;
    float warmth;
    float verbosity;
    float humor;
    float directness;
    float emoji_usage;
} hu_persona_vector_t;

hu_error_t hu_persona_vector_from_adapter(const hu_persona_fuse_adapter_t *adapter,
                                          hu_persona_vector_t *out);

hu_error_t hu_persona_vector_compose(const hu_persona_vector_t *vectors, size_t count,
                                     hu_persona_vector_t *out);

hu_error_t hu_persona_vector_to_directive(const hu_persona_vector_t *vec, char *buf,
                                          size_t buf_size, size_t *out_len);

#endif /* HU_PERSONA_FUSE_H */
