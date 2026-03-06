#define SC_IS_TEST 1

#include "seaclaw/agent/action_preview.h"
#include "seaclaw/agent/undo.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/string.h"
#include "seaclaw/security/audit.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

static sc_allocator_t sys;

static void test_action_preview_shell_shows_command(void) {
    sc_action_preview_t p;
    memset(&p, 0, sizeof(p));
    const char *args = "{\"command\":\"ls -la /tmp\"}";
    sc_error_t err = sc_action_preview_generate(&sys, "shell", args, strlen(args), &p);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT_NOT_NULL(p.description);
    SC_ASSERT(strstr(p.description, "ls -la") != NULL);
    SC_ASSERT(strstr(p.description, "Run:") != NULL);
    sc_action_preview_free(&sys, &p);
}

static void test_action_preview_file_write_shows_path(void) {
    sc_action_preview_t p;
    memset(&p, 0, sizeof(p));
    const char *args = "{\"path\":\"/home/user/notes.txt\"}";
    sc_error_t err = sc_action_preview_generate(&sys, "file_write", args, strlen(args), &p);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT_NOT_NULL(p.description);
    SC_ASSERT(strstr(p.description, "/home/user/notes.txt") != NULL);
    SC_ASSERT(strstr(p.description, "Write") != NULL);
    sc_action_preview_free(&sys, &p);
}

static void test_action_preview_format_produces_readable_string(void) {
    sc_action_preview_t p;
    memset(&p, 0, sizeof(p));
    const char *args = "{\"command\":\"echo hello\"}";
    sc_error_t err = sc_action_preview_generate(&sys, "shell", args, strlen(args), &p);
    SC_ASSERT(err == SC_OK);
    char *formatted = NULL;
    size_t len = 0;
    err = sc_action_preview_format(&sys, &p, &formatted, &len);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT_NOT_NULL(formatted);
    SC_ASSERT(strstr(formatted, "[high]") != NULL);
    SC_ASSERT(strstr(formatted, "shell") != NULL);
    SC_ASSERT(strstr(formatted, "Run:") != NULL);
    sys.free(sys.ctx, formatted, strlen(formatted) + 1);
    sc_action_preview_free(&sys, &p);
}

static void test_undo_stack_push_pop_works(void) {
    sc_undo_stack_t *stack = sc_undo_stack_create(&sys, 10);
    SC_ASSERT_NOT_NULL(stack);
    SC_ASSERT_EQ(sc_undo_stack_count(stack), 0);

    sc_undo_entry_t e = {
        .type = SC_UNDO_FILE_WRITE,
        .description = sc_strdup(&sys, "test"),
        .path = sc_strdup(&sys, "/tmp/x"),
        .original_content = sc_strdup(&sys, "content"),
        .original_content_len = 7,
        .timestamp = 0,
        .reversible = true,
    };
    sc_error_t err = sc_undo_stack_push(stack, &e);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT_EQ(sc_undo_stack_count(stack), 1);

    sys.free(sys.ctx, e.description, strlen(e.description) + 1);
    sys.free(sys.ctx, e.path, strlen(e.path) + 1);
    sys.free(sys.ctx, e.original_content, e.original_content_len + 1);

    err = sc_undo_stack_execute_undo(stack, &sys);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT_EQ(sc_undo_stack_count(stack), 0);

    sc_undo_stack_destroy(stack);
}

static void test_undo_stack_capacity_ring_buffer(void) {
    sc_undo_stack_t *stack = sc_undo_stack_create(&sys, 3);
    SC_ASSERT_NOT_NULL(stack);

    for (int i = 0; i < 5; i++) {
        char desc[32], path[32];
        snprintf(desc, sizeof(desc), "desc%d", i);
        snprintf(path, sizeof(path), "/tmp/f%d", i);
        sc_undo_entry_t e = {
            .type = SC_UNDO_FILE_WRITE,
            .description = sc_strdup(&sys, desc),
            .path = sc_strdup(&sys, path),
            .original_content = NULL,
            .original_content_len = 0,
            .timestamp = 0,
            .reversible = true,
        };
        sc_error_t err = sc_undo_stack_push(stack, &e);
        SC_ASSERT(err == SC_OK);
        sys.free(sys.ctx, e.description, strlen(e.description) + 1);
        sys.free(sys.ctx, e.path, strlen(e.path) + 1);
    }
    SC_ASSERT_EQ(sc_undo_stack_count(stack), 3);

    sc_undo_stack_destroy(stack);
}

static void test_undo_execute_pops_in_test_mode(void) {
#if defined(SC_IS_TEST) && SC_IS_TEST
    sc_undo_stack_t *stack = sc_undo_stack_create(&sys, 5);
    SC_ASSERT_NOT_NULL(stack);

    sc_undo_entry_t e = {
        .type = SC_UNDO_FILE_WRITE,
        .description = sc_strdup(&sys, "test"),
        .path = sc_strdup(&sys, "/tmp/nonexistent"),
        .original_content = sc_strdup(&sys, "data"),
        .original_content_len = 4,
        .timestamp = 0,
        .reversible = true,
    };
    sc_undo_stack_push(stack, &e);
    sys.free(sys.ctx, e.description, strlen(e.description) + 1);
    sys.free(sys.ctx, e.path, strlen(e.path) + 1);
    sys.free(sys.ctx, e.original_content, e.original_content_len + 1);

    SC_ASSERT_EQ(sc_undo_stack_count(stack), 1);
    sc_error_t err = sc_undo_stack_execute_undo(stack, &sys);
    SC_ASSERT(err == SC_OK);
    SC_ASSERT_EQ(sc_undo_stack_count(stack), 0);

    sc_undo_stack_destroy(stack);
#endif
}

static void test_audit_logger_records_tool_execution(void) {
#if defined(__unix__) || defined(__APPLE__)
    char tmp[] = "/tmp/sc_audit_pipe_XXXXXX";
    char *dir = mkdtemp(tmp);
    SC_ASSERT_NOT_NULL(dir);

    sc_audit_config_t cfg = {
        .enabled = true,
        .log_path = "audit.log",
        .max_size_mb = 10,
    };
    sc_audit_logger_t *log = sc_audit_logger_create(&sys, &cfg, dir);
    SC_ASSERT_NOT_NULL(log);

    sc_audit_event_t ev;
    sc_audit_event_init(&ev, SC_AUDIT_COMMAND_EXECUTION);
    sc_audit_event_with_action(&ev, "shell", "tool", true, true);
    sc_audit_event_with_result(&ev, true, 0, 0, NULL);

    sc_error_t err = sc_audit_logger_log(log, &ev);
    SC_ASSERT(err == SC_OK);

    sc_audit_logger_destroy(log, &sys);
    rmdir(dir);
#endif
}

void run_security_pipeline_tests(void) {
    sys = sc_system_allocator();

    SC_TEST_SUITE("security_pipeline");

    SC_RUN_TEST(test_action_preview_shell_shows_command);
    SC_RUN_TEST(test_action_preview_file_write_shows_path);
    SC_RUN_TEST(test_action_preview_format_produces_readable_string);
    SC_RUN_TEST(test_undo_stack_push_pop_works);
    SC_RUN_TEST(test_undo_stack_capacity_ring_buffer);
    SC_RUN_TEST(test_undo_execute_pops_in_test_mode);
    SC_RUN_TEST(test_audit_logger_records_tool_execution);
}
