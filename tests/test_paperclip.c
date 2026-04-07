#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/paperclip/client.h"
#include "human/paperclip/heartbeat.h"
#include "human/tools/paperclip.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

/* ── Client init / deinit ────────────────────────────────────────────── */

static void test_paperclip_client_init_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    HU_ASSERT_EQ(hu_paperclip_client_init(NULL, &alloc), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_paperclip_client_init(&c, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_paperclip_client_init_no_env(void) {
#if defined(__unix__) || defined(__APPLE__)
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    /* Unset required env vars so init fails gracefully */
    unsetenv("PAPERCLIP_API_URL");
    unsetenv("PAPERCLIP_AGENT_ID");
    hu_error_t err = hu_paperclip_client_init(&c, &alloc);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
#endif
}

static void client_init_from_config_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_error_t err = hu_paperclip_client_init_from_config(&c, &alloc, "http://localhost:3100/api",
                                                          "agent-123", "company-456");
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(c.api_url);
    HU_ASSERT_NOT_NULL(c.agent_id);
    HU_ASSERT_NOT_NULL(c.company_id);
    HU_ASSERT_STR_EQ(c.api_url, "http://localhost:3100/api");
    HU_ASSERT_STR_EQ(c.agent_id, "agent-123");
    HU_ASSERT_STR_EQ(c.company_id, "company-456");
    hu_paperclip_client_deinit(&c);
}

static void client_init_requires_url_and_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    HU_ASSERT_EQ(hu_paperclip_client_init_from_config(&c, &alloc, NULL, "id", NULL),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_paperclip_client_init_from_config(&c, &alloc, "url", NULL, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void client_init_from_env_requires_vars(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    /* Without env vars set, this should fail or succeed based on environment.
       We test the explicit path instead. */
    hu_error_t err = hu_paperclip_client_init_from_config(&c, &alloc, "http://test:3100/api",
                                                          "test-agent", NULL);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NULL(c.company_id);
    hu_paperclip_client_deinit(&c);
}

static void client_deinit_handles_null(void) {
    hu_paperclip_client_deinit(NULL);
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_deinit(&c);
}

/* ── Task parsing ────────────────────────────────────────────────────── */

static void test_paperclip_list_tasks_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    hu_paperclip_task_list_t list = {0};
    HU_ASSERT_EQ(hu_paperclip_list_tasks(NULL, &list), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_paperclip_list_tasks(&c, NULL), HU_ERR_INVALID_ARGUMENT);
    hu_paperclip_client_deinit(&c);
}

static void list_tasks_returns_mock_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");

    hu_paperclip_task_list_t list = {0};
    hu_error_t err = hu_paperclip_list_tasks(&c, &list);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(list.count, 2u);

    HU_ASSERT_STR_EQ(list.tasks[0].id, "task-001");
    HU_ASSERT_STR_EQ(list.tasks[0].title, "Fix login bug");
    HU_ASSERT_STR_EQ(list.tasks[0].description, "Users cannot log in with SSO");
    HU_ASSERT_STR_EQ(list.tasks[0].status, "todo");
    HU_ASSERT_STR_EQ(list.tasks[0].priority, "high");
    HU_ASSERT_STR_EQ(list.tasks[0].project_name, "Auth");
    HU_ASSERT_STR_EQ(list.tasks[0].goal_title, "Q2 stability");

    HU_ASSERT_STR_EQ(list.tasks[1].id, "task-002");
    HU_ASSERT_STR_EQ(list.tasks[1].title, "Add rate limiting");
    HU_ASSERT_STR_EQ(list.tasks[1].status, "in_progress");

    hu_paperclip_task_list_free(&alloc, &list);
    hu_paperclip_client_deinit(&c);
}

static void task_free_handles_null_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_task_t task = {0};
    hu_paperclip_task_free(&alloc, &task);
    hu_paperclip_task_free(NULL, &task);
    hu_paperclip_task_free(&alloc, NULL);
}

static void task_list_free_handles_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_task_list_free(&alloc, NULL);
    hu_paperclip_task_list_t list = {0};
    hu_paperclip_task_list_free(&alloc, &list);
}

/* ── Comment list ────────────────────────────────────────────────────── */

static void comment_list_free_handles_null(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_comment_list_free(&alloc, NULL);
    hu_paperclip_comment_list_t list = {0};
    hu_paperclip_comment_list_free(&alloc, &list);
}

static void get_comments_returns_mock_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");

    hu_paperclip_comment_list_t comments = {0};
    hu_error_t err = hu_paperclip_get_comments(&c, "task-1", &comments);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(comments.count, 2u);

    HU_ASSERT_STR_EQ(comments.comments[0].id, "c-001");
    HU_ASSERT_STR_EQ(comments.comments[0].body, "Started investigating");
    HU_ASSERT_STR_EQ(comments.comments[0].author_name, "agent-1");
    HU_ASSERT_STR_EQ(comments.comments[0].created_at, "2026-04-06T10:00:00Z");

    HU_ASSERT_STR_EQ(comments.comments[1].id, "c-002");
    HU_ASSERT_STR_EQ(comments.comments[1].body, "Found root cause in auth module");

    hu_paperclip_comment_list_free(&alloc, &comments);
    hu_paperclip_client_deinit(&c);
}

/* ── Checkout / update / post ────────────────────────────────────────── */

static void checkout_requires_task_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_checkout_task(&c, NULL), HU_ERR_INVALID_ARGUMENT);
    hu_paperclip_client_deinit(&c);
}

static void checkout_ok_in_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_checkout_task(&c, "task-1"), HU_OK);
    hu_paperclip_client_deinit(&c);
}

static void update_task_requires_params(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_update_task(&c, NULL, "done"), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_paperclip_update_task(&c, "t1", NULL), HU_ERR_INVALID_ARGUMENT);
    hu_paperclip_client_deinit(&c);
}

static void update_task_ok_in_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_update_task(&c, "task-1", "done"), HU_OK);
    hu_paperclip_client_deinit(&c);
}

static void post_comment_requires_params(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_post_comment(&c, NULL, "hi", 2), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_paperclip_post_comment(&c, "t1", NULL, 0), HU_ERR_INVALID_ARGUMENT);
    hu_paperclip_client_deinit(&c);
}

static void post_comment_ok_in_test_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_post_comment(&c, "task-1", "test comment", 12), HU_OK);
    hu_paperclip_client_deinit(&c);
}

/* ── Tool creation ───────────────────────────────────────────────────── */

static void paperclip_tool_creates_ok(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_error_t err = hu_paperclip_tool_create(&alloc, &tool);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(tool.ctx);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "paperclip");
    HU_ASSERT_NOT_NULL(tool.vtable->description(tool.ctx));
    HU_ASSERT_NOT_NULL(tool.vtable->parameters_json(tool.ctx));
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void paperclip_tool_requires_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT_NOT_NULL(strstr(result.output, "action"));

    if (result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void paperclip_tool_unknown_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"explode\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_TRUE(strstr(result.output, "Unknown") != NULL ||
                   strstr(result.output, "Missing") != NULL);

    if (result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Direct client: get_task returns populated fields ────────────────── */

static void get_task_returns_mock_data(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");

    hu_paperclip_task_t task = {0};
    hu_error_t err = hu_paperclip_get_task(&c, "task-42", &task);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(task.id, "task-42");
    HU_ASSERT_STR_EQ(task.title, "Fix login bug");
    HU_ASSERT_STR_EQ(task.description, "Users cannot log in with SSO");
    HU_ASSERT_STR_EQ(task.status, "todo");
    HU_ASSERT_STR_EQ(task.priority, "high");
    HU_ASSERT_STR_EQ(task.project_name, "Auth");
    HU_ASSERT_STR_EQ(task.goal_title, "Q2 stability");

    hu_paperclip_task_free(&alloc, &task);
    hu_paperclip_client_deinit(&c);
}

/* ── Tool E2E: list_tasks action returns formatted task list ─────────── */

static void tool_list_tasks_returns_formatted_output(void) {
    hu_allocator_t alloc = hu_system_allocator();
#if defined(__unix__) || defined(__APPLE__)
    setenv("PAPERCLIP_API_URL", "http://test/api", 1);
    setenv("PAPERCLIP_AGENT_ID", "test-agent", 1);
#endif
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"list_tasks\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_TRUE(result.output_len > 0);
    HU_ASSERT_NOT_NULL(strstr(result.output, "2 task(s)"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "task-001"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "Fix login bug"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "task-002"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "Add rate limiting"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "todo"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "in_progress"));

    alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Tool E2E: get_task action returns formatted details ─────────────── */

static void tool_get_task_returns_formatted_details(void) {
    hu_allocator_t alloc = hu_system_allocator();
#if defined(__unix__) || defined(__APPLE__)
    setenv("PAPERCLIP_API_URL", "http://test/api", 1);
    setenv("PAPERCLIP_AGENT_ID", "test-agent", 1);
#endif
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"get_task\",\"task_id\":\"task-99\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_NOT_NULL(strstr(result.output, "Fix login bug"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "Status: todo"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "Priority: high"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "Users cannot log in with SSO"));

    alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Tool E2E: update_status action succeeds ─────────────────────────── */

static void tool_update_status_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
#if defined(__unix__) || defined(__APPLE__)
    setenv("PAPERCLIP_API_URL", "http://test/api", 1);
    setenv("PAPERCLIP_AGENT_ID", "test-agent", 1);
#endif
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"update_status\",\"task_id\":\"task-001\",\"status\":\"done\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_NOT_NULL(strstr(result.output, "task-001"));
    HU_ASSERT_NOT_NULL(strstr(result.output, "done"));

    if (result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Tool E2E: comment action succeeds ───────────────────────────────── */

static void tool_comment_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
#if defined(__unix__) || defined(__APPLE__)
    setenv("PAPERCLIP_API_URL", "http://test/api", 1);
    setenv("PAPERCLIP_AGENT_ID", "test-agent", 1);
#endif
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"comment\",\"task_id\":\"task-001\","
                       "\"body\":\"Deployed the fix to staging\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT_NOT_NULL(strstr(result.output, "Comment posted"));

    if (result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Tool E2E: missing params return clear error ─────────────────────── */

static void tool_update_status_missing_params(void) {
    hu_allocator_t alloc = hu_system_allocator();
#if defined(__unix__) || defined(__APPLE__)
    setenv("PAPERCLIP_API_URL", "http://test/api", 1);
    setenv("PAPERCLIP_AGENT_ID", "test-agent", 1);
#endif
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"update_status\",\"task_id\":\"task-001\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT_NOT_NULL(strstr(result.output, "status"));

    if (result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void tool_get_task_missing_task_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
#if defined(__unix__) || defined(__APPLE__)
    setenv("PAPERCLIP_API_URL", "http://test/api", 1);
    setenv("PAPERCLIP_AGENT_ID", "test-agent", 1);
#endif
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);

    const char *json = "{\"action\":\"get_task\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_FALSE(result.success);
    HU_ASSERT_NOT_NULL(strstr(result.output, "task_id"));

    if (result.output)
        alloc.free(alloc.ctx, (void *)result.output, result.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ═══════════════════════════════════════════════════════════════════════
 * RED-TEAM: Context Builder
 * Proves build_task_context assembles correct agent prompts from data.
 * ═══════════════════════════════════════════════════════════════════════ */

static void context_builder_full_task(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t client = {0};
    hu_paperclip_client_init_from_config(&client, &alloc, "http://test/api", "a1", "c1");
    client.wake_reason = hu_strdup(&alloc, "new_comment");

    hu_paperclip_task_t task = {0};
    task.id = hu_strdup(&alloc, "task-77");
    task.title = hu_strdup(&alloc, "Deploy auth fix");
    task.description = hu_strdup(&alloc, "SSO is broken for SAML users");
    task.status = hu_strdup(&alloc, "in_progress");
    task.priority = hu_strdup(&alloc, "critical");
    task.project_name = hu_strdup(&alloc, "Platform");
    task.goal_title = hu_strdup(&alloc, "Zero downtime");

    hu_paperclip_comment_t c1 = {
        .id = hu_strdup(&alloc, "c1"),
        .body = hu_strdup(&alloc, "Investigating SAML flow"),
        .author_name = hu_strdup(&alloc, "agent-x"),
        .created_at = hu_strdup(&alloc, "2026-04-06T09:00:00Z"),
    };
    hu_paperclip_comment_list_t comments = { .comments = &c1, .count = 1 };

    char buf[4096];
    size_t len = hu_paperclip_build_task_context(buf, sizeof(buf), &task, &comments, &client);
    buf[len] = '\0';

    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_NOT_NULL(strstr(buf, "PAPERCLIP TASK CONTEXT:"));
    HU_ASSERT_NOT_NULL(strstr(buf, "Goal: Zero downtime"));
    HU_ASSERT_NOT_NULL(strstr(buf, "Project: Platform"));
    HU_ASSERT_NOT_NULL(strstr(buf, "Task: Deploy auth fix"));
    HU_ASSERT_NOT_NULL(strstr(buf, "Priority: critical"));
    HU_ASSERT_NOT_NULL(strstr(buf, "Status: in_progress"));
    HU_ASSERT_NOT_NULL(strstr(buf, "SSO is broken for SAML users"));
    HU_ASSERT_NOT_NULL(strstr(buf, "agent-x: Investigating SAML flow"));
    HU_ASSERT_NOT_NULL(strstr(buf, "Wake reason: new_comment"));
    HU_ASSERT_NOT_NULL(strstr(buf, "paperclip tool"));

    hu_paperclip_task_free(&alloc, &task);
    if (c1.id) alloc.free(alloc.ctx, c1.id, strlen(c1.id) + 1);
    if (c1.body) alloc.free(alloc.ctx, c1.body, strlen(c1.body) + 1);
    if (c1.author_name) alloc.free(alloc.ctx, c1.author_name, strlen(c1.author_name) + 1);
    if (c1.created_at) alloc.free(alloc.ctx, c1.created_at, strlen(c1.created_at) + 1);
    hu_paperclip_client_deinit(&client);
}

static void context_builder_minimal_task(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t client = {0};
    hu_paperclip_client_init_from_config(&client, &alloc, "http://test/api", "a1", NULL);

    hu_paperclip_task_t task = {0};

    char buf[4096];
    size_t len = hu_paperclip_build_task_context(buf, sizeof(buf), &task, NULL, &client);
    buf[len] = '\0';

    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT_NOT_NULL(strstr(buf, "PAPERCLIP TASK CONTEXT:"));
    HU_ASSERT_NOT_NULL(strstr(buf, "(untitled)"));
    HU_ASSERT_NULL(strstr(buf, "Goal:"));
    HU_ASSERT_NULL(strstr(buf, "Project:"));
    HU_ASSERT_NULL(strstr(buf, "Priority:"));
    HU_ASSERT_NULL(strstr(buf, "Wake reason:"));

    hu_paperclip_client_deinit(&client);
}

static void context_builder_truncates_at_capacity(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t client = {0};
    hu_paperclip_client_init_from_config(&client, &alloc, "http://test/api", "a1", NULL);

    hu_paperclip_task_t task = {0};
    task.title = hu_strdup(&alloc, "Test");

    char tiny[32];
    size_t len = hu_paperclip_build_task_context(tiny, sizeof(tiny), &task, NULL, &client);
    HU_ASSERT_TRUE(len < sizeof(tiny));
    HU_ASSERT_TRUE(len > 0);

    hu_paperclip_task_free(&alloc, &task);
    hu_paperclip_client_deinit(&client);
}

/* ═══════════════════════════════════════════════════════════════════════
 * RED-TEAM: Integration flow — client → task → checkout → comments
 * Proves the full pre-heartbeat data pipeline works e2e.
 * ═══════════════════════════════════════════════════════════════════════ */

static void integration_full_task_lifecycle(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t client = {0};
    hu_paperclip_client_init_from_config(&client, &alloc, "http://test/api", "agent-e2e", "co-1");

    hu_paperclip_task_list_t list = {0};
    hu_error_t err = hu_paperclip_list_tasks(&client, &list);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(list.count > 0);

    const char *task_id = list.tasks[0].id;
    HU_ASSERT_NOT_NULL(task_id);

    err = hu_paperclip_checkout_task(&client, task_id);
    HU_ASSERT_EQ(err, HU_OK);

    hu_paperclip_task_t task = {0};
    err = hu_paperclip_get_task(&client, task_id, &task);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(task.title);

    hu_paperclip_comment_list_t comments = {0};
    err = hu_paperclip_get_comments(&client, task_id, &comments);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(comments.count > 0);

    char context[4096];
    size_t ctx_len = hu_paperclip_build_task_context(
        context, sizeof(context), &task, &comments, &client);
    context[ctx_len] = '\0';
    HU_ASSERT_TRUE(ctx_len > 100);
    HU_ASSERT_NOT_NULL(strstr(context, task.title));

    err = hu_paperclip_post_comment(&client, task_id, "Work complete", 13);
    HU_ASSERT_EQ(err, HU_OK);

    err = hu_paperclip_update_task(&client, task_id, "done");
    HU_ASSERT_EQ(err, HU_OK);

    hu_paperclip_comment_list_free(&alloc, &comments);
    hu_paperclip_task_free(&alloc, &task);
    hu_paperclip_task_list_free(&alloc, &list);
    hu_paperclip_client_deinit(&client);
}

/* ═══════════════════════════════════════════════════════════════════════
 * RED-TEAM: Adversarial — boundary attacks on every API
 * ═══════════════════════════════════════════════════════════════════════ */

static void adversarial_get_task_empty_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    hu_paperclip_task_t task = {0};
    hu_error_t err = hu_paperclip_get_task(&c, "", &task);
    HU_ASSERT_EQ(err, HU_OK);
    hu_paperclip_task_free(&alloc, &task);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_checkout_empty_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_checkout_task(&c, ""), HU_OK);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_update_empty_status(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_update_task(&c, "t1", ""), HU_OK);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_post_comment_zero_length(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_post_comment(&c, "t1", "", 0), HU_OK);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_get_comments_null_client(void) {
    hu_paperclip_comment_list_t out = {0};
    HU_ASSERT_EQ(hu_paperclip_get_comments(NULL, "t1", &out), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_get_comments_null_task_id(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    hu_paperclip_comment_list_t out = {0};
    HU_ASSERT_EQ(hu_paperclip_get_comments(&c, NULL, &out), HU_ERR_INVALID_ARGUMENT);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_get_comments_null_out(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_get_comments(&c, "t1", NULL), HU_ERR_INVALID_ARGUMENT);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_get_task_null_out(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    HU_ASSERT_EQ(hu_paperclip_get_task(&c, "t1", NULL), HU_ERR_INVALID_ARGUMENT);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_checkout_null_client(void) {
    HU_ASSERT_EQ(hu_paperclip_checkout_task(NULL, "t1"), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_update_null_client(void) {
    HU_ASSERT_EQ(hu_paperclip_update_task(NULL, "t1", "done"), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_post_null_client(void) {
    HU_ASSERT_EQ(hu_paperclip_post_comment(NULL, "t1", "x", 1), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_tool_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);
    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, NULL, &result),
                 HU_ERR_INVALID_ARGUMENT);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void adversarial_tool_null_result(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);
    const char *json = "{\"action\":\"list_tasks\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void adversarial_tool_null_ctx(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_paperclip_tool_create(&alloc, &tool);
    hu_tool_result_t result = {0};
    const char *json = "{\"action\":\"list_tasks\"}";
    hu_json_value_t *args = NULL;
    hu_json_parse(&alloc, json, strlen(json), &args);
    HU_ASSERT_EQ(tool.vtable->execute(NULL, &alloc, args, &result),
                 HU_ERR_INVALID_ARGUMENT);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void adversarial_tool_create_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_paperclip_tool_create(NULL, &tool), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_paperclip_tool_create(&alloc, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void adversarial_double_deinit_client(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    hu_paperclip_client_deinit(&c);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_double_free_task_list(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    hu_paperclip_task_list_t list = {0};
    hu_paperclip_list_tasks(&c, &list);
    hu_paperclip_task_list_free(&alloc, &list);
    hu_paperclip_task_list_free(&alloc, &list);
    hu_paperclip_client_deinit(&c);
}

static void adversarial_double_free_comment_list(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_paperclip_client_t c = {0};
    hu_paperclip_client_init_from_config(&c, &alloc, "http://test/api", "a1", "c1");
    hu_paperclip_comment_list_t comments = {0};
    hu_paperclip_get_comments(&c, "t1", &comments);
    hu_paperclip_comment_list_free(&alloc, &comments);
    hu_paperclip_comment_list_free(&alloc, &comments);
    hu_paperclip_client_deinit(&c);
}

/* ── Heartbeat ───────────────────────────────────────────────────────── */

static void heartbeat_null_alloc_returns_error(void) {
    HU_ASSERT_EQ(hu_paperclip_heartbeat(NULL, 0, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void heartbeat_no_env_fails_gracefully(void) {
#if defined(__unix__) || defined(__APPLE__)
    unsetenv("PAPERCLIP_API_URL");
    unsetenv("PAPERCLIP_AGENT_ID");
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_paperclip_heartbeat(&alloc, 0, NULL);
    HU_ASSERT_TRUE(err != HU_OK);
#endif
}

void run_paperclip_tests(void) {
    HU_TEST_SUITE("paperclip");

    /* Client init / deinit */
    HU_RUN_TEST(test_paperclip_client_init_null_args);
    HU_RUN_TEST(test_paperclip_client_init_no_env);
    HU_RUN_TEST(client_init_from_config_ok);
    HU_RUN_TEST(client_init_requires_url_and_id);
    HU_RUN_TEST(client_init_from_env_requires_vars);
    HU_RUN_TEST(client_deinit_handles_null);

    /* Client data round-trip */
    HU_RUN_TEST(test_paperclip_list_tasks_null_args);
    HU_RUN_TEST(list_tasks_returns_mock_data);
    HU_RUN_TEST(task_free_handles_null_fields);
    HU_RUN_TEST(task_list_free_handles_null);
    HU_RUN_TEST(comment_list_free_handles_null);
    HU_RUN_TEST(get_comments_returns_mock_data);
    HU_RUN_TEST(checkout_requires_task_id);
    HU_RUN_TEST(checkout_ok_in_test_mode);
    HU_RUN_TEST(update_task_requires_params);
    HU_RUN_TEST(update_task_ok_in_test_mode);
    HU_RUN_TEST(post_comment_requires_params);
    HU_RUN_TEST(post_comment_ok_in_test_mode);
    HU_RUN_TEST(get_task_returns_mock_data);

    /* Tool vtable e2e */
    HU_RUN_TEST(paperclip_tool_creates_ok);
    HU_RUN_TEST(paperclip_tool_requires_action);
    HU_RUN_TEST(paperclip_tool_unknown_action);
    HU_RUN_TEST(tool_list_tasks_returns_formatted_output);
    HU_RUN_TEST(tool_get_task_returns_formatted_details);
    HU_RUN_TEST(tool_update_status_succeeds);
    HU_RUN_TEST(tool_comment_succeeds);
    HU_RUN_TEST(tool_update_status_missing_params);
    HU_RUN_TEST(tool_get_task_missing_task_id);

    /* Context builder */
    HU_RUN_TEST(context_builder_full_task);
    HU_RUN_TEST(context_builder_minimal_task);
    HU_RUN_TEST(context_builder_truncates_at_capacity);

    /* Integration: full lifecycle */
    HU_RUN_TEST(integration_full_task_lifecycle);

    /* Red-team: adversarial boundary attacks */
    HU_RUN_TEST(adversarial_get_task_empty_id);
    HU_RUN_TEST(adversarial_checkout_empty_id);
    HU_RUN_TEST(adversarial_update_empty_status);
    HU_RUN_TEST(adversarial_post_comment_zero_length);
    HU_RUN_TEST(adversarial_get_comments_null_client);
    HU_RUN_TEST(adversarial_get_comments_null_task_id);
    HU_RUN_TEST(adversarial_get_comments_null_out);
    HU_RUN_TEST(adversarial_get_task_null_out);
    HU_RUN_TEST(adversarial_checkout_null_client);
    HU_RUN_TEST(adversarial_update_null_client);
    HU_RUN_TEST(adversarial_post_null_client);
    HU_RUN_TEST(adversarial_tool_null_args);
    HU_RUN_TEST(adversarial_tool_null_result);
    HU_RUN_TEST(adversarial_tool_null_ctx);
    HU_RUN_TEST(adversarial_tool_create_null_args);
    HU_RUN_TEST(adversarial_double_deinit_client);
    HU_RUN_TEST(adversarial_double_free_task_list);
    HU_RUN_TEST(adversarial_double_free_comment_list);

    /* Heartbeat */
    HU_RUN_TEST(heartbeat_null_alloc_returns_error);
    HU_RUN_TEST(heartbeat_no_env_fails_gracefully);
}
