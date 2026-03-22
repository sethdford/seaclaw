#include "human/channels/rate_limit.h"
#include <stdint.h>

#if defined(__APPLE__) || defined(__unix__)
#include <strings.h>
#include <time.h>
#endif

static int64_t rate_limit_now_ms(void) {
#if defined(__APPLE__) || defined(__unix__)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
#endif
    return 0;
}

void hu_channel_rate_limiter_init(hu_channel_rate_limiter_t *lim, int64_t max_tokens,
                                  int64_t refill_per_sec) {
    if (!lim)
        return;
    lim->max_tokens = max_tokens > 0 ? max_tokens : 1;
    lim->refill_per_sec = refill_per_sec >= 0 ? refill_per_sec : 0;
    lim->tokens = lim->max_tokens;
    lim->last_refill_ms = rate_limit_now_ms();
}

void hu_channel_rate_limiter_reset(hu_channel_rate_limiter_t *lim) {
    if (!lim)
        return;
    lim->tokens = lim->max_tokens;
    lim->last_refill_ms = rate_limit_now_ms();
}

bool hu_channel_rate_limiter_try_consume(hu_channel_rate_limiter_t *lim, int64_t tokens) {
    if (!lim || tokens <= 0)
        return true;

    int64_t now = rate_limit_now_ms();
    int64_t delta = now - lim->last_refill_ms;
    if (delta < 0)
        delta = 0;

    if (lim->refill_per_sec > 0) {
        int64_t add = delta * lim->refill_per_sec / 1000;
        lim->tokens += add;
        if (lim->tokens > lim->max_tokens)
            lim->tokens = lim->max_tokens;
    }
    lim->last_refill_ms = now;

    if (lim->tokens < tokens)
        return false;
    lim->tokens -= tokens;
    return true;
}

int64_t hu_channel_rate_limiter_wait_ms(const hu_channel_rate_limiter_t *lim, int64_t tokens_needed) {
    if (!lim || tokens_needed <= 0)
        return 0;

    int64_t now = rate_limit_now_ms();
    int64_t delta = now - lim->last_refill_ms;
    if (delta < 0)
        delta = 0;

    int64_t tok = lim->tokens;
    if (lim->refill_per_sec > 0) {
        tok += delta * lim->refill_per_sec / 1000;
        if (tok > lim->max_tokens)
            tok = lim->max_tokens;
    }

    if (tok >= tokens_needed)
        return 0;
    if (lim->refill_per_sec <= 0)
        return INT64_MAX / 4;

    int64_t deficit = tokens_needed - tok;
    return (deficit * 1000 + lim->refill_per_sec - 1) / lim->refill_per_sec;
}

hu_channel_rate_limiter_t hu_channel_rate_limit_default(const char *channel_name) {
    hu_channel_rate_limiter_t lim;
    const char *n = channel_name ? channel_name : "";

#if defined(__APPLE__) || defined(__unix__)
    if (strcasecmp(n, "slack") == 0)
        hu_channel_rate_limiter_init(&lim, 1, 1);
    else if (strcasecmp(n, "discord") == 0)
        hu_channel_rate_limiter_init(&lim, 5, 1);
    else if (strcasecmp(n, "telegram") == 0)
        hu_channel_rate_limiter_init(&lim, 30, 30);
    else if (strcasecmp(n, "whatsapp") == 0)
        hu_channel_rate_limiter_init(&lim, 80, 80);
    else if (strcasecmp(n, "signal") == 0)
        hu_channel_rate_limiter_init(&lim, 5, 5);
    else if (strcasecmp(n, "teams") == 0)
        hu_channel_rate_limiter_init(&lim, 2, 2);
    else if (strcasecmp(n, "twilio") == 0 || strcasecmp(n, "sms") == 0)
        hu_channel_rate_limiter_init(&lim, 1, 1);
    else
        hu_channel_rate_limiter_init(&lim, 10, 10);
#else
    (void)n;
    hu_channel_rate_limiter_init(&lim, 10, 10);
#endif
    return lim;
}
