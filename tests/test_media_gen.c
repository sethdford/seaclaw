#include "human/core/json.h"
#include "human/core/string.h"
#include "human/core/vertex_auth.h"
#include "human/tools/media_image.h"
#include "human/tools/media_video.h"
#include "human/tools/media_gif.h"
#include "test_framework.h"
#include <string.h>

/* ── vertex_auth tests ──────────────────────────────────────────────────── */

static void vertex_auth_load_adc_mock_succeeds(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_vertex_auth_t auth;
    memset(&auth, 0, sizeof(auth));
    HU_ASSERT_EQ(hu_vertex_auth_load_adc(&auth, &alloc), HU_OK);
    HU_ASSERT_NOT_NULL(auth.access_token);
    HU_ASSERT(auth.access_token_len > 0);
    hu_vertex_auth_free(&auth);
}

static void vertex_auth_ensure_token_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_vertex_auth_t auth;
    memset(&auth, 0, sizeof(auth));
    HU_ASSERT_EQ(hu_vertex_auth_load_adc(&auth, &alloc), HU_OK);
    HU_ASSERT_EQ(hu_vertex_auth_ensure_token(&auth, &alloc), HU_OK);
    HU_ASSERT_NOT_NULL(auth.access_token);
    hu_vertex_auth_free(&auth);
}

static void vertex_auth_get_bearer_formats(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_vertex_auth_t auth;
    memset(&auth, 0, sizeof(auth));
    HU_ASSERT_EQ(hu_vertex_auth_load_adc(&auth, &alloc), HU_OK);
    char buf[256];
    HU_ASSERT_EQ(hu_vertex_auth_get_bearer(&auth, buf, sizeof(buf)), HU_OK);
    HU_ASSERT(strncmp(buf, "Bearer ", 7) == 0);
    HU_ASSERT(strlen(buf) > 7);
    hu_vertex_auth_free(&auth);
}

static void vertex_auth_get_bearer_small_buf_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_vertex_auth_t auth;
    memset(&auth, 0, sizeof(auth));
    HU_ASSERT_EQ(hu_vertex_auth_load_adc(&auth, &alloc), HU_OK);
    char buf[8];
    HU_ASSERT_EQ(hu_vertex_auth_get_bearer(&auth, buf, sizeof(buf)), HU_ERR_INVALID_ARGUMENT);
    hu_vertex_auth_free(&auth);
}

static void vertex_auth_null_args_rejected(void) {
    HU_ASSERT_EQ(hu_vertex_auth_load_adc(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_vertex_auth_ensure_token(NULL, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void vertex_auth_free_null_safe(void) {
    hu_vertex_auth_t auth;
    memset(&auth, 0, sizeof(auth));
    hu_vertex_auth_free(&auth);
    hu_vertex_auth_free(NULL);
}

/* ── media_image tool tests ─────────────────────────────────────────────── */

static void media_image_create_registers_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_image_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "media_image");
}

static void media_image_has_description(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_image_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable->description(tool.ctx));
    HU_ASSERT(strlen(tool.vtable->description(tool.ctx)) > 10);
}

static void media_image_has_parameters_json(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_image_create(&alloc, &tool), HU_OK);
    const char *params = tool.vtable->parameters_json(tool.ctx);
    HU_ASSERT_NOT_NULL(params);
    hu_json_value_t *parsed = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc, params, strlen(params), &parsed), HU_OK);
    HU_ASSERT_NOT_NULL(parsed);
    hu_json_free(&alloc, parsed);
}

static void media_image_execute_mock_returns_path(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_image_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"prompt\":\"a sunset over mountains\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.media_path);
    HU_ASSERT(result.media_path_len > 0);
    HU_ASSERT(strstr(result.media_path, "/tmp/human_img_mock_") != NULL);
    HU_ASSERT(strstr(result.media_path, ".png") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void media_image_missing_prompt_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_image_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"model\":\"nano_banana\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void media_image_invalid_model_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_image_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"prompt\":\"test\",\"model\":\"nonexistent\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void media_image_invalid_aspect_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_image_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"prompt\":\"test\",\"aspect_ratio\":\"99:99\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

/* ── media_video tool tests ─────────────────────────────────────────────── */

static void media_video_create_registers_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_video_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "media_video");
}

static void media_video_execute_mock_returns_path(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_video_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"prompt\":\"a cat jumping\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.media_path);
    HU_ASSERT(result.media_path_len > 0);
    HU_ASSERT(strstr(result.media_path, "/tmp/human_vid_mock_") != NULL);
    HU_ASSERT(strstr(result.media_path, ".mp4") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void media_video_missing_prompt_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_video_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"duration\":4}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void media_video_invalid_model_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_video_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"prompt\":\"test\",\"model\":\"bad_model\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

/* ── media_gif tool tests ───────────────────────────────────────────────── */

static void media_gif_create_registers_name(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_gif_create(&alloc, &tool), HU_OK);
    HU_ASSERT_NOT_NULL(tool.vtable);
    HU_ASSERT_STR_EQ(tool.vtable->name(tool.ctx), "media_gif");
}

static void media_gif_execute_mock_returns_path(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_gif_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"prompt\":\"a dog dancing\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(result.success);
    HU_ASSERT_NOT_NULL(result.media_path);
    HU_ASSERT(result.media_path_len > 0);
    HU_ASSERT(strstr(result.media_path, "/tmp/human_gif_mock_") != NULL);
    HU_ASSERT(strstr(result.media_path, ".gif") != NULL);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

static void media_gif_missing_prompt_fails(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_t tool;
    memset(&tool, 0, sizeof(tool));
    HU_ASSERT_EQ(hu_media_gif_create(&alloc, &tool), HU_OK);

    hu_json_value_t *args = NULL;
    const char *json = "{\"aspect_ratio\":\"16:9\"}";
    HU_ASSERT_EQ(hu_json_parse(&alloc, json, strlen(json), &args), HU_OK);

    hu_tool_result_t result = {0};
    HU_ASSERT_EQ(tool.vtable->execute(tool.ctx, &alloc, args, &result), HU_OK);
    HU_ASSERT(!result.success);

    hu_tool_result_free(&alloc, &result);
    hu_json_free(&alloc, args);
}

/* ── tool_result media_path lifecycle ───────────────────────────────────── */

static void tool_result_ok_with_media_sets_fields(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *output = hu_strndup(&alloc, "Generated image", 15);
    char *path = hu_strndup(&alloc, "/tmp/human_img_test.png", 23);
    HU_ASSERT_NOT_NULL(output);
    HU_ASSERT_NOT_NULL(path);

    hu_tool_result_t r = hu_tool_result_ok_with_media(output, 15, path, 23);
    HU_ASSERT(r.success);
    HU_ASSERT(r.output_owned);
    HU_ASSERT(r.media_path_owned);
    HU_ASSERT_STR_EQ(r.media_path, "/tmp/human_img_test.png");
    HU_ASSERT_EQ(r.media_path_len, (size_t)23);

    hu_tool_result_free(&alloc, &r);
    HU_ASSERT(r.media_path == NULL);
    HU_ASSERT(r.output == NULL);
}

static void tool_result_free_null_media_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_tool_result_t r = hu_tool_result_ok("test", 4);
    HU_ASSERT(r.media_path == NULL);
    HU_ASSERT(!r.media_path_owned);
    hu_tool_result_free(&alloc, &r);
}

/* ── registration ───────────────────────────────────────────────────────── */

void run_media_gen_tests(void) {
    HU_TEST_SUITE("media_gen");

    HU_RUN_TEST(vertex_auth_load_adc_mock_succeeds);
    HU_RUN_TEST(vertex_auth_ensure_token_mock);
    HU_RUN_TEST(vertex_auth_get_bearer_formats);
    HU_RUN_TEST(vertex_auth_get_bearer_small_buf_fails);
    HU_RUN_TEST(vertex_auth_null_args_rejected);
    HU_RUN_TEST(vertex_auth_free_null_safe);

    HU_RUN_TEST(media_image_create_registers_name);
    HU_RUN_TEST(media_image_has_description);
    HU_RUN_TEST(media_image_has_parameters_json);
    HU_RUN_TEST(media_image_execute_mock_returns_path);
    HU_RUN_TEST(media_image_missing_prompt_fails);
    HU_RUN_TEST(media_image_invalid_model_fails);
    HU_RUN_TEST(media_image_invalid_aspect_fails);

    HU_RUN_TEST(media_video_create_registers_name);
    HU_RUN_TEST(media_video_execute_mock_returns_path);
    HU_RUN_TEST(media_video_missing_prompt_fails);
    HU_RUN_TEST(media_video_invalid_model_fails);

    HU_RUN_TEST(media_gif_create_registers_name);
    HU_RUN_TEST(media_gif_execute_mock_returns_path);
    HU_RUN_TEST(media_gif_missing_prompt_fails);

    HU_RUN_TEST(tool_result_ok_with_media_sets_fields);
    HU_RUN_TEST(tool_result_free_null_media_safe);
}
