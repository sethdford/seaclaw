#include "human/channels/rate_limit.h"
#include "test_framework.h"
#include <unistd.h>

static void channel_rate_limiter_init_fills_bucket(void) {
    hu_channel_rate_limiter_t lim;
    hu_channel_rate_limiter_init(&lim, 5, 10);
    HU_ASSERT_EQ(lim.max_tokens, 5);
    HU_ASSERT_EQ(lim.refill_per_sec, 10);
    HU_ASSERT_EQ(lim.tokens, 5);
}

static void channel_rate_limiter_consume_exhausts_bucket(void) {
    hu_channel_rate_limiter_t lim;
    hu_channel_rate_limiter_init(&lim, 2, 0);
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(&lim, 1));
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(&lim, 1));
    HU_ASSERT_FALSE(hu_channel_rate_limiter_try_consume(&lim, 1));
}

static void channel_rate_limiter_refills_over_time(void) {
    hu_channel_rate_limiter_t lim;
    hu_channel_rate_limiter_init(&lim, 2, 10);
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(&lim, 2));
    HU_ASSERT_FALSE(hu_channel_rate_limiter_try_consume(&lim, 1));
    usleep(150000);
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(&lim, 1));
}

static void channel_rate_limiter_wait_ms_reports_delay(void) {
    hu_channel_rate_limiter_t lim;
    hu_channel_rate_limiter_init(&lim, 1, 2);
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(&lim, 1));
    int64_t w = hu_channel_rate_limiter_wait_ms(&lim, 1);
    HU_ASSERT_TRUE(w > 0);
}

static void channel_rate_limiter_reset_restores_capacity(void) {
    hu_channel_rate_limiter_t lim;
    hu_channel_rate_limiter_init(&lim, 3, 5);
    HU_ASSERT_TRUE(hu_channel_rate_limiter_try_consume(&lim, 2));
    hu_channel_rate_limiter_reset(&lim);
    HU_ASSERT_EQ(lim.tokens, 3);
}

static void channel_rate_limit_default_slack(void) {
    hu_channel_rate_limiter_t lim = hu_channel_rate_limit_default("slack");
    HU_ASSERT_EQ(lim.max_tokens, 1);
    HU_ASSERT_EQ(lim.refill_per_sec, 1);
}

static void channel_rate_limit_default_discord(void) {
    hu_channel_rate_limiter_t lim = hu_channel_rate_limit_default("discord");
    HU_ASSERT_EQ(lim.max_tokens, 5);
    HU_ASSERT_EQ(lim.refill_per_sec, 1);
}

static void channel_rate_limit_default_telegram(void) {
    hu_channel_rate_limiter_t lim = hu_channel_rate_limit_default("telegram");
    HU_ASSERT_EQ(lim.max_tokens, 30);
    HU_ASSERT_EQ(lim.refill_per_sec, 30);
}

static void channel_rate_limit_default_twilio(void) {
    hu_channel_rate_limiter_t lim = hu_channel_rate_limit_default("twilio");
    HU_ASSERT_EQ(lim.max_tokens, 1);
    HU_ASSERT_EQ(lim.refill_per_sec, 1);
}

static void channel_rate_limit_default_unknown_is_ten_per_sec(void) {
    hu_channel_rate_limiter_t lim = hu_channel_rate_limit_default("unknown_channel");
    HU_ASSERT_EQ(lim.max_tokens, 10);
    HU_ASSERT_EQ(lim.refill_per_sec, 10);
}

void run_channel_rate_limit_tests(void) {
    HU_TEST_SUITE("channel_rate_limit");
    HU_RUN_TEST(channel_rate_limiter_init_fills_bucket);
    HU_RUN_TEST(channel_rate_limiter_consume_exhausts_bucket);
    HU_RUN_TEST(channel_rate_limiter_refills_over_time);
    HU_RUN_TEST(channel_rate_limiter_wait_ms_reports_delay);
    HU_RUN_TEST(channel_rate_limiter_reset_restores_capacity);
    HU_RUN_TEST(channel_rate_limit_default_slack);
    HU_RUN_TEST(channel_rate_limit_default_discord);
    HU_RUN_TEST(channel_rate_limit_default_telegram);
    HU_RUN_TEST(channel_rate_limit_default_twilio);
    HU_RUN_TEST(channel_rate_limit_default_unknown_is_ten_per_sec);
}
