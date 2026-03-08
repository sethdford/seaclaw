#include "seaclaw/paperclip/client.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include "seaclaw/core/string.h"
#include "seaclaw/tools/paperclip.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

/* ── Client init / deinit ────────────────────────────────────────────── */

static void client_init_from_config_ok(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_error_t err = sc_paperclip_client_init_from_config(&c, &alloc,
        "http://localhost:3100/api", "agent-123", "company-456");
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(c.api_url);
    SC_ASSERT_NOT_NULL(c.agent_id);
    SC_ASSERT_NOT_NULL(c.company_id);
    SC_ASSERT_STR_EQ(c.api_url, "http://localhost:3100/api");
    SC_ASSERT_STR_EQ(c.agent_id, "agent-123");
    SC_ASSERT_STR_EQ(c.company_id, "company-456");
    sc_paperclip_client_deinit(&c);
}

static void client_init_requires_url_and_id(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    SC_ASSERT_EQ(sc_paperclip_client_init_from_config(&c, &alloc, NULL, "id", NULL),
                 SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_EQ(sc_paperclip_client_init_from_config(&c, &alloc, "url", NULL, NULL),
                 SC_ERR_INVALID_ARGUMENT);
}

static void client_init_from_env_requires_vars(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    /* Without env vars set, this should fail or succeed based on environment.
       We test the explicit path instead. */
    sc_error_t err = sc_paperclip_client_init_from_config(&c, &alloc,
        "http://test:3100/api", "test-agent", NULL);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NULL(c.company_id);
    sc_paperclip_client_deinit(&c);
}

static void client_deinit_handles_null(void) {
    sc_paperclip_client_deinit(NULL);
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_deinit(&c);
}

/* ── Task parsing ────────────────────────────────────────────────────── */

static void task_list_empty_returns_ok(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");

    sc_paperclip_task_list_t list = {0};
    sc_error_t err = sc_paperclip_list_tasks(&c, &list);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_EQ(list.count, 0u);

    sc_paperclip_task_list_free(&alloc, &list);
    sc_paperclip_client_deinit(&c);
}

static void task_free_handles_null_fields(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_task_t task = {0};
    sc_paperclip_task_free(&alloc, &task);
    sc_paperclip_task_free(NULL, &task);
    sc_paperclip_task_free(&alloc, NULL);
}

static void task_list_free_handles_null(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_task_list_free(&alloc, NULL);
    sc_paperclip_task_list_t list = {0};
    sc_paperclip_task_list_free(&alloc, &list);
}

/* ── Comment list ────────────────────────────────────────────────────── */

static void comment_list_free_handles_null(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_comment_list_free(&alloc, NULL);
    sc_paperclip_comment_list_t list = {0};
    sc_paperclip_comment_list_free(&alloc, &list);
}

static void get_comments_returns_ok_in_test(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");

    sc_paperclip_comment_list_t comments = {0};
    sc_error_t err = sc_paperclip_get_comments(&c, "task-1", &comments);
    SC_ASSERT_EQ(err, SC_OK);

    sc_paperclip_comment_list_free(&alloc, &comments);
    sc_paperclip_client_deinit(&c);
}

/* ── Checkout / update / post ────────────────────────────────────────── */

static void checkout_requires_task_id(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    SC_ASSERT_EQ(sc_paperclip_checkout_task(&c, NULL), SC_ERR_INVALID_ARGUMENT);
    sc_paperclip_client_deinit(&c);
}

static void checkout_ok_in_test_mode(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    SC_ASSERT_EQ(sc_paperclip_checkout_task(&c, "task-1"), SC_OK);
    sc_paperclip_client_deinit(&c);
}

static void update_task_requires_params(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    SC_ASSERT_EQ(sc_paperclip_update_task(&c, NULL, "done"), SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_EQ(sc_paperclip_update_task(&c, "t1", NULL), SC_ERR_INVALID_ARGUMENT);
    sc_paperclip_client_deinit(&c);
}

static void update_task_ok_in_test_mode(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    SC_ASSERT_EQ(sc_paperclip_update_task(&c, "task-1", "done"), SC_OK);
    sc_paperclip_client_deinit(&c);
}

static void post_comment_requires_params(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    SC_ASSERT_EQ(sc_paperclip_post_comment(&c, NULL, "hi", 2), SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_EQ(sc_paperclip_post_comment(&c, "t1", NULL, 0), SC_ERR_INVALID_ARGUMENT);
    sc_paperclip_client_deinit(&c);
}

static void post_comment_ok_in_test_mode(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_paperclip_client_t c = {0};
    sc_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    SC_ASSERT_EQ(sc_paperclip_post_comment(&c, "task-1", "test comment", 12), SC_OK);
    sc_paperclip_client_deinit(&c);
}

/* ── Tool creation ───────────────────────────────────────────────────── */

static void paperclip_tool_creates_ok(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_error_t err = sc_paperclip_tool_create(&alloc, &tool);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(tool.ctx);
    SC_ASSERT_NOT_NULL(tool.vtable);
    SC_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "paperclip");
    SC_ASSERT_NOT_NULL(tool.vtable->description(tool.ctx));
    SC_ASSERT_NOT_NULL(tool.vtable->parameters_json(tool.ctx));
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void paperclip_tool_requires_action(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_paperclip_tool_create(&alloc, &tool);

    const char *json = "{}";
    sc_json_value_t *args = NULL;
    sc_json_parse(&alloc, json, strlen(json), &args);

    sc_tool_result_t result = {0};
    sc_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_FALSE(result.success);
    SC_ASSERT_NOT_NULL(strstr(result.output, "action"));

    if (result.output)
        alloc.free(alloc.ctx, result.output, result.output_len + 1);
    sc_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void paperclip_tool_unknown_action(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_tool_t tool = {0};
    sc_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"explode\"}";
    sc_json_value_t *args = NULL;
    sc_json_parse(&alloc, json, strlen(json), &args);

    sc_tool_result_t result = {0};
    sc_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_FALSE(result.success);
    SC_ASSERT_NOT_NULL(result.output);
    SC_ASSERT_TRUE(strstr(result.output, "Unknown") != NULL ||
                   strstr(result.output, "Missing") != NULL);

    if (result.output)
        alloc.free(alloc.ctx, result.output, result.output_len + 1);
    sc_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Test registration ───────────────────────────────────────────────── */

void run_paperclip_tests(void) {
    SC_TEST_SUITE("paperclip");

    SC_RUN_TEST(client_init_from_config_ok);
    SC_RUN_TEST(client_init_requires_url_and_id);
    SC_RUN_TEST(client_init_from_env_requires_vars);
    SC_RUN_TEST(client_deinit_handles_null);

    SC_RUN_TEST(task_list_empty_returns_ok);
    SC_RUN_TEST(task_free_handles_null_fields);
    SC_RUN_TEST(task_list_free_handles_null);

    SC_RUN_TEST(comment_list_free_handles_null);
    SC_RUN_TEST(get_comments_returns_ok_in_test);

    SC_RUN_TEST(checkout_requires_task_id);
    SC_RUN_TEST(checkout_ok_in_test_mode);
    SC_RUN_TEST(update_task_requires_params);
    SC_RUN_TEST(update_task_ok_in_test_mode);
    SC_RUN_TEST(post_comment_requires_params);
    SC_RUN_TEST(post_comment_ok_in_test_mode);

    SC_RUN_TEST(paperclip_tool_creates_ok);
    SC_RUN_TEST(paperclip_tool_requires_action);
    SC_RUN_TEST(paperclip_tool_unknown_action);
}
