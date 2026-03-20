#include "human/core/allocator.h"
#include "human/multimodal.h"
#include "human/multimodal/video.h"
#include "human/provider.h"
#include "test_framework.h"
#include <string.h>

static const char *stub_video_provider_name(void *ctx) {
    (void)ctx;
    return "openai";
}

static hu_provider_vtable_t stub_video_provider_vtable = {
    .get_name = stub_video_provider_name,
};

static void multimodal_process_video_returns_mock_description(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_video_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err = hu_multimodal_process_video(&alloc, "clip.mp4", 8, &provider, NULL, 0, &text,
                                                 &text_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(text);
    HU_ASSERT_STR_EQ(text, "Mock video description: person walking in park");
    alloc.free(alloc.ctx, text, text_len + 1);
}

static void multimodal_process_video_rejects_bad_extension(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_video_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err = hu_multimodal_process_video(&alloc, "x.wav", 5, &provider, NULL, 0, &text,
                                                 &text_len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NULL(text);
}

static void multimodal_process_video_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_video_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    HU_ASSERT_EQ(hu_multimodal_process_video(NULL, "a.mp4", 5, &provider, NULL, 0, &text, &text_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(
        hu_multimodal_process_video(&alloc, NULL, 5, &provider, NULL, 0, &text, &text_len),
        HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_process_video(&alloc, "a.mp4", 5, NULL, NULL, 0, &text, &text_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(
        hu_multimodal_process_video(&alloc, "a.mp4", 5, &provider, NULL, 0, NULL, &text_len),
        HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_process_video(&alloc, "a.mp4", 5, &provider, NULL, 0, &text, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    provider.vtable = NULL;
    HU_ASSERT_EQ(
        hu_multimodal_process_video(&alloc, "a.mp4", 5, &provider, NULL, 0, &text, &text_len),
        HU_ERR_INVALID_ARGUMENT);
}

static void multimodal_route_local_media_dispatches_video(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_video_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err = hu_multimodal_route_local_media(&alloc, "take.webm", 9, &provider, NULL, 0,
                                                     &text, &text_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(text);
    HU_ASSERT_STR_EQ(text, "Mock video description: person walking in park");
    alloc.free(alloc.ctx, text, text_len + 1);
}

void run_multimodal_video_tests(void) {
    HU_TEST_SUITE("MultimodalVideo");
    HU_RUN_TEST(multimodal_process_video_returns_mock_description);
    HU_RUN_TEST(multimodal_process_video_rejects_bad_extension);
    HU_RUN_TEST(multimodal_process_video_null_args);
    HU_RUN_TEST(multimodal_route_local_media_dispatches_video);
}
