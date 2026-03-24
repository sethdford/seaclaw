#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/plugin_discovery.h"
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

static void test_get_default_dir(void) {
    char buf[512];
    size_t n = hu_plugin_get_default_dir(buf, sizeof(buf));
    /* HOME is set in most environments */
    if (n > 0) {
        assert(strstr(buf, ".human/plugins") != NULL);
    }
}

static void test_get_default_dir_small_buffer(void) {
    char buf[4];
    size_t n = hu_plugin_get_default_dir(buf, sizeof(buf));
    assert(n == 0);
}

static void test_discover_null_args(void) {
    hu_plugin_discovery_result_t *res = NULL;
    size_t count = 0;
    assert(hu_plugin_discover_and_load(NULL, NULL, NULL, &res, &count) == HU_ERR_INVALID_ARGUMENT);
    assert(hu_plugin_discover_and_load(&s_alloc, NULL, NULL, NULL, &count) ==
           HU_ERR_INVALID_ARGUMENT);
    assert(hu_plugin_discover_and_load(&s_alloc, NULL, NULL, &res, NULL) ==
           HU_ERR_INVALID_ARGUMENT);
}

static void test_discover_empty_dir(void) {
    hu_plugin_discovery_result_t *res = NULL;
    size_t count = 0;
    hu_error_t err = hu_plugin_discover_and_load(&s_alloc, "", NULL, &res, &count);
    assert(err == HU_OK);
    assert(count == 0);
    assert(res == NULL);
}

static void test_discover_with_dir(void) {
    hu_plugin_discovery_result_t *res = NULL;
    size_t count = 0;
    hu_error_t err = hu_plugin_discover_and_load(&s_alloc, "/tmp/test-plugins", NULL, &res, &count);
    assert(err == HU_OK);
    assert(count == 1);
    assert(res != NULL);
    assert(strcmp(res[0].name, "mock-plugin") == 0);
    assert(strcmp(res[0].version, "1.0.0") == 0);
    assert(res[0].load_error == HU_OK);
    hu_plugin_discovery_results_free(&s_alloc, res, count);
}

static void test_results_free_null(void) {
    hu_plugin_discovery_results_free(&s_alloc, NULL, 0);
    hu_plugin_discovery_results_free(NULL, NULL, 0);
}

static void test_discover_mock_path(void) {
    hu_plugin_discovery_result_t *res = NULL;
    size_t count = 0;
    hu_error_t err = hu_plugin_discover_and_load(&s_alloc, "/fake/dir", NULL, &res, &count);
    assert(err == HU_OK);
    assert(count == 1);
    assert(res[0].path != NULL);
    assert(strcmp(res[0].path, "mock-plugin.so") == 0);
    hu_plugin_discovery_results_free(&s_alloc, res, count);
}

int run_plugin_discovery_tests(void) {
    s_tests_run = 0;
    s_tests_passed = 0;

    RUN(test_get_default_dir);
    RUN(test_get_default_dir_small_buffer);
    RUN(test_discover_null_args);
    RUN(test_discover_empty_dir);
    RUN(test_discover_with_dir);
    RUN(test_results_free_null);
    RUN(test_discover_mock_path);

    printf("  plugin_discovery: %d/%d passed\n", s_tests_passed, s_tests_run);
    return s_tests_run - s_tests_passed;
}
