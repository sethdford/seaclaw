/* QMD retrieval: header and types compile; in SC_IS_TEST all logic is compiled out.
 * Functional tests not needed since code is SC_IS_TEST-guarded. */
#include "seaclaw/core/allocator.h"
#include "seaclaw/memory/retrieval/qmd.h"
#include "test_framework.h"
#include <string.h>

static void test_qmd_header_compiles(void) {
    /* Verify header exists and sc_qmd_keyword_candidates is callable. */
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_entry_t *out = NULL;
    size_t count = 99;
    sc_error_t err = sc_qmd_keyword_candidates(&alloc, "/tmp", 4, "query", 5, 10, &out, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);
}

static void test_qmd_keyword_candidates_null_alloc_returns_error(void) {
    sc_memory_entry_t *out = NULL;
    size_t count = 0;
    sc_error_t err = sc_qmd_keyword_candidates(NULL, "/tmp", 4, "query", 5, 10, &out, &count);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_NULL(out);
    SC_ASSERT_EQ(count, 0u);
}

static void test_qmd_keyword_candidates_null_query_returns_ok_in_test(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_entry_t *out = NULL;
    size_t count = 99;
    sc_error_t err = sc_qmd_keyword_candidates(&alloc, "/tmp", 4, NULL, 0, 10, &out, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);
}

static void test_qmd_keyword_candidates_null_out_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    size_t count = 0;
    sc_error_t err = sc_qmd_keyword_candidates(&alloc, "/tmp", 4, "query", 5, 10, NULL, &count);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_qmd_keyword_candidates_null_out_count_returns_error(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_entry_t *out = NULL;
    sc_error_t err = sc_qmd_keyword_candidates(&alloc, "/tmp", 4, "query", 5, 10, &out, NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_qmd_keyword_candidates_zero_limit_returns_ok_in_test(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_entry_t *out = NULL;
    size_t count = 99;
    sc_error_t err = sc_qmd_keyword_candidates(&alloc, "/tmp", 4, "query", 5, 0, &out, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);
}

static void test_qmd_keyword_candidates_null_workspace_dir_returns_ok_in_test(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_memory_entry_t *out = NULL;
    size_t count = 99;
    sc_error_t err = sc_qmd_keyword_candidates(&alloc, NULL, 0, "query", 5, 10, &out, &count);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(count, 0u);
}

void run_qmd_tests(void) {
    SC_TEST_SUITE("QMD");
    SC_RUN_TEST(test_qmd_header_compiles);
    SC_RUN_TEST(test_qmd_keyword_candidates_null_alloc_returns_error);
    SC_RUN_TEST(test_qmd_keyword_candidates_null_query_returns_ok_in_test);
    SC_RUN_TEST(test_qmd_keyword_candidates_null_out_returns_error);
    SC_RUN_TEST(test_qmd_keyword_candidates_null_out_count_returns_error);
    SC_RUN_TEST(test_qmd_keyword_candidates_zero_limit_returns_ok_in_test);
    SC_RUN_TEST(test_qmd_keyword_candidates_null_workspace_dir_returns_ok_in_test);
}
