#include "human/channel_monitor.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_tests_run = 0;
static int s_tests_passed = 0;

#define RUN(fn)           \
    do {                  \
        s_tests_run++;    \
        fn();             \
        s_tests_passed++; \
    } while (0)

/* ── mock channel ── */

static bool s_mock_healthy = true;
static int s_mock_start_calls = 0;
static int s_mock_stop_calls = 0;

static hu_error_t mock_start(void *ctx) {
    (void)ctx;
    s_mock_start_calls++;
    return HU_OK;
}
static void mock_stop(void *ctx) {
    (void)ctx;
    s_mock_stop_calls++;
}
static hu_error_t mock_send(void *ctx, const char *t, size_t tl, const char *m, size_t ml,
                            const char *const *media, size_t mc) {
    (void)ctx;
    (void)t;
    (void)tl;
    (void)m;
    (void)ml;
    (void)media;
    (void)mc;
    return HU_OK;
}
static const char *mock_name(void *ctx) {
    (void)ctx;
    return "mock-channel";
}
static bool mock_health(void *ctx) {
    (void)ctx;
    return s_mock_healthy;
}

static hu_channel_vtable_t s_mock_vtable = {
    .start = mock_start,
    .stop = mock_stop,
    .send = mock_send,
    .name = mock_name,
    .health_check = mock_health,
};

static hu_channel_t s_mock_channel = {
    .ctx = NULL,
    .vtable = &s_mock_vtable,
};

/* ── allocator ── */

static void *test_alloc(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}
static void test_free(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}
static hu_allocator_t s_alloc = {.alloc = test_alloc, .free = test_free};

/* ── tests ── */

static void test_create_destroy(void) {
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    assert(hu_channel_monitor_create(&s_alloc, &cfg, &mon) == HU_OK);
    assert(mon != NULL);
    hu_channel_monitor_destroy(mon);
}

static void test_create_null_args(void) {
    hu_channel_monitor_t *mon = NULL;
    assert(hu_channel_monitor_create(NULL, NULL, &mon) == HU_ERR_INVALID_ARGUMENT);
    assert(hu_channel_monitor_create(&s_alloc, NULL, NULL) == HU_ERR_INVALID_ARGUMENT);
}

static void test_add_channel(void) {
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    assert(hu_channel_monitor_add(mon, &s_mock_channel) == HU_OK);

    const hu_channel_status_t *status = NULL;
    size_t count = 0;
    hu_channel_monitor_get_status(mon, &status, &count);
    assert(count == 1);
    assert(strcmp(status[0].channel_name, "mock-channel") == 0);
    assert(status[0].healthy == true);
    hu_channel_monitor_destroy(mon);
}

static void test_tick_healthy(void) {
    s_mock_healthy = true;
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    cfg.check_interval_sec = 0;
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    hu_channel_monitor_add(mon, &s_mock_channel);

    assert(hu_channel_monitor_tick(mon, 100) == HU_OK);

    const hu_channel_status_t *status = NULL;
    size_t count = 0;
    hu_channel_monitor_get_status(mon, &status, &count);
    assert(count == 1);
    assert(status[0].healthy == true);
    assert(status[0].last_healthy_ts == 100);
    assert(status[0].consecutive_failures == 0);
    hu_channel_monitor_destroy(mon);
}

static void test_tick_unhealthy_tracks_failures(void) {
    s_mock_healthy = false;
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    cfg.check_interval_sec = 0;
    cfg.max_restart_count = 3;
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    hu_channel_monitor_add(mon, &s_mock_channel);

    hu_channel_monitor_tick(mon, 100);

    const hu_channel_status_t *status = NULL;
    size_t count = 0;
    hu_channel_monitor_get_status(mon, &status, &count);
    assert(count == 1);
    assert(status[0].healthy == false);
    assert(status[0].consecutive_failures == 1);
    assert(status[0].restart_count == 1);
    assert(strlen(status[0].last_error) > 0);
    hu_channel_monitor_destroy(mon);
}

static void test_backoff_doubles(void) {
    s_mock_healthy = false;
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    cfg.check_interval_sec = 0;
    cfg.backoff_initial_sec = 2;
    cfg.backoff_max_sec = 16;
    cfg.max_restart_count = 10;
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    hu_channel_monitor_add(mon, &s_mock_channel);

    hu_channel_monitor_tick(mon, 100);
    const hu_channel_status_t *st = NULL;
    size_t cnt = 0;
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].current_backoff_sec == 4);

    hu_channel_monitor_tick(mon, 200);
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].current_backoff_sec == 8);

    hu_channel_monitor_tick(mon, 400);
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].current_backoff_sec == 16);

    /* Cap at max */
    hu_channel_monitor_tick(mon, 600);
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].current_backoff_sec == 16);

    hu_channel_monitor_destroy(mon);
}

static void test_record_event(void) {
    s_mock_healthy = true;
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    cfg.check_interval_sec = 0;
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    hu_channel_monitor_add(mon, &s_mock_channel);

    hu_channel_monitor_tick(mon, 100);
    hu_channel_monitor_record_event(mon, "mock-channel");

    const hu_channel_status_t *st = NULL;
    size_t cnt = 0;
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].last_event_ts == 100);
    hu_channel_monitor_destroy(mon);
}

static void test_stale_event_warning(void) {
    s_mock_healthy = true;
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    cfg.check_interval_sec = 0;
    cfg.stale_event_threshold = 60;
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    hu_channel_monitor_add(mon, &s_mock_channel);

    hu_channel_monitor_tick(mon, 100);
    hu_channel_monitor_record_event(mon, "mock-channel");

    /* Tick way after stale threshold */
    hu_channel_monitor_tick(mon, 200);
    const hu_channel_status_t *st = NULL;
    size_t cnt = 0;
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].healthy == true);
    assert(strstr(st[0].last_error, "no events") != NULL);
    hu_channel_monitor_destroy(mon);
}

static void test_recovery_resets_state(void) {
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    cfg.check_interval_sec = 0;
    cfg.backoff_initial_sec = 2;
    cfg.max_restart_count = 5;
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    hu_channel_monitor_add(mon, &s_mock_channel);

    s_mock_healthy = false;
    hu_channel_monitor_tick(mon, 100);

    const hu_channel_status_t *st = NULL;
    size_t cnt = 0;
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].consecutive_failures == 1);
    assert(st[0].current_backoff_sec == 4);

    s_mock_healthy = true;
    hu_channel_monitor_tick(mon, 200);
    hu_channel_monitor_get_status(mon, &st, &cnt);
    assert(st[0].healthy == true);
    assert(st[0].consecutive_failures == 0);
    assert(st[0].current_backoff_sec == 2);
    hu_channel_monitor_destroy(mon);
}

static void test_default_config_values(void) {
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    assert(cfg.check_interval_sec == 30);
    assert(cfg.max_restart_count == 5);
    assert(cfg.backoff_initial_sec == 2);
    assert(cfg.backoff_max_sec == 120);
    assert(cfg.stale_event_threshold == 300);
}

static void test_add_null_args(void) {
    hu_channel_monitor_t *mon = NULL;
    hu_channel_monitor_config_t cfg = hu_channel_monitor_config_default();
    hu_channel_monitor_create(&s_alloc, &cfg, &mon);
    assert(hu_channel_monitor_add(NULL, &s_mock_channel) == HU_ERR_INVALID_ARGUMENT);
    assert(hu_channel_monitor_add(mon, NULL) == HU_ERR_INVALID_ARGUMENT);
    hu_channel_monitor_destroy(mon);
}

static void test_tick_null(void) {
    assert(hu_channel_monitor_tick(NULL, 100) == HU_ERR_INVALID_ARGUMENT);
}

int run_channel_monitor_tests(void) {
    s_tests_run = 0;
    s_tests_passed = 0;

    RUN(test_create_destroy);
    RUN(test_create_null_args);
    RUN(test_add_channel);
    RUN(test_tick_healthy);
    RUN(test_tick_unhealthy_tracks_failures);
    RUN(test_backoff_doubles);
    RUN(test_record_event);
    RUN(test_stale_event_warning);
    RUN(test_recovery_resets_state);
    RUN(test_default_config_values);
    RUN(test_add_null_args);
    RUN(test_tick_null);

    printf("  channel_monitor: %d/%d passed\n", s_tests_passed, s_tests_run);
    return s_tests_run - s_tests_passed;
}
