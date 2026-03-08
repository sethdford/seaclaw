/* Vision module tests — image reading, context building, describe flow. */

#include "seaclaw/context/vision.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/provider.h"
#include "test_framework.h"
#include <string.h>

/* ── sc_vision_build_context ───────────────────────────────────────────── */

static void vision_build_context_with_description(void) {
    sc_allocator_t alloc = sc_system_allocator();
    const char *desc = "a sunset over the ocean";
    size_t desc_len = strlen(desc);
    size_t out_len = 0;
    char *ctx = sc_vision_build_context(&alloc, desc, desc_len, &out_len);
    SC_ASSERT_NOT_NULL(ctx);
    SC_ASSERT_TRUE(out_len > 0);
    SC_ASSERT_TRUE(strstr(ctx, "sunset") != NULL);
    SC_ASSERT_TRUE(strstr(ctx, "Image Context") != NULL);
    alloc.free(alloc.ctx, ctx, out_len + 1);
}

static void vision_build_context_null_description(void) {
    sc_allocator_t alloc = sc_system_allocator();
    size_t out_len = 99;
    char *ctx = sc_vision_build_context(&alloc, NULL, 0, &out_len);
    SC_ASSERT_NULL(ctx);
    SC_ASSERT_EQ(out_len, 0u);
}

static void vision_build_context_empty_description(void) {
    sc_allocator_t alloc = sc_system_allocator();
    size_t out_len = 99;
    char *ctx = sc_vision_build_context(&alloc, "", 0, &out_len);
    SC_ASSERT_NULL(ctx);
    SC_ASSERT_EQ(out_len, 0u);
}

/* ── sc_vision_read_image ───────────────────────────────────────────────── */

static void vision_read_image_null_path(void) {
    sc_allocator_t alloc = sc_system_allocator();
    char *base64 = NULL;
    size_t base64_len = 0;
    char *media_type = NULL;
    size_t media_type_len = 0;
    sc_error_t err = sc_vision_read_image(&alloc, NULL, 0, &base64, &base64_len,
                                          &media_type, &media_type_len);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
    SC_ASSERT_NULL(base64);
    SC_ASSERT_NULL(media_type);
}

static void vision_read_image_mock_in_test(void) {
    /* In SC_IS_TEST, sc_vision_read_image returns mock base64 without file I/O */
    sc_allocator_t alloc = sc_system_allocator();
    char *base64 = NULL;
    size_t base64_len = 0;
    char *media_type = NULL;
    size_t media_type_len = 0;
    sc_error_t err = sc_vision_read_image(&alloc, "/nonexistent/path.png", 20,
                                          &base64, &base64_len,
                                          &media_type, &media_type_len);
    SC_ASSERT_EQ(err, SC_OK);
    SC_ASSERT_NOT_NULL(base64);
    SC_ASSERT_TRUE(base64_len > 0);
    SC_ASSERT_NOT_NULL(media_type);
    SC_ASSERT_STR_EQ(media_type, "image/png");
    alloc.free(alloc.ctx, base64, base64_len + 1);
    alloc.free(alloc.ctx, media_type, media_type_len + 1);
}

/* ── sc_vision_describe_image (no vision support) ───────────────────────── */

static bool no_vision_supports_vision(void *ctx) {
    (void)ctx;
    return false;
}

static const char *no_vision_get_name(void *ctx) {
    (void)ctx;
    return "no_vision";
}

static sc_provider_vtable_t no_vision_vtable = {
    .chat_with_system = NULL,
    .chat = NULL,
    .supports_native_tools = NULL,
    .get_name = no_vision_get_name,
    .deinit = NULL,
    .supports_vision = no_vision_supports_vision,
};

static void vision_describe_image_no_vision_support(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t provider = {
        .ctx = NULL,
        .vtable = &no_vision_vtable,
    };
    char *desc = NULL;
    size_t desc_len = 0;
    sc_error_t err = sc_vision_describe_image(
        &alloc, &provider, "/tmp/test.png", 11, "gpt-4o", 6, &desc, &desc_len);
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
    SC_ASSERT_NULL(desc);
}

/* Provider with supports_vision = NULL (optional field) */
static sc_provider_vtable_t no_vision_vtable_null = {
    .chat_with_system = NULL,
    .chat = NULL,
    .supports_native_tools = NULL,
    .get_name = no_vision_get_name,
    .deinit = NULL,
    .supports_vision = NULL,
};

static void vision_describe_image_no_vision_vtable_null(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t provider = {
        .ctx = NULL,
        .vtable = &no_vision_vtable_null,
    };
    char *desc = NULL;
    size_t desc_len = 0;
    sc_error_t err = sc_vision_describe_image(
        &alloc, &provider, "/tmp/test.png", 11, "gpt-4o", 6, &desc, &desc_len);
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
    SC_ASSERT_NULL(desc);
}

static void vision_describe_image_null_vtable(void) {
    sc_allocator_t alloc = sc_system_allocator();
    sc_provider_t provider = {.ctx = NULL, .vtable = NULL};
    char *desc = NULL;
    size_t desc_len = 0;
    sc_error_t err = sc_vision_describe_image(&alloc, &provider, "test.png", 8, "gpt-4o", 6,
                                              &desc, &desc_len);
    SC_ASSERT_EQ(err, SC_ERR_NOT_SUPPORTED);
    SC_ASSERT_NULL(desc);
}

/* ── Suite ──────────────────────────────────────────────────────────────── */

void run_vision_tests(void) {
    SC_TEST_SUITE("vision");
    SC_RUN_TEST(vision_build_context_with_description);
    SC_RUN_TEST(vision_build_context_null_description);
    SC_RUN_TEST(vision_build_context_empty_description);
    SC_RUN_TEST(vision_read_image_null_path);
    SC_RUN_TEST(vision_read_image_mock_in_test);
    SC_RUN_TEST(vision_describe_image_no_vision_support);
    SC_RUN_TEST(vision_describe_image_no_vision_vtable_null);
    SC_RUN_TEST(vision_describe_image_null_vtable);
}
