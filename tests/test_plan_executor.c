#include "human/agent/plan_executor.h"
#include "human/agent/planner.h"
#include "human/core/json.h"
#include "test_framework.h"
#include <string.h>

static void *tpe_alloc_fn(void *ctx, size_t size) { (void)ctx; return malloc(size); }
static void *tpe_realloc_fn(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    (void)ctx; (void)old_size; return realloc(ptr, new_size);
}
static void tpe_free_fn(void *ctx, void *ptr, size_t size) { (void)ctx; (void)size; free(ptr); }

static hu_allocator_t tpe_alloc;
static void tpe_setup(void) {
    tpe_alloc = (hu_allocator_t){
        .alloc = tpe_alloc_fn, .realloc = tpe_realloc_fn, .free = tpe_free_fn, .ctx = NULL,
    };
}

static hu_error_t mock_exec_ok(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                               hu_tool_result_t *out) {
    (void)ctx; (void)alloc; (void)args;
    *out = hu_tool_result_ok("done", 4);
    return HU_OK;
}
static const char *mock_name_shell(void *ctx) { (void)ctx; return "shell"; }
static const char *mock_desc(void *ctx) { (void)ctx; return "mock"; }
static const char *mock_params(void *ctx) { (void)ctx; return "{}"; }
static const hu_tool_vtable_t mock_ok_vt = {
    .execute = mock_exec_ok, .name = mock_name_shell,
    .description = mock_desc, .parameters_json = mock_params,
};

static hu_error_t mock_exec_fail(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                 hu_tool_result_t *out) {
    (void)ctx; (void)alloc; (void)args;
    *out = hu_tool_result_fail("command failed", 14);
    return HU_OK;
}
static const char *mock_name_fail(void *ctx) { (void)ctx; return "fail_tool"; }
static const hu_tool_vtable_t mock_fail_vt = {
    .execute = mock_exec_fail, .name = mock_name_fail,
    .description = mock_desc, .parameters_json = mock_params,
};

static hu_error_t mock_chat(void *ctx, hu_allocator_t *alloc, const hu_chat_request_t *req,
                            const char *model, size_t model_len, double temp,
                            hu_chat_response_t *out) {
    (void)ctx; (void)alloc; (void)req; (void)model; (void)model_len; (void)temp;
    memset(out, 0, sizeof(*out));
    return HU_OK;
}
static const char *mock_prov_name(void *ctx) { (void)ctx; return "mock"; }
static const hu_provider_vtable_t mock_prov_vt = {
    .chat = mock_chat, .get_name = mock_prov_name,
};

static void plan_executor_init_defaults(void) {
    tpe_setup();
    hu_tool_t tools[1] = {{.ctx = NULL, .vtable = &mock_ok_vt}};
    hu_provider_t prov = {.ctx = NULL, .vtable = &mock_prov_vt};
    hu_plan_executor_t exec;
    hu_plan_executor_init(&exec, &tpe_alloc, &prov, "test", 4, tools, 1);
    HU_ASSERT_EQ(exec.max_replans, HU_PLAN_EXEC_MAX_REPLANS);
    HU_ASSERT_EQ(exec.tools_count, 1);
}

static void plan_executor_run_single_step_success(void) {
    tpe_setup();
    hu_tool_t tools[1] = {{.ctx = NULL, .vtable = &mock_ok_vt}};
    hu_provider_t prov = {.ctx = NULL, .vtable = &mock_prov_vt};
    hu_plan_executor_t exec;
    hu_plan_executor_init(&exec, &tpe_alloc, &prov, "test", 4, tools, 1);

    const char *json = "{\"steps\":[{\"tool\":\"shell\",\"args\":{\"command\":\"ls\"}}]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&tpe_alloc, json, strlen(json), &plan), HU_OK);

    hu_plan_exec_result_t result;
    HU_ASSERT_EQ(hu_plan_executor_run(&exec, plan, "list files", 10, &result), HU_OK);
    HU_ASSERT_EQ(result.steps_completed, 1);
    HU_ASSERT_EQ(result.steps_failed, 0);
    HU_ASSERT(result.goal_achieved);
    hu_plan_free(&tpe_alloc, plan);
}

static void plan_executor_run_step_failure_triggers_replan(void) {
    tpe_setup();
    hu_tool_t tools[2] = {
        {.ctx = NULL, .vtable = &mock_fail_vt},
        {.ctx = NULL, .vtable = &mock_ok_vt},
    };
    hu_provider_t prov = {.ctx = NULL, .vtable = &mock_prov_vt};
    hu_plan_executor_t exec;
    hu_plan_executor_init(&exec, &tpe_alloc, &prov, "test", 4, tools, 2);

    const char *json = "{\"steps\":[{\"tool\":\"fail_tool\",\"args\":{}}]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&tpe_alloc, json, strlen(json), &plan), HU_OK);

    hu_plan_exec_result_t result;
    HU_ASSERT_EQ(hu_plan_executor_run(&exec, plan, "do something", 12, &result), HU_OK);
    HU_ASSERT(result.replans > 0);
    hu_plan_free(&tpe_alloc, plan);
}

static void plan_executor_run_missing_tool(void) {
    tpe_setup();
    hu_tool_t tools[1] = {{.ctx = NULL, .vtable = &mock_ok_vt}};
    hu_provider_t prov = {.ctx = NULL, .vtable = &mock_prov_vt};
    hu_plan_executor_t exec;
    hu_plan_executor_init(&exec, &tpe_alloc, &prov, "test", 4, tools, 1);

    const char *json = "{\"steps\":[{\"tool\":\"nonexistent\",\"args\":{}}]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&tpe_alloc, json, strlen(json), &plan), HU_OK);

    hu_plan_exec_result_t result;
    HU_ASSERT_EQ(hu_plan_executor_run(&exec, plan, "find stuff", 10, &result), HU_OK);
    HU_ASSERT(result.replans > 0);
    hu_plan_free(&tpe_alloc, plan);
}

static void plan_executor_run_goal_uses_planner(void) {
    tpe_setup();
    hu_tool_t tools[1] = {{.ctx = NULL, .vtable = &mock_ok_vt}};
    hu_provider_t prov = {.ctx = NULL, .vtable = &mock_prov_vt};
    hu_plan_executor_t exec;
    hu_plan_executor_init(&exec, &tpe_alloc, &prov, "test", 4, tools, 1);

    hu_plan_exec_result_t result;
    HU_ASSERT_EQ(hu_plan_executor_run_goal(&exec, "list all files", 14, &result), HU_OK);
    HU_ASSERT(result.steps_completed > 0);
    HU_ASSERT(result.goal_achieved);
}

static void plan_executor_null_args(void) {
    hu_plan_exec_result_t result;
    HU_ASSERT_EQ(hu_plan_executor_run(NULL, NULL, NULL, 0, &result), HU_ERR_INVALID_ARGUMENT);
}

static void plan_executor_multi_step(void) {
    tpe_setup();
    hu_tool_t tools[1] = {{.ctx = NULL, .vtable = &mock_ok_vt}};
    hu_provider_t prov = {.ctx = NULL, .vtable = &mock_prov_vt};
    hu_plan_executor_t exec;
    hu_plan_executor_init(&exec, &tpe_alloc, &prov, "test", 4, tools, 1);

    const char *json =
        "{\"steps\":["
        "{\"tool\":\"shell\",\"args\":{\"command\":\"mkdir build\"}},"
        "{\"tool\":\"shell\",\"args\":{\"command\":\"cmake ..\"}},"
        "{\"tool\":\"shell\",\"args\":{\"command\":\"make\"}}"
        "]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&tpe_alloc, json, strlen(json), &plan), HU_OK);

    hu_plan_exec_result_t result;
    HU_ASSERT_EQ(hu_plan_executor_run(&exec, plan, "build project", 13, &result), HU_OK);
    HU_ASSERT_EQ(result.steps_completed, 3);
    HU_ASSERT(result.goal_achieved);
    HU_ASSERT(result.summary_len > 0);
    hu_plan_free(&tpe_alloc, plan);
}

static void plan_executor_streaming_vtable_field(void) {
    hu_tool_vtable_t vt;
    memset(&vt, 0, sizeof(vt));
    HU_ASSERT_NULL(vt.execute_streaming);
}

static size_t stream_chunk_count;
static size_t stream_total_bytes;
static void test_on_chunk(void *ctx, const char *data, size_t len)
    __attribute__((unused));
static void test_on_chunk(void *ctx, const char *data, size_t len) {
    (void)ctx; (void)data;
    stream_chunk_count++;
    stream_total_bytes += len;
}

static hu_error_t mock_exec_streaming(void *ctx, hu_allocator_t *alloc,
                                       const hu_json_value_t *args,
                                       void (*on_chunk)(void *cb, const char *data, size_t len),
                                       void *cb_ctx,
                                       hu_tool_result_t *out) {
    (void)ctx; (void)alloc; (void)args;
    if (on_chunk) {
        on_chunk(cb_ctx, "partial1", 8);
        on_chunk(cb_ctx, "partial2", 8);
    }
    *out = hu_tool_result_ok("partial1partial2", 16);
    return HU_OK;
}

static const char *mock_name_stream(void *ctx) { (void)ctx; return "stream_tool"; }
static const hu_tool_vtable_t mock_stream_vt = {
    .execute = mock_exec_ok, .name = mock_name_stream,
    .description = mock_desc, .parameters_json = mock_params,
    .execute_streaming = mock_exec_streaming,
};

static void plan_executor_streaming_tool_integration(void) {
    tpe_setup();
    hu_tool_t tools[1] = {{.ctx = NULL, .vtable = &mock_stream_vt}};
    hu_provider_t prov = {.ctx = NULL, .vtable = &mock_prov_vt};
    hu_plan_executor_t exec;
    hu_plan_executor_init(&exec, &tpe_alloc, &prov, "test", 4, tools, 1);

    const char *json = "{\"steps\":[{\"tool\":\"stream_tool\",\"args\":{}}]}";
    hu_plan_t *plan = NULL;
    HU_ASSERT_EQ(hu_planner_create_plan(&tpe_alloc, json, strlen(json), &plan), HU_OK);

    hu_plan_exec_result_t result;
    HU_ASSERT_EQ(hu_plan_executor_run(&exec, plan, "test streaming", 14, &result), HU_OK);
    HU_ASSERT_EQ(result.steps_completed, 1);
    HU_ASSERT(result.goal_achieved);
    hu_plan_free(&tpe_alloc, plan);
}

static void plan_executor_dispatcher_prefers_streaming(void) {
    stream_chunk_count = 0;
    stream_total_bytes = 0;
    hu_tool_vtable_t vt;
    memset(&vt, 0, sizeof(vt));
    vt.execute = mock_exec_ok;
    vt.execute_streaming = mock_exec_streaming;
    vt.name = mock_name_stream;
    vt.description = mock_desc;
    vt.parameters_json = mock_params;
    HU_ASSERT_NOT_NULL(vt.execute_streaming);
    HU_ASSERT_NOT_NULL(vt.execute);
}

void run_plan_executor_tests(void) {
    HU_TEST_SUITE("PlanExecutor");
    HU_RUN_TEST(plan_executor_init_defaults);
    HU_RUN_TEST(plan_executor_run_single_step_success);
    HU_RUN_TEST(plan_executor_run_step_failure_triggers_replan);
    HU_RUN_TEST(plan_executor_run_missing_tool);
    HU_RUN_TEST(plan_executor_run_goal_uses_planner);
    HU_RUN_TEST(plan_executor_null_args);
    HU_RUN_TEST(plan_executor_multi_step);
    HU_RUN_TEST(plan_executor_streaming_vtable_field);
    HU_RUN_TEST(plan_executor_streaming_tool_integration);
    HU_RUN_TEST(plan_executor_dispatcher_prefers_streaming);
}
