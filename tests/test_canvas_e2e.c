#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tools/canvas_store.h"
#include "human/gateway/control_protocol.h"
#include "test_framework.h"
#include <string.h>

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

static void test_store_version_history_undo_redo(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);
    HU_ASSERT_NOT_NULL(store);

    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_0", "html", NULL, NULL, "Test", "v0"), HU_OK);

    HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "cv_0", "v1"), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "cv_0", "v2"), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "cv_0", "v3"), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "cv_0", "v4"), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "cv_0", "v5"), HU_OK);

    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_find(store, "cv_0", &info));
    HU_ASSERT_STR_EQ(info.content, "v5");
    HU_ASSERT_EQ(info.version_seq, 5u);

    hu_canvas_info_t undo1;
    HU_ASSERT_EQ(hu_canvas_store_undo(store, "cv_0", &undo1), HU_OK);
    HU_ASSERT_STR_EQ(undo1.content, "v4");

    hu_canvas_info_t undo2;
    HU_ASSERT_EQ(hu_canvas_store_undo(store, "cv_0", &undo2), HU_OK);
    HU_ASSERT_STR_EQ(undo2.content, "v3");

    hu_canvas_info_t redo1;
    HU_ASSERT_EQ(hu_canvas_store_redo(store, "cv_0", &redo1), HU_OK);
    HU_ASSERT_STR_EQ(redo1.content, "v4");

    hu_canvas_store_destroy(store);
}

static void test_store_list_all(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);

    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_0", "html", NULL, NULL, "A", ""), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_1", "react", NULL, NULL, "B", ""), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_2", "mermaid", NULL, NULL, "C", ""), HU_OK);

    HU_ASSERT_EQ(hu_canvas_store_count(store), 3u);

    for (size_t i = 0; i < 3; i++) {
        hu_canvas_info_t info;
        HU_ASSERT_TRUE(hu_canvas_store_get(store, i, &info));
        HU_ASSERT_NOT_NULL(info.canvas_id);
    }

    hu_canvas_store_destroy(store);
}

static void test_store_user_edit_sets_pending(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);

    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_0", "html", NULL, NULL, "T", "<p>init</p>"),
                 HU_OK);

    HU_ASSERT_EQ(hu_canvas_store_edit(store, "cv_0", "<p>user edit</p>"), HU_OK);
    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_find(store, "cv_0", &info));
    HU_ASSERT_TRUE(info.user_edit_pending);

    HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "cv_0", "<p>agent</p>"), HU_OK);
    HU_ASSERT_TRUE(hu_canvas_store_find(store, "cv_0", &info));
    HU_ASSERT_STR_EQ(info.content, "<p>agent</p>");

    hu_canvas_store_destroy(store);
}

static void test_store_format_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);

    const char *formats[] = {"html", "svg", "mockup", "react", "mermaid", "markdown", "code"};
    size_t nfmt = sizeof(formats) / sizeof(formats[0]);

    for (size_t i = 0; i < nfmt; i++) {
        char id[16];
        snprintf(id, sizeof(id), "cv_%zu", i);
        HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, id, formats[i], NULL, NULL, NULL, ""), HU_OK);
    }

    HU_ASSERT_EQ(hu_canvas_store_count(store), nfmt);

    for (size_t i = 0; i < nfmt; i++) {
        hu_canvas_info_t info;
        HU_ASSERT_TRUE(hu_canvas_store_get(store, i, &info));
        HU_ASSERT_STR_EQ(info.format, formats[i]);
    }

    hu_canvas_store_destroy(store);
}

static void test_store_remove_canvas(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);

    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_a", "html", NULL, NULL, "A", ""), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_b", "react", NULL, NULL, "B", ""), HU_OK);
    HU_ASSERT_EQ(hu_canvas_store_count(store), 2u);

    hu_canvas_store_remove_canvas(store, "cv_a");
    HU_ASSERT_EQ(hu_canvas_store_count(store), 1u);
    HU_ASSERT_FALSE(hu_canvas_store_find(store, "cv_a", NULL));
    HU_ASSERT_TRUE(hu_canvas_store_find(store, "cv_b", NULL));

    hu_canvas_store_destroy(store);
}

static void test_store_version_ring_wraparound(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);

    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_0", "html", NULL, NULL, NULL, "init"), HU_OK);

    char buf[32];
    for (int i = 0; i < 40; i++) {
        snprintf(buf, sizeof(buf), "content-%d", i);
        HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "cv_0", buf), HU_OK);
    }

    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_find(store, "cv_0", &info));
    HU_ASSERT_STR_EQ(info.content, "content-39");
    HU_ASSERT_EQ(info.version_seq, 40u);
    HU_ASSERT_EQ(info.version_count, 32u);

    hu_canvas_info_t undo_info;
    HU_ASSERT_EQ(hu_canvas_store_undo(store, "cv_0", &undo_info), HU_OK);
    HU_ASSERT_STR_EQ(undo_info.content, "content-38");

    hu_canvas_store_destroy(store);
}

static void test_store_imports_and_language(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);

    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, "cv_0", "react",
                                            "{\"lodash\":\"https://esm.sh/lodash\"}", "jsx",
                                            "Counter", ""), HU_OK);

    hu_canvas_info_t info;
    HU_ASSERT_TRUE(hu_canvas_store_get(store, 0, &info));
    HU_ASSERT_STR_EQ(info.format, "react");
    HU_ASSERT_STR_EQ(info.language, "jsx");
    HU_ASSERT_NOT_NULL(info.imports);
    HU_ASSERT(strstr(info.imports, "lodash") != NULL);

    hu_canvas_store_destroy(store);
}

static void test_store_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    HU_ASSERT_NULL(hu_canvas_store_create(NULL));
    HU_ASSERT_EQ(hu_canvas_store_count(NULL), 0u);

    hu_canvas_store_t *store = hu_canvas_store_create(&alloc);
    HU_ASSERT_EQ(hu_canvas_store_put_canvas(store, NULL, "html", NULL, NULL, NULL, ""), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_canvas_store_agent_update(store, "nope", "x"), HU_ERR_NOT_FOUND);
    HU_ASSERT_EQ(hu_canvas_store_edit(store, "nope", "x"), HU_ERR_NOT_FOUND);
    HU_ASSERT_EQ(hu_canvas_store_undo(store, "nope", NULL), HU_ERR_NOT_FOUND);
    HU_ASSERT_EQ(hu_canvas_store_redo(store, "nope", NULL), HU_ERR_NOT_FOUND);

    hu_canvas_store_destroy(store);
}

static void test_rpc_handlers_mock_mode(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_context_t app = {0};
    app.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    char *out = NULL;
    size_t out_len = 0;

    hu_error_t err = cp_canvas_list(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "canvases") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);

    out = NULL;
    out_len = 0;
    err = cp_canvas_get(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);

    out = NULL;
    out_len = 0;
    err = cp_canvas_edit(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "ok") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);

    out = NULL;
    out_len = 0;
    err = cp_canvas_undo(&alloc, &app, NULL, NULL, root, &out, &out_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);

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
    HU_RUN_TEST(test_store_version_history_undo_redo);
    HU_RUN_TEST(test_store_list_all);
    HU_RUN_TEST(test_store_user_edit_sets_pending);
    HU_RUN_TEST(test_store_format_fields);
    HU_RUN_TEST(test_store_remove_canvas);
    HU_RUN_TEST(test_store_version_ring_wraparound);
    HU_RUN_TEST(test_store_imports_and_language);
    HU_RUN_TEST(test_store_null_args);
    HU_RUN_TEST(test_rpc_handlers_mock_mode);
}
