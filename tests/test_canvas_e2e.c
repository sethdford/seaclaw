#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/gateway/control_protocol.h"
#include "human/tools/canvas.h"
#include "test_framework.h"
#include <string.h>

/* Declare RPC handlers used in tests (defined in cp_canvas.c) */
extern hu_error_t cp_canvas_list(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len);
extern hu_error_t cp_canvas_get(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                char **out, size_t *out_len);
extern hu_error_t cp_canvas_edit(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len);
extern hu_error_t cp_canvas_undo(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len);
extern hu_error_t cp_canvas_redo(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len);

/* Helper: create tool and extract canvas_id from create response */
static hu_tool_t setup_tool(hu_allocator_t *alloc) {
    hu_tool_t tool = {0};
    hu_canvas_tool_create(alloc, &tool);
    return tool;
}

static char *create_canvas(hu_allocator_t *alloc, hu_tool_t *tool, const char *fmt) {
    hu_json_value_t *args = hu_json_object_new(alloc);
    hu_json_object_set(alloc, args, "action", hu_json_string_new(alloc, "create", 6));
    if (fmt)
        hu_json_object_set(alloc, args, "format",
                           hu_json_string_new(alloc, fmt, strlen(fmt)));
    hu_tool_result_t res = {0};
    tool->vtable->execute(tool->ctx, alloc, args, &res);
    char *id = NULL;
    if (res.output) {
        const char *p = strstr(res.output, "\"canvas_id\":\"");
        if (p) {
            p += strlen("\"canvas_id\":\"");
            const char *end = strchr(p, '"');
            if (end)
                id = hu_strndup(alloc, p, (size_t)(end - p));
        }
    }
    if (res.output_owned && res.output)
        alloc->free(alloc->ctx, (void *)res.output, res.output_len + 1);
    hu_json_free(alloc, args);
    return id;
}

static hu_tool_result_t update_canvas(hu_allocator_t *alloc, hu_tool_t *tool, const char *id,
                                      const char *content) {
    hu_json_value_t *args = hu_json_object_new(alloc);
    hu_json_object_set(alloc, args, "action", hu_json_string_new(alloc, "update", 6));
    hu_json_object_set(alloc, args, "canvas_id", hu_json_string_new(alloc, id, strlen(id)));
    hu_json_object_set(alloc, args, "content",
                       hu_json_string_new(alloc, content, strlen(content)));
    hu_tool_result_t res = {0};
    tool->vtable->execute(tool->ctx, alloc, args, &res);
    hu_json_free(alloc, args);
    return res;
}

/* ── Version History Tests ─────────────────────────────────────────── */

static void test_version_history_create_update_undo_redo(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = setup_tool(&alloc);

    char *id = create_canvas(&alloc, &tool, "html");
    HU_ASSERT_NOT_NULL(id);

    /* Update 5 times to build version history */
    const char *versions[] = {"v1", "v2", "v3", "v4", "v5"};
    for (int i = 0; i < 5; i++) {
        hu_tool_result_t r = update_canvas(&alloc, &tool, id, versions[i]);
        HU_ASSERT_TRUE(r.success);
        if (r.output_owned && r.output)
            alloc.free(alloc.ctx, (void *)r.output, r.output_len + 1);
    }

    /* Verify current content via store API */
    hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
    HU_ASSERT_NOT_NULL(store);
    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_find(store, id, &info));
    HU_ASSERT_STR_EQ(info.content, "v5");
    HU_ASSERT_EQ(info.version_seq, 5u);
    HU_ASSERT_EQ(info.version_count, 5u);

    /* Undo twice: should go v5 -> v4 -> v3 */
    hu_canvas_info_t undo1;
    HU_ASSERT_EQ(hu_canvas_store_undo(store, id, &undo1), HU_OK);
    HU_ASSERT_STR_EQ(undo1.content, "v4");

    hu_canvas_info_t undo2;
    HU_ASSERT_EQ(hu_canvas_store_undo(store, id, &undo2), HU_OK);
    HU_ASSERT_STR_EQ(undo2.content, "v3");

    /* Redo once: should go v3 -> v4 */
    hu_canvas_info_t redo1;
    HU_ASSERT_EQ(hu_canvas_store_redo(store, id, &redo1), HU_OK);
    HU_ASSERT_STR_EQ(redo1.content, "v4");

    alloc.free(alloc.ctx, id, strlen(id) + 1);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── State Sync / Store API Tests ──────────────────────────────────── */

static void test_canvas_store_list_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = setup_tool(&alloc);

    char *ids[3];
    ids[0] = create_canvas(&alloc, &tool, "html");
    ids[1] = create_canvas(&alloc, &tool, "react");
    ids[2] = create_canvas(&alloc, &tool, "mermaid");

    hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
    HU_ASSERT_EQ(hu_canvas_store_count(store), 3u);

    for (int i = 0; i < 3; i++) {
        hu_canvas_info_t info;
        HU_ASSERT_TRUE(hu_canvas_store_get(store, (size_t)i, &info));
        HU_ASSERT_NOT_NULL(info.canvas_id);
    }

    for (int i = 0; i < 3; i++)
        alloc.free(alloc.ctx, ids[i], strlen(ids[i]) + 1);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── User Edit Tests ───────────────────────────────────────────────── */

static void test_canvas_user_edit_sets_pending(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = setup_tool(&alloc);

    char *id = create_canvas(&alloc, &tool, "html");
    HU_ASSERT_NOT_NULL(id);

    hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
    hu_error_t err = hu_canvas_store_edit(store, id, "<p>user edit</p>");
    HU_ASSERT_EQ(err, HU_OK);

    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_find(store, id, &info));
    HU_ASSERT_TRUE(info.user_edit_pending);

    /* Agent update clears the pending flag */
    hu_tool_result_t r = update_canvas(&alloc, &tool, id, "<p>agent update</p>");
    HU_ASSERT_TRUE(r.success);
    if (r.output_owned && r.output)
        alloc.free(alloc.ctx, (void *)r.output, r.output_len + 1);

    HU_ASSERT_TRUE(hu_canvas_store_find(store, id, &info));
    HU_ASSERT_FALSE(info.user_edit_pending);
    HU_ASSERT_STR_EQ(info.content, "<p>agent update</p>");

    alloc.free(alloc.ctx, id, strlen(id) + 1);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Format Expansion Tests ────────────────────────────────────────── */

static void test_canvas_format_expansion_all_types(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = setup_tool(&alloc);

    const char *formats[] = {"html", "svg", "mockup", "react", "mermaid", "markdown", "code"};
    size_t nfmt = sizeof(formats) / sizeof(formats[0]);

    for (size_t i = 0; i < nfmt; i++) {
        char *id = create_canvas(&alloc, &tool, formats[i]);
        HU_ASSERT_NOT_NULL(id);

        hu_canvas_info_t info;
        hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
        HU_ASSERT_TRUE(hu_canvas_store_find(store, id, &info));
        HU_ASSERT_STR_EQ(info.format, formats[i]);

        alloc.free(alloc.ctx, id, strlen(id) + 1);
    }

    hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
    HU_ASSERT_EQ(hu_canvas_store_count(store), nfmt);

    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Reconnect Resilience (store state survives operations) ────────── */

static void test_canvas_reconnect_state_matches(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = setup_tool(&alloc);

    char *id1 = create_canvas(&alloc, &tool, "html");
    char *id2 = create_canvas(&alloc, &tool, "react");
    HU_ASSERT_NOT_NULL(id1);
    HU_ASSERT_NOT_NULL(id2);

    hu_tool_result_t r1 = update_canvas(&alloc, &tool, id1, "<h1>Hello</h1>");
    HU_ASSERT_TRUE(r1.success);
    if (r1.output_owned && r1.output)
        alloc.free(alloc.ctx, (void *)r1.output, r1.output_len + 1);

    hu_tool_result_t r2 = update_canvas(&alloc, &tool, id2, "function App() {}");
    HU_ASSERT_TRUE(r2.success);
    if (r2.output_owned && r2.output)
        alloc.free(alloc.ctx, (void *)r2.output, r2.output_len + 1);

    /* Simulate "reconnect" by querying all store state */
    hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
    HU_ASSERT_EQ(hu_canvas_store_count(store), 2u);

    hu_canvas_info_t info1, info2;
    HU_ASSERT_TRUE(hu_canvas_store_find(store, id1, &info1));
    HU_ASSERT_STR_EQ(info1.content, "<h1>Hello</h1>");
    HU_ASSERT_STR_EQ(info1.format, "html");

    HU_ASSERT_TRUE(hu_canvas_store_find(store, id2, &info2));
    HU_ASSERT_STR_EQ(info2.content, "function App() {}");
    HU_ASSERT_STR_EQ(info2.format, "react");

    alloc.free(alloc.ctx, id1, strlen(id1) + 1);
    alloc.free(alloc.ctx, id2, strlen(id2) + 1);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Version Ring Buffer Wraparound ────────────────────────────────── */

static void test_canvas_version_ring_wraparound(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = setup_tool(&alloc);

    char *id = create_canvas(&alloc, &tool, "html");
    HU_ASSERT_NOT_NULL(id);

    /* Push 40 versions (exceeds CANVAS_MAX_VERSIONS=32) to test ring wraparound */
    char buf[32];
    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "content-%d", i);
        hu_tool_result_t r = update_canvas(&alloc, &tool, id, buf);
        HU_ASSERT_TRUE(r.success);
        if (r.output_owned && r.output)
            alloc.free(alloc.ctx, (void *)r.output, r.output_len + 1);
    }

    hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_find(store, id, &info));
    HU_ASSERT_STR_EQ(info.content, "content-39");
    HU_ASSERT_EQ(info.version_seq, 40u);
    /* version_count capped at 32 */
    HU_ASSERT_EQ(info.version_count, 32u);

    /* Should still be able to undo at least once */
    hu_canvas_info_t undo_info;
    HU_ASSERT_EQ(hu_canvas_store_undo(store, id, &undo_info), HU_OK);
    HU_ASSERT_STR_EQ(undo_info.content, "content-38");

    alloc.free(alloc.ctx, id, strlen(id) + 1);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Canvas with Imports and Language Fields ────────────────────────── */

static void test_canvas_create_with_imports_and_language(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = setup_tool(&alloc);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action", hu_json_string_new(&alloc, "create", 6));
    hu_json_object_set(&alloc, args, "format", hu_json_string_new(&alloc, "react", 5));
    hu_json_object_set(&alloc, args, "language", hu_json_string_new(&alloc, "jsx", 3));

    hu_json_value_t *imports = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, imports, "lodash",
                       hu_json_string_new(&alloc, "https://esm.sh/lodash", 21));
    hu_json_object_set(&alloc, args, "imports", imports);

    hu_tool_result_t res = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &res), HU_OK);
    HU_ASSERT_TRUE(res.success);

    hu_canvas_store_t *store = hu_canvas_store_from_tool(&tool);
    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_get(store, 0, &info));
    HU_ASSERT_STR_EQ(info.format, "react");
    HU_ASSERT_STR_EQ(info.language, "jsx");
    HU_ASSERT_NOT_NULL(info.imports);
    HU_ASSERT(strstr(info.imports, "lodash") != NULL);

    if (res.output_owned && res.output)
        alloc.free(alloc.ctx, (void *)res.output, res.output_len + 1);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

/* ── Gateway RPC Handler Tests ─────────────────────────────────────── */

static void test_canvas_rpc_handlers_compile_and_run(void) {
    /* Under HU_IS_TEST, RPC handlers return mock data. Verify they work. */
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_context_t app = {0};
    app.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    char *out = NULL;
    size_t out_len = 0;

    /* canvas.list */
    hu_error_t err = cp_canvas_list(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "canvases") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);

    /* canvas.get */
    out = NULL;
    out_len = 0;
    err = cp_canvas_get(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);

    /* canvas.edit */
    out = NULL;
    out_len = 0;
    err = cp_canvas_edit(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "ok") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);

    /* canvas.undo */
    out = NULL;
    out_len = 0;
    err = cp_canvas_undo(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);

    /* canvas.redo */
    out = NULL;
    out_len = 0;
    err = cp_canvas_redo(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);

    hu_json_free(&alloc, root);
}

void run_canvas_e2e_tests(void) {
    HU_TEST_SUITE("canvas_e2e");
    HU_RUN_TEST(test_version_history_create_update_undo_redo);
    HU_RUN_TEST(test_canvas_store_list_all);
    HU_RUN_TEST(test_canvas_user_edit_sets_pending);
    HU_RUN_TEST(test_canvas_format_expansion_all_types);
    HU_RUN_TEST(test_canvas_reconnect_state_matches);
    HU_RUN_TEST(test_canvas_version_ring_wraparound);
    HU_RUN_TEST(test_canvas_create_with_imports_and_language);
    HU_RUN_TEST(test_canvas_rpc_handlers_compile_and_run);
}
