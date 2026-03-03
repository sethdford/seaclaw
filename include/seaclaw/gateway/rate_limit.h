#ifndef SC_GATEWAY_RATE_LIMIT_H
#define SC_GATEWAY_RATE_LIMIT_H

#include "seaclaw/core/allocator.h"
#include <stdbool.h>

typedef struct sc_rate_limiter sc_rate_limiter_t;

sc_rate_limiter_t *sc_rate_limiter_create(sc_allocator_t *alloc, int requests_per_window,
                                          int window_secs);
bool sc_rate_limiter_allow(sc_rate_limiter_t *lim, const char *ip);
void sc_rate_limiter_destroy(sc_rate_limiter_t *lim);

#endif /* SC_GATEWAY_RATE_LIMIT_H */
