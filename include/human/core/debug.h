#ifndef HU_CORE_DEBUG_H
#define HU_CORE_DEBUG_H

/*
 * Lightweight debug-only assertions for single-threaded modules.
 *
 * HU_ASSERT_NOT_REENTRANT(tag) / HU_LEAVE_NOT_REENTRANT(tag)
 *   Bracket a function body to detect accidental concurrent or reentrant
 *   access in debug builds. Compiles to nothing in release (NDEBUG).
 *
 * Usage:
 *   void my_function(my_struct_t *s) {
 *       HU_ASSERT_NOT_REENTRANT(my_function);
 *       // ... function body ...
 *       HU_LEAVE_NOT_REENTRANT(my_function);
 *   }
 */

#ifndef NDEBUG
#include <assert.h>
#include <stdatomic.h>

#define HU_ASSERT_NOT_REENTRANT(tag)           \
    static atomic_int _hu_reentrant_##tag = 0; \
    assert(atomic_fetch_add(&_hu_reentrant_##tag, 1) == 0 && "reentrant/concurrent access: " #tag)

#define HU_LEAVE_NOT_REENTRANT(tag) atomic_fetch_sub(&_hu_reentrant_##tag, 1)

#else /* NDEBUG */

#define HU_ASSERT_NOT_REENTRANT(tag) ((void)0)
#define HU_LEAVE_NOT_REENTRANT(tag)  ((void)0)

#endif /* NDEBUG */

#endif /* HU_CORE_DEBUG_H */
