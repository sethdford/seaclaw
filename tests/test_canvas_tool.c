#include "human/core/allocator.h"
#include "human/core/json.h"
#include "human/tool.h"
#include "human/tools/canvas.h"
#include "test_framework.h"
#include <string.h>

static void test_canvas_tool_create_and_find(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    HU_ASSERT_EQ(hu_canvas_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "canvas");

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action",
                       hu_json_string_new(&alloc, "create", 6));
    hu_json_object_set(&alloc, args, "title",
                       hu_json_string_new(&alloc, "Test Doc", 8));
    hu_json_object_set(&alloc, args, "content",
                       hu_json_string_new(&alloc, "<p>Hello</p>", 12));

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT_TRUE(result.success);
    HU_ASSERT_NOT_NULL(result.output);
    HU_ASSERT(strstr(result.output, "\"id\"") != NULL);
    HU_ASSERT(strstr(result.output, "\"created\":true") != NULL);
    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_canvas_tool_edit(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_canvas_create(&alloc, &tool);

    hu_json_value_t *create_args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, create_args, "action",
                       hu_json_string_new(&alloc, "create", 6));
    hu_json_object_set(&alloc, create_args, "title",
                       hu_json_string_new(&alloc, "Editable", 8));
    hu_json_object_set(&alloc, create_args, "content",
                       hu_json_string_new(&alloc, "original", 8));

    hu_tool_result_t cr = {0};
    tool.vtable->execute(tool.ctx, &alloc, create_args, &cr);
    HU_ASSERT_TRUE(cr.success);

    char id_buf[64] = {0};
    const char *id_start = strstr(cr.output, "\"id\":\"");
    if (id_start) {
        id_start += 6;
        const char *id_end = strchr(id_start, '"');
        if (id_end && (size_t)(id_end - id_start) < sizeof(id_buf))
            memcpy(id_buf, id_start, (size_t)(id_end - id_start));
    }
    hu_tool_result_free(&alloc, &cr);
    hu_json_free(&alloc, create_args);

    hu_json_value_t *edit_args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, edit_args, "action",
                       hu_json_string_new(&alloc, "edit", 4));
    hu_json_object_set(&alloc, edit_args, "id",
                       hu_json_string_new(&alloc, id_buf, strlen(id_buf)));
    hu_json_object_set(&alloc, edit_args, "content",
                       hu_json_string_new(&alloc, "updated", 7));

    hu_tool_result_t er = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, edit_args, &er), HU_OK);
    HU_ASSERT_TRUE(er.success);
    HU_ASSERT(strstr(er.output, "updated") != NULL);
    hu_tool_result_free(&alloc, &er);
    hu_json_free(&alloc, edit_args);

    hu_json_value_t *view_args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, view_args, "action",
                       hu_json_string_new(&alloc, "view", 4));
    hu_json_object_set(&alloc, view_args, "id",
                       hu_json_string_new(&alloc, id_buf, strlen(id_buf)));

    hu_tool_result_t vr = {0};
    tool.vtable->execute(tool.ctx, &alloc, view_args, &vr);
    HU_ASSERT_TRUE(vr.success);
    HU_ASSERT(strstr(vr.output, "updated") != NULL);
    hu_tool_result_free(&alloc, &vr);
    hu_json_free(&alloc, view_args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_canvas_tool_delete(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_canvas_create(&alloc, &tool);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action",
                       hu_json_string_new(&alloc, "create", 6));
    hu_json_object_set(&alloc, args, "content",
                       hu_json_string_new(&alloc, "temp", 4));
    hu_tool_result_t cr = {0};
    tool.vtable->execute(tool.ctx, &alloc, args, &cr);

    char id_buf[64] = {0};
    const char *id_start = strstr(cr.output, "\"id\":\"");
    if (id_start) {
        id_start += 6;
        const char *id_end = strchr(id_start, '"');
        if (id_end && (size_t)(id_end - id_start) < sizeof(id_buf))
            memcpy(id_buf, id_start, (size_t)(id_end - id_start));
    }
    hu_tool_result_free(&alloc, &cr);
    hu_json_free(&alloc, args);

    hu_json_value_t *del_args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, del_args, "action",
                       hu_json_string_new(&alloc, "delete", 6));
    hu_json_object_set(&alloc, del_args, "id",
                       hu_json_string_new(&alloc, id_buf, strlen(id_buf)));

    hu_tool_result_t dr = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, del_args, &dr), HU_OK);
    HU_ASSERT_TRUE(dr.success);
    HU_ASSERT(strstr(dr.output, "deleted") != NULL);
    hu_tool_result_free(&alloc, &dr);
    hu_json_free(&alloc, del_args);

    hu_json_value_t *view_args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, view_args, "action",
                       hu_json_string_new(&alloc, "view", 4));
    hu_json_object_set(&alloc, view_args, "id",
                       hu_json_string_new(&alloc, id_buf, strlen(id_buf)));
    hu_tool_result_t vr = {0};
    tool.vtable->execute(tool.ctx, &alloc, view_args, &vr);
    HU_ASSERT_FALSE(vr.success);
    hu_tool_result_free(&alloc, &vr);
    hu_json_free(&alloc, view_args);

    tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_canvas_tool_missing_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_canvas_create(&alloc, &tool);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_FALSE(result.success);
    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_canvas_tool_unknown_action(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_canvas_create(&alloc, &tool);

    hu_json_value_t *args = hu_json_object_new(&alloc);
    hu_json_object_set(&alloc, args, "action",
                       hu_json_string_new(&alloc, "explode", 7));
    hu_tool_result_t result = {0};
    tool.vtable->execute(tool.ctx, &alloc, args, &result);
    HU_ASSERT_FALSE(result.success);
    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
    tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_canvas_tool_vtable_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_canvas_create(&alloc, &tool);

    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "canvas");
    HU_ASSERT_NOT_NULL(tool.vtable->description(tool.ctx));
    HU_ASSERT_NOT_NULL(tool.vtable->parameters_json(tool.ctx));
    HU_ASSERT(strstr(tool.vtable->parameters_json(tool.ctx), "action") != NULL);
    HU_ASSERT_NOT_NULL(tool.vtable->execute);
    HU_ASSERT_NOT_NULL(tool.vtable->deinit);

    tool.vtable->deinit(tool.ctx, &alloc);
}

static void test_canvas_tool_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool = {0};
    hu_canvas_create(&alloc, &tool);

    hu_tool_result_t result = {0};
    hu_error_t err = tool.vtable->execute(tool.ctx, &alloc, NULL, &result);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    hu_tool_result_free(&alloc, &result);
    tool.vtable->deinit(tool.ctx, &alloc);
}

void run_canvas_tool_tests(void) {
    HU_TEST_SUITE("CanvasTool");
    HU_RUN_TEST(test_canvas_tool_create_and_find);
    HU_RUN_TEST(test_canvas_tool_edit);
    HU_RUN_TEST(test_canvas_tool_delete);
    HU_RUN_TEST(test_canvas_tool_missing_action);
    HU_RUN_TEST(test_canvas_tool_unknown_action);
    HU_RUN_TEST(test_canvas_tool_vtable_fields);
    HU_RUN_TEST(test_canvas_tool_null_args);
}
