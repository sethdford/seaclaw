#ifndef HU_CHANNELS_RATE_LIMIT_H
#define HU_CHANNELS_RATE_LIMIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct hu_channel_rate_limiter {
    int64_t tokens;         /* current tokens */
    int64_t max_tokens;     /* bucket capacity */
    int64_t refill_per_sec; /* tokens added per second */
    int64_t last_refill_ms; /* last refill timestamp */
} hu_channel_rate_limiter_t;

void hu_channel_rate_limiter_init(hu_channel_rate_limiter_t *lim, int64_t max_tokens,
                                  int64_t refill_per_sec);
bool hu_channel_rate_limiter_try_consume(hu_channel_rate_limiter_t *lim, int64_t tokens);
void hu_channel_rate_limiter_reset(hu_channel_rate_limiter_t *lim);
int64_t hu_channel_rate_limiter_wait_ms(const hu_channel_rate_limiter_t *lim, int64_t tokens);

/* Platform-oriented defaults by channel name (case-insensitive). */
hu_channel_rate_limiter_t hu_channel_rate_limit_default(const char *channel_name);

#endif
