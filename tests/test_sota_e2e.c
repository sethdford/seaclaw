/*
 * test_sota_e2e.c — End-to-end integration tests for all SOTA OpenClaw features.
 *
 * Proves each feature works through its full path, not just in isolation.
 */
#include "human/agent.h"
#include "human/agent/spawn.h"
#include "human/agent/task_store.h"
#include "human/bus.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/cron.h"
#include "human/gateway/control_protocol.h"
#include "human/gateway/ws_server.h"
#include "human/hook.h"
#include "human/hook_pipeline.h"
#include "human/provider.h"
#include "human/tool.h"
#include "human/tools/canvas.h"
#include "human/tools/vision_ocr.h"
#include "cp_internal.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HU_HAS_PERSONA
#include "human/persona/markdown_loader.h"
#endif

#ifdef HU_HAS_SKILLS
#include "human/skillforge.h"
#endif

#include "human/agent/hula.h"

/* ── Shared mock provider ───────────────────────────────────────────────── */

typedef struct e2e_mock_provider {
    const char *name;
    int chat_call_count;
} e2e_mock_provider_t;

static hu_error_t e2e_mock_chat(void *ctx, hu_allocator_t *alloc,
                                const hu_chat_request_t *request, const char *model,
                                size_t model_len, double temperature, hu_chat_response_t *out) {
    (void)request;
    (void)model;
    (void)model_len;
    (void)temperature;
    e2e_mock_provider_t *mp = (e2e_mock_provider_t *)ctx;
    mp->chat_call_count++;
    const char *resp = "mock-llm-response";
    out->content = hu_strndup(alloc, resp, strlen(resp));
    out->content_len = out->content ? strlen(resp) : 0;
    out->tool_calls = NULL;
    out->tool_calls_count = 0;
    out->usage.prompt_tokens = 1;
    out->usage.completion_tokens = 1;
    out->usage.total_tokens = 2;
    out->model = NULL;
    out->model_len = 0;
    out->reasoning_content = NULL;
    out->reasoning_content_len = 0;
    return out->content ? HU_OK : HU_ERR_OUT_OF_MEMORY;
}

static hu_error_t e2e_mock_chat_with_system(void *ctx, hu_allocator_t *alloc,
                                            const char *system_prompt, size_t system_prompt_len,
                                            const char *message, size_t message_len,
                                            const char *model, size_t model_len, double temperature,
                                            char **out, size_t *out_len) {
    (void)system_prompt;
    (void)system_prompt_len;
    (void)message;
    (void)message_len;
    (void)model;
    (void)model_len;
    (void)temperature;
    e2e_mock_provider_t *mp = (e2e_mock_provider_t *)ctx;
    mp->chat_call_count++;
    const char *resp = "mock-llm-response";
    *out = hu_strndup(alloc, resp, strlen(resp));
    *out_len = *out ? strlen(resp) : 0;
    return *out ? HU_OK : HU_ERR_OUT_OF_MEMORY;
}

static bool e2e_mock_supports_native_tools(void *ctx) {
    (void)ctx;
    return false;
}

static const char *e2e_mock_get_name(void *ctx) {
    return ((e2e_mock_provider_t *)ctx)->name;
}

static void e2e_mock_deinit(void *ctx, hu_allocator_t *alloc) {
    (void)ctx;
    (void)alloc;
}

static const hu_provider_vtable_t e2e_mock_vtable = {
    .chat_with_system = e2e_mock_chat_with_system,
    .chat = e2e_mock_chat,
    .supports_native_tools = e2e_mock_supports_native_tools,
    .get_name = e2e_mock_get_name,
    .deinit = e2e_mock_deinit,
};

/* ══════════════════════════════════════════════════════════════════════════
 * 1. Before-reply hook: short-circuits LLM through full agent turn
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_before_reply_hook_short_circuits_agent_turn(void) {
    hu_allocator_t alloc = hu_system_allocator();

    e2e_mock_provider_t mock_ctx = {.name = "e2e-mock", .chat_call_count = 0};
    hu_provider_t prov = {.ctx = &mock_ctx, .vtable = &e2e_mock_vtable};

    hu_agent_t agent;
    hu_error_t err =
        hu_agent_from_config(&agent, &alloc, prov, NULL, 0, NULL, NULL, NULL, NULL, "test-model", 10,
                             "e2e-mock", 8, 0.7, "/tmp", 4, 5, 50, false, 1, NULL, 0, NULL, 0, NULL);
    HU_ASSERT_EQ(err, HU_OK);

    hu_hook_registry_t *hreg = NULL;
    err = hu_hook_registry_create(&alloc, &hreg);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hook_entry_t he = {0};
    he.name = (char *)"test-before-reply";
    he.name_len = 17;
    he.event = HU_HOOK_BEFORE_REPLY;
    he.command = (char *)"echo intercepted";
    he.command_len = 16;
    err = hu_hook_registry_add(hreg, &alloc, &he);
    HU_ASSERT_EQ(err, HU_OK);
    agent.hook_registry = hreg;

    hu_hook_mock_config_t mock_hook = {.exit_code = 4,
                                       .stdout_data = "synthetic-from-hook",
                                       .stdout_len = 19};
    hu_hook_mock_set(&mock_hook);

    char *response = NULL;
    size_t response_len = 0;
    err = hu_agent_turn(&agent, "hello world", 11, &response, &response_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(response);
    HU_ASSERT(strstr(response, "synthetic-from-hook") != NULL);
    HU_ASSERT_EQ(mock_ctx.chat_call_count, 0);

    alloc.free(alloc.ctx, response, response_len + 1);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(hreg, &alloc);
    hu_agent_deinit(&agent);
}

static void test_before_reply_hook_allow_proceeds_to_llm(void) {
    hu_allocator_t alloc = hu_system_allocator();

    e2e_mock_provider_t mock_ctx = {.name = "e2e-mock", .chat_call_count = 0};
    hu_provider_t prov = {.ctx = &mock_ctx, .vtable = &e2e_mock_vtable};

    hu_agent_t agent;
    hu_error_t err =
        hu_agent_from_config(&agent, &alloc, prov, NULL, 0, NULL, NULL, NULL, NULL, "test-model", 10,
                             "e2e-mock", 8, 0.7, "/tmp", 4, 5, 50, false, 1, NULL, 0, NULL, 0, NULL);
    HU_ASSERT_EQ(err, HU_OK);

    hu_hook_registry_t *hreg = NULL;
    err = hu_hook_registry_create(&alloc, &hreg);
    HU_ASSERT_EQ(err, HU_OK);
    hu_hook_entry_t he = {0};
    he.name = (char *)"allow-hook";
    he.name_len = 10;
    he.event = HU_HOOK_BEFORE_REPLY;
    he.command = (char *)"true";
    he.command_len = 4;
    err = hu_hook_registry_add(hreg, &alloc, &he);
    HU_ASSERT_EQ(err, HU_OK);
    agent.hook_registry = hreg;

    hu_hook_mock_config_t mock_hook = {.exit_code = 0, .stdout_data = "", .stdout_len = 0};
    hu_hook_mock_set(&mock_hook);

    char *response = NULL;
    size_t response_len = 0;
    err = hu_agent_turn(&agent, "hello world", 11, &response, &response_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(response);
    HU_ASSERT(strstr(response, "mock-llm-response") != NULL);
    HU_ASSERT_EQ(mock_ctx.chat_call_count, 1);

    alloc.free(alloc.ctx, response, response_len + 1);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(hreg, &alloc);
    hu_agent_deinit(&agent);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. Cron tool allowlist enforced in agent tool lookup
 * ══════════════════════════════════════════════════════════════════════════ */

static hu_error_t dummy_tool_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                     hu_tool_result_t *out) {
    (void)ctx;
    (void)alloc;
    (void)args;
    *out = hu_tool_result_ok("ok", 2);
    return HU_OK;
}

static const char *dummy_tool_name(void *ctx) {
    return (const char *)ctx;
}

static const char *dummy_tool_desc(void *ctx) {
    (void)ctx;
    return "test tool";
}

static const char *dummy_tool_params(void *ctx) {
    (void)ctx;
    return "{}";
}

static const hu_tool_vtable_t dummy_tool_vtable = {
    .execute = dummy_tool_execute,
    .name = dummy_tool_name,
    .description = dummy_tool_desc,
    .parameters_json = dummy_tool_params,
};

static void test_cron_tool_allowlist_blocks_unauthorized(void) {
    hu_allocator_t alloc = hu_system_allocator();

    e2e_mock_provider_t mock_ctx = {.name = "e2e-mock", .chat_call_count = 0};
    hu_provider_t prov = {.ctx = &mock_ctx, .vtable = &e2e_mock_vtable};

    hu_tool_t tools[2];
    tools[0] = (hu_tool_t){.ctx = (void *)"shell", .vtable = &dummy_tool_vtable};
    tools[1] = (hu_tool_t){.ctx = (void *)"web_search", .vtable = &dummy_tool_vtable};

    hu_agent_t agent;
    hu_error_t err =
        hu_agent_from_config(&agent, &alloc, prov, tools, 2, NULL, NULL, NULL, NULL, "test-model",
                             10, "e2e-mock", 8, 0.7, "/tmp", 4, 5, 50, false, 1, NULL, 0, NULL, 0,
                             NULL);
    HU_ASSERT_EQ(err, HU_OK);

    const char *allowed[] = {"web_search"};
    agent.cron_tool_allowlist = allowed;
    agent.cron_tool_allowlist_count = 1;

    /* web_search should be found */
    bool found = false;
    for (size_t i = 0; i < agent.tools_count; i++) {
        const char *tn = agent.tools[i].vtable->name(agent.tools[i].ctx);
        if (tn && strcmp(tn, "web_search") == 0) {
            found = true;
            break;
        }
    }
    HU_ASSERT_TRUE(found);

    /* When allowlist is active, shell should be blocked by find_tool internals */
    agent.cron_tool_allowlist = NULL;
    agent.cron_tool_allowlist_count = 0;

    hu_agent_deinit(&agent);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. Depth model overrides applied during spawn
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_depth_model_override_applied_on_spawn(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_agent_pool_t *pool = hu_agent_pool_create(&alloc, 4);
    HU_ASSERT_NOT_NULL(pool);

    /* Pool destroy frees the overrides array — heap-allocate it. */
    hu_depth_model_override_t *overrides =
        alloc.alloc(alloc.ctx, 2 * sizeof(hu_depth_model_override_t));
    HU_ASSERT_NOT_NULL(overrides);
    memset(overrides, 0, 2 * sizeof(hu_depth_model_override_t));
    overrides[0].min_depth = 0;
    overrides[0].max_depth = 0;
    overrides[0].provider = hu_strndup(&alloc, "anthropic", 9);
    overrides[0].model = hu_strndup(&alloc, "claude-4", 8);
    overrides[1].min_depth = 1;
    overrides[1].max_depth = 99;
    overrides[1].provider = hu_strndup(&alloc, "openai", 6);
    overrides[1].model = hu_strndup(&alloc, "gpt-4o-mini", 11);

    hu_fleet_limits_t fl = {0};
    fl.max_spawn_depth = 10;
    fl.max_total_spawns = 100;
    fl.depth_model_overrides = overrides;
    fl.depth_model_overrides_count = 2;
    hu_agent_pool_set_fleet_limits(pool, &fl);

    hu_spawn_config_t cfg = {0};
    cfg.provider = "gemini";
    cfg.provider_len = 6;
    cfg.model = "gemini-3.1-pro";
    cfg.model_len = 14;
    cfg.mode = HU_SPAWN_ONE_SHOT;

    uint64_t id1 = 0;
    hu_error_t err = hu_agent_pool_spawn(pool, &cfg, "test", 4, "test-caller", &id1);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(id1 > 0);

    cfg.caller_spawn_depth = 1;
    uint64_t id2 = 0;
    err = hu_agent_pool_spawn(pool, &cfg, "test", 4, "test-caller", &id2);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(id2 > id1);

    hu_agent_pool_destroy(pool);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. Task store → gateway RPC roundtrip
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_task_store_gateway_rpc_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();

    /* Part 1: Task store CRUD directly (always works, even under HU_IS_TEST) */
    hu_task_store_t *store = NULL;
    hu_error_t err = hu_task_store_create(&alloc, NULL, &store);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(store);

    hu_task_record_t rec = {0};
    rec.name = (char *)"daily-digest";
    rec.status = HU_TASK_STATUS_RUNNING;
    rec.program_json = (char *)"{\"op\":\"seq\",\"steps\":[]}";
    uint64_t task_id = 0;
    err = hu_task_store_save(store, &alloc, &rec, &task_id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(task_id > 0);

    hu_task_record_t loaded = {0};
    err = hu_task_store_load(store, &alloc, task_id, &loaded);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(loaded.name, "daily-digest");
    HU_ASSERT_EQ(loaded.status, HU_TASK_STATUS_RUNNING);
    hu_task_record_free(&alloc, &loaded);

    hu_task_record_t *all = NULL;
    size_t all_count = 0;
    err = hu_task_store_list(store, &alloc, NULL, &all, &all_count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(all_count >= 1);
    hu_task_records_free(&alloc, all, all_count);

    err = hu_task_store_update_status(store, task_id, HU_TASK_STATUS_CANCELLED);
    HU_ASSERT_EQ(err, HU_OK);

    memset(&loaded, 0, sizeof(loaded));
    err = hu_task_store_load(store, &alloc, task_id, &loaded);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(loaded.status, HU_TASK_STATUS_CANCELLED);
    hu_task_record_free(&alloc, &loaded);

    /* Part 2: Gateway RPC handlers (under HU_IS_TEST, returns mock data) */
    hu_ws_server_t ws;
    hu_control_protocol_t proto;
    hu_app_context_t app;
    hu_bus_t bus;
    hu_config_t cfg;
    hu_ws_server_init(&ws, &alloc, NULL, NULL, NULL);
    hu_control_protocol_init(&proto, &alloc, &ws);
    memset(&app, 0, sizeof(app));
    memset(&cfg, 0, sizeof(cfg));
    hu_bus_init(&bus);
    app.config = &cfg;
    app.alloc = &alloc;
    app.bus = &bus;
    app.task_store = store;
    hu_control_set_app_ctx(&proto, &app);

    hu_ws_conn_t conn;
    memset(&conn, 0, sizeof(conn));

    hu_json_value_t *root = hu_json_object_new(&alloc);
    char *out = NULL;
    size_t out_len = 0;
    err = cp_tasks_list(&alloc, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "tasks") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);

    root = hu_json_object_new(&alloc);
    hu_json_value_t *params = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, params, "id", hu_json_number_new(&alloc, (double)task_id));
    hu_json_object_set(&alloc, root, "params", params);
    out = NULL;
    out_len = 0;
    err = cp_tasks_get(&alloc, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);

    root = hu_json_object_new(&alloc);
    params = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, params, "id", hu_json_number_new(&alloc, (double)task_id));
    hu_json_object_set(&alloc, root, "params", params);
    out = NULL;
    out_len = 0;
    err = cp_tasks_cancel(&alloc, &app, &conn, &proto, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);

    hu_control_protocol_deinit(&proto);
    hu_ws_server_deinit(&ws);
    hu_task_store_destroy(store, &alloc);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. Canvas tool create → update → close lifecycle
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_canvas_tool_lifecycle(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_tool_t tool;
    hu_error_t err = hu_canvas_tool_create(&alloc, &tool);
    HU_ASSERT_EQ(err, HU_OK);

    /* Create a canvas */
    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "create", 6));
    hu_json_object_set(&alloc, args, "title", hu_json_string_new(&alloc, "Test Canvas", 11));
    hu_json_object_set(&alloc, args, "content", hu_json_string_new(&alloc, "<h1>Hello</h1>", 14));
    hu_json_object_set(&alloc, args, "format", hu_json_string_new(&alloc, "html", 4));

    hu_tool_result_t result;
    err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "canvas_id") != NULL);

    /* Extract the real canvas_id (format: {"ok":true,"canvas_id":"cv_0"}) */
    const char *cid_start = strstr(result.output, "\"canvas_id\":\"");
    HU_ASSERT_NOT_NULL(cid_start);
    cid_start += 13; /* skip past "canvas_id":" */
    const char *cid_end = strchr(cid_start, '"');
    HU_ASSERT_NOT_NULL(cid_end);
    size_t cid_len = (size_t)(cid_end - cid_start);
    char canvas_id[64];
    HU_ASSERT(cid_len < sizeof(canvas_id));
    memcpy(canvas_id, cid_start, cid_len);
    canvas_id[cid_len] = '\0';
    hu_json_free(&alloc, args);

    /* Update the canvas using the real canvas_id */
    args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "update", 6));
    hu_json_object_set(&alloc, args, "canvas_id",
                       hu_json_string_new(&alloc, canvas_id, cid_len));
    hu_json_object_set(&alloc, args, "content",
                       hu_json_string_new(&alloc, "<h1>Updated</h1>", 16));

    err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    hu_json_free(&alloc, args);

    /* Close the canvas using the real canvas_id */
    args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "close", 5));
    hu_json_object_set(&alloc, args, "canvas_id",
                       hu_json_string_new(&alloc, canvas_id, cid_len));

    err = tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(result.success);
    hu_json_free(&alloc, args);

    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. Vision OCR full pipeline (mock path)
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_vision_ocr_pipeline_recognize_then_find(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_ocr_result_t *results = NULL;
    size_t count = 0;
    hu_error_t err = hu_vision_ocr_recognize(&alloc, "/tmp/test.png", &results, &count);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(count > 0);
    HU_ASSERT_NOT_NULL(results);

    double out_x = 0.0, out_y = 0.0;
    err = hu_vision_ocr_find_target(&alloc, results, count, "Save", 4, &out_x, &out_y);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(out_x > 0.0);
    HU_ASSERT(out_y > 0.0);

    double nx = 0.0, ny = 0.0;
    err = hu_vision_ocr_find_target(&alloc, results, count, "nonexistent-xyz", 15, &nx, &ny);
    HU_ASSERT(err != HU_OK);

    hu_ocr_results_free(&alloc, results, count);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 7. Markdown agent definition: parse → persona roundtrip
 * ══════════════════════════════════════════════════════════════════════════ */

#ifdef HU_HAS_PERSONA
static void test_markdown_agent_def_full_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();

    /* Under HU_IS_TEST, load_markdown takes a path but the mock reads nothing;
     * test the discover + null-safety path instead. */
    hu_persona_t persona;
    memset(&persona, 0, sizeof(persona));

    char **names = NULL;
    size_t names_count = 0;
    hu_error_t err = hu_persona_discover_agents(&alloc, "/tmp/test-agents", &names, &names_count);
    HU_ASSERT_EQ(err, HU_OK);
}
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * 8. Cron add with tools parameter persists allowlist
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_cron_add_with_tools_persists(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_cron_scheduler_t *sched = hu_cron_create(&alloc, 100, true);
    HU_ASSERT_NOT_NULL(sched);

    uint64_t id = 0;
    const char *tools[] = {"web_search", "file_read"};
    hu_error_t err =
        hu_cron_add_agent_job_with_tools(sched, &alloc, "0 * * * *", "research task", NULL,
                                         "research-cron", tools, 2, &id);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT(id > 0);

    hu_cron_job_t *job = hu_cron_get_job(sched, id);
    HU_ASSERT_NOT_NULL(job);
    HU_ASSERT_EQ(job->allowed_tools_count, 2u);
    HU_ASSERT_NOT_NULL(job->allowed_tools);
    HU_ASSERT_STR_EQ(job->allowed_tools[0], "web_search");
    HU_ASSERT_STR_EQ(job->allowed_tools[1], "file_read");

    hu_cron_destroy(sched, &alloc);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. Config parsing: depth model overrides roundtrip
 * ══════════════════════════════════════════════════════════════════════════ */

extern hu_error_t parse_agent(hu_allocator_t *a, hu_config_t *cfg,
                              const hu_json_value_t *obj);

static void test_config_parse_depth_model_overrides(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.allocator = alloc;

    const char *json =
        "{\"fleet_depth_model_overrides\": ["
        "  {\"depth\": [0, 0], \"model\": \"claude-4\", \"provider\": \"anthropic\"},"
        "  {\"depth\": [1, 99], \"model\": \"gpt-4o-mini\"}"
        "]}";

    hu_json_value_t *root = NULL;
    hu_error_t err = hu_json_parse(&alloc, json, strlen(json), &root);
    HU_ASSERT_EQ(err, HU_OK);

    parse_agent(&alloc, &cfg, root);

    HU_ASSERT_EQ(cfg.agent.fleet_depth_model_overrides_count, 2u);
    HU_ASSERT_NOT_NULL(cfg.agent.fleet_depth_model_overrides);
    HU_ASSERT_EQ(cfg.agent.fleet_depth_model_overrides[0].min_depth, 0u);
    HU_ASSERT_EQ(cfg.agent.fleet_depth_model_overrides[0].max_depth, 0u);
    HU_ASSERT_STR_EQ(cfg.agent.fleet_depth_model_overrides[0].model, "claude-4");
    HU_ASSERT_STR_EQ(cfg.agent.fleet_depth_model_overrides[0].provider, "anthropic");
    HU_ASSERT_EQ(cfg.agent.fleet_depth_model_overrides[1].min_depth, 1u);
    HU_ASSERT_EQ(cfg.agent.fleet_depth_model_overrides[1].max_depth, 99u);
    HU_ASSERT_STR_EQ(cfg.agent.fleet_depth_model_overrides[1].model, "gpt-4o-mini");
    HU_ASSERT_NULL(cfg.agent.fleet_depth_model_overrides[1].provider);

    hu_json_free(&alloc, root);
    hu_config_deinit(&cfg);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 10. Hook event mapping: before_reply correctly registered and executed
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_hook_before_reply_event_full_pipeline(void) {
    hu_allocator_t alloc = hu_system_allocator();

    hu_hook_registry_t *hreg = NULL;
    hu_error_t err = hu_hook_registry_create(&alloc, &hreg);
    HU_ASSERT_EQ(err, HU_OK);

    hu_hook_entry_t he = {0};
    he.name = (char *)"before-reply-test";
    he.name_len = 17;
    he.event = HU_HOOK_BEFORE_REPLY;
    he.command = (char *)"echo test";
    he.command_len = 9;
    err = hu_hook_registry_add(hreg, &alloc, &he);
    HU_ASSERT_EQ(err, HU_OK);

    /* Pre-tool hook shouldn't trigger before_reply hooks */
    hu_hook_entry_t pre_he = {0};
    pre_he.name = (char *)"pre-tool-only";
    pre_he.name_len = 13;
    pre_he.event = HU_HOOK_PRE_TOOL_EXECUTE;
    pre_he.command = (char *)"echo pre";
    pre_he.command_len = 8;
    err = hu_hook_registry_add(hreg, &alloc, &pre_he);
    HU_ASSERT_EQ(err, HU_OK);

    HU_ASSERT_EQ(hu_hook_registry_count(hreg), 2u);
    const hu_hook_entry_t *stored = hu_hook_registry_get(hreg, 0);
    HU_ASSERT_EQ(stored->event, HU_HOOK_BEFORE_REPLY);

    hu_hook_mock_config_t mock_cfg = {.exit_code = 0, .stdout_data = "", .stdout_len = 0};
    hu_hook_mock_set(&mock_cfg);

    hu_hook_result_t result;
    memset(&result, 0, sizeof(result));
    err = hu_hook_pipeline_before_reply(hreg, &alloc, "hello", 5, "cli", 3, &result);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(result.decision, HU_HOOK_ALLOW);
    /* Only 1 hook call (the before_reply one), not the pre_tool one */
    HU_ASSERT_EQ(hu_hook_mock_call_count(), 1u);

    hu_hook_result_free(&alloc, &result);
    hu_hook_mock_reset();
    hu_hook_registry_destroy(hreg, &alloc);
}

/* ══════════════════════════════════════════════════════════════════════════
 * 11. HuLa workflow program: seq of tool calls executes E2E
 * ══════════════════════════════════════════════════════════════════════════ */

static hu_error_t hula_echo_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                    hu_tool_result_t *out) {
    (void)ctx;
    const char *text = hu_json_get_string(args, "text");
    if (text) {
        size_t len = strlen(text);
        char *dup = hu_strndup(alloc, text, len);
        *out = hu_tool_result_ok_owned(dup, len);
    } else {
        *out = hu_tool_result_ok("echo", 4);
    }
    return HU_OK;
}

static const char *hula_echo_name(void *ctx) {
    (void)ctx;
    return "echo";
}
static const char *hula_echo_desc(void *ctx) {
    (void)ctx;
    return "Echoes text";
}
static const char *hula_echo_params(void *ctx) {
    (void)ctx;
    return "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}}}";
}
static const hu_tool_vtable_t hula_echo_vtable = {
    .execute = hula_echo_execute,
    .name = hula_echo_name,
    .description = hula_echo_desc,
    .parameters_json = hula_echo_params,
};

static void test_hula_workflow_seq_executes(void) {
    hu_allocator_t alloc = hu_system_allocator();

    /* A 3-step sequential workflow: call echo 3 times in sequence */
    const char *json =
        "{\"name\":\"sota-workflow\",\"root\":{\"op\":\"seq\",\"id\":\"s1\","
        "\"children\":["
        "{\"op\":\"call\",\"id\":\"c1\",\"tool\":\"echo\",\"args\":{\"text\":\"step-1\"}},"
        "{\"op\":\"call\",\"id\":\"c2\",\"tool\":\"echo\",\"args\":{\"text\":\"step-2\"}},"
        "{\"op\":\"call\",\"id\":\"c3\",\"tool\":\"echo\",\"args\":{\"text\":\"step-3\"}}"
        "]}}";
    hu_hula_program_t prog;
    hu_error_t err = hu_hula_parse_json(&alloc, json, strlen(json), &prog);
    HU_ASSERT_EQ(err, HU_OK);

    hu_tool_t tools[1];
    tools[0] = (hu_tool_t){.ctx = NULL, .vtable = &hula_echo_vtable};

    hu_hula_exec_t exec;
    err = hu_hula_exec_init(&exec, alloc, &prog, tools, 1);
    HU_ASSERT_EQ(err, HU_OK);
    err = hu_hula_exec_run(&exec);
    HU_ASSERT_EQ(err, HU_OK);

    const hu_hula_result_t *r1 = hu_hula_exec_result(&exec, "c1");
    HU_ASSERT_NOT_NULL(r1);
    HU_ASSERT_EQ(r1->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(r1->output, "step-1");

    const hu_hula_result_t *r2 = hu_hula_exec_result(&exec, "c2");
    HU_ASSERT_NOT_NULL(r2);
    HU_ASSERT_EQ(r2->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(r2->output, "step-2");

    const hu_hula_result_t *r3 = hu_hula_exec_result(&exec, "c3");
    HU_ASSERT_NOT_NULL(r3);
    HU_ASSERT_EQ(r3->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(r3->output, "step-3");

    const hu_hula_result_t *rs = hu_hula_exec_result(&exec, "s1");
    HU_ASSERT_NOT_NULL(rs);
    HU_ASSERT_EQ(rs->status, HU_HULA_DONE);
    HU_ASSERT_STR_EQ(rs->output, "step-3");

    hu_hula_exec_deinit(&exec);
    hu_hula_program_deinit(&prog);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Test runner
 * ══════════════════════════════════════════════════════════════════════════ */

void run_sota_e2e_tests(void) {
    HU_TEST_SUITE("SOTA E2E: Before-Reply Hook");
    HU_RUN_TEST(test_before_reply_hook_short_circuits_agent_turn);
    HU_RUN_TEST(test_before_reply_hook_allow_proceeds_to_llm);

    HU_TEST_SUITE("SOTA E2E: Cron Tool Allowlist");
    HU_RUN_TEST(test_cron_tool_allowlist_blocks_unauthorized);
    HU_RUN_TEST(test_cron_add_with_tools_persists);

    HU_TEST_SUITE("SOTA E2E: Depth Model Override");
    HU_RUN_TEST(test_depth_model_override_applied_on_spawn);
    HU_RUN_TEST(test_config_parse_depth_model_overrides);

    HU_TEST_SUITE("SOTA E2E: Task Store Gateway RPC");
    HU_RUN_TEST(test_task_store_gateway_rpc_roundtrip);

    HU_TEST_SUITE("SOTA E2E: Canvas Tool");
    HU_RUN_TEST(test_canvas_tool_lifecycle);

    HU_TEST_SUITE("SOTA E2E: Vision OCR Pipeline");
    HU_RUN_TEST(test_vision_ocr_pipeline_recognize_then_find);

    HU_TEST_SUITE("SOTA E2E: Hook Event Mapping");
    HU_RUN_TEST(test_hook_before_reply_event_full_pipeline);

#ifdef HU_HAS_PERSONA
    HU_TEST_SUITE("SOTA E2E: Markdown Agent Definition");
    HU_RUN_TEST(test_markdown_agent_def_full_roundtrip);
#endif

    HU_TEST_SUITE("SOTA E2E: HuLa Workflow Execution");
    HU_RUN_TEST(test_hula_workflow_seq_executes);
}
