#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/gateway/control_protocol.h"
#include "test_framework.h"

#ifdef HU_GATEWAY_POSIX

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

static void cp_canvas_list_rejects_null_pointers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(cp_canvas_list(NULL, NULL, NULL, NULL, NULL, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_list(&alloc, NULL, NULL, NULL, NULL, NULL, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_list(&alloc, NULL, NULL, NULL, NULL, &out, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void cp_canvas_get_rejects_null_pointers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(cp_canvas_get(NULL, NULL, NULL, NULL, NULL, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_get(&alloc, NULL, NULL, NULL, NULL, NULL, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_get(&alloc, NULL, NULL, NULL, NULL, &out, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void cp_canvas_edit_rejects_null_pointers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(cp_canvas_edit(NULL, NULL, NULL, NULL, NULL, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_edit(&alloc, NULL, NULL, NULL, NULL, NULL, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_edit(&alloc, NULL, NULL, NULL, NULL, &out, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void cp_canvas_undo_rejects_null_pointers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(cp_canvas_undo(NULL, NULL, NULL, NULL, NULL, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_undo(&alloc, NULL, NULL, NULL, NULL, NULL, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_undo(&alloc, NULL, NULL, NULL, NULL, &out, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void cp_canvas_redo_rejects_null_pointers(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(cp_canvas_redo(NULL, NULL, NULL, NULL, NULL, &out, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_redo(&alloc, NULL, NULL, NULL, NULL, NULL, &out_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(cp_canvas_redo(&alloc, NULL, NULL, NULL, NULL, &out, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

static void cp_canvas_list_ok_allocates_json_in_test_build(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_app_context_t app = {0};
    app.alloc = &alloc;

    hu_json_value_t *root = hu_json_object_new(&alloc);
    HU_ASSERT_NOT_NULL(root);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(cp_canvas_list(&alloc, &app, NULL, NULL, root, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(out_len > 0);
    alloc.free(alloc.ctx, out, out_len + 1);
    hu_json_free(&alloc, root);
}

#endif /* HU_GATEWAY_POSIX */

void run_cp_canvas_tests(void) {
    HU_TEST_SUITE("cp_canvas");
#ifdef HU_GATEWAY_POSIX
    HU_RUN_TEST(cp_canvas_list_rejects_null_pointers);
    HU_RUN_TEST(cp_canvas_get_rejects_null_pointers);
    HU_RUN_TEST(cp_canvas_edit_rejects_null_pointers);
    HU_RUN_TEST(cp_canvas_undo_rejects_null_pointers);
    HU_RUN_TEST(cp_canvas_redo_rejects_null_pointers);
    HU_RUN_TEST(cp_canvas_list_ok_allocates_json_in_test_build);
#endif
}
