#include "human/daemon.h"
#include "human/daemon_lifecycle.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

/* ── hu_daemon_validate_home ─────────────────────────────────────────── */

static void test_validate_home_normal_path(void) {
    HU_ASSERT_EQ(hu_daemon_validate_home("/Users/alice"), HU_OK);
    HU_ASSERT_EQ(hu_daemon_validate_home("/home/bob"), HU_OK);
}

static void test_validate_home_with_spaces(void) {
    HU_ASSERT_EQ(hu_daemon_validate_home("/Users/Bob Smith"), HU_OK);
}

static void test_validate_home_with_dots_underscores(void) {
    HU_ASSERT_EQ(hu_daemon_validate_home("/home/.local_user-1"), HU_OK);
}

static void test_validate_home_null(void) {
    HU_ASSERT_EQ(hu_daemon_validate_home(NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_validate_home_empty(void) {
    HU_ASSERT_EQ(hu_daemon_validate_home(""), HU_ERR_INVALID_ARGUMENT);
}

static void test_validate_home_unsafe_chars(void) {
    HU_ASSERT_EQ(hu_daemon_validate_home("/home/user;rm -rf /"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_daemon_validate_home("/home/$USER"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_daemon_validate_home("/home/user`cmd`"), HU_ERR_INVALID_ARGUMENT);
}

/* ── hu_daemon_get_pid_path ──────────────────────────────────────────── */

static void test_get_pid_path_returns_valid(void) {
    char buf[1024];
    int n = hu_daemon_get_pid_path(buf, sizeof(buf));
    /* Should return a positive length */
    HU_ASSERT(n > 0);
    /* Path should end with human.pid */
    HU_ASSERT_NOT_NULL(strstr(buf, "human.pid"));
}

static void test_get_pid_path_small_buffer(void) {
    char buf[4];
    int n = hu_daemon_get_pid_path(buf, sizeof(buf));
    /* snprintf returns needed length, which exceeds buf_size */
    HU_ASSERT(n > 0);
    HU_ASSERT((size_t)n >= sizeof(buf));
}

/* ── hu_daemon_start/stop/status (HU_IS_TEST stubs) ─────────────────── */

static void test_daemon_start_test_mode(void) {
    hu_error_t err = hu_daemon_start();
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_daemon_stop_test_mode(void) {
    hu_error_t err = hu_daemon_stop();
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_daemon_status_test_mode(void) {
    HU_ASSERT_FALSE(hu_daemon_status());
}

/* ── hu_daemon_install/uninstall/logs (HU_IS_TEST stubs) ─────────────── */

static void test_daemon_install_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_daemon_install(&alloc);
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_daemon_uninstall_test_mode(void) {
    hu_error_t err = hu_daemon_uninstall();
    HU_ASSERT_EQ(err, HU_OK);
}

static void test_daemon_logs_test_mode(void) {
    hu_error_t err = hu_daemon_logs();
    HU_ASSERT_EQ(err, HU_OK);
}

/* ── PID file (HU_IS_TEST guards skip mkdir) ─────────────────────────── */

static void test_daemon_remove_pid_no_crash(void) {
    /* Should not crash even if PID file doesn't exist */
    hu_daemon_remove_pid();
}

void run_daemon_lifecycle_tests(void) {
    HU_TEST_SUITE("daemon_lifecycle");

    /* validate_home */
    HU_RUN_TEST(test_validate_home_normal_path);
    HU_RUN_TEST(test_validate_home_with_spaces);
    HU_RUN_TEST(test_validate_home_with_dots_underscores);
    HU_RUN_TEST(test_validate_home_null);
    HU_RUN_TEST(test_validate_home_empty);
    HU_RUN_TEST(test_validate_home_unsafe_chars);

    /* get_pid_path */
    HU_RUN_TEST(test_get_pid_path_returns_valid);
    HU_RUN_TEST(test_get_pid_path_small_buffer);

    /* daemon management stubs */
    HU_RUN_TEST(test_daemon_start_test_mode);
    HU_RUN_TEST(test_daemon_stop_test_mode);
    HU_RUN_TEST(test_daemon_status_test_mode);

    /* install stubs */
    HU_RUN_TEST(test_daemon_install_test_mode);
    HU_RUN_TEST(test_daemon_uninstall_test_mode);
    HU_RUN_TEST(test_daemon_logs_test_mode);

    /* PID file */
    HU_RUN_TEST(test_daemon_remove_pid_no_crash);
}
