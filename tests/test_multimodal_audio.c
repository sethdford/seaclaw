#include "human/core/allocator.h"
#include "human/multimodal.h"
#include "human/multimodal/audio.h"
#include "human/provider.h"
#include "test_framework.h"
#include <string.h>

static const char *stub_provider_name(void *ctx) {
    (void)ctx;
    return "openai";
}

static hu_provider_vtable_t stub_audio_provider_vtable = {
    .get_name = stub_provider_name,
};

static void multimodal_process_audio_returns_mock_transcription(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_audio_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err = hu_multimodal_process_audio(&alloc, "clip.wav", 8, &provider, NULL, 0, &text,
                                                 &text_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(text);
    HU_ASSERT_STR_EQ(text, "Mock audio transcription");
    HU_ASSERT_EQ(text_len, strlen("Mock audio transcription"));
    alloc.free(alloc.ctx, text, text_len + 1);
}

static void multimodal_process_audio_rejects_bad_extension(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_audio_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err = hu_multimodal_process_audio(&alloc, "x.txt", 5, &provider, NULL, 0, &text,
                                                 &text_len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NULL(text);
}

static void multimodal_process_audio_null_args(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_audio_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    HU_ASSERT_EQ(hu_multimodal_process_audio(NULL, "a.wav", 5, &provider, NULL, 0, &text, &text_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_process_audio(&alloc, NULL, 5, &provider, NULL, 0, &text, &text_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_process_audio(&alloc, "a.wav", 5, NULL, NULL, 0, &text, &text_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_process_audio(&alloc, "a.wav", 5, &provider, NULL, 0, NULL, &text_len),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_process_audio(&alloc, "a.wav", 5, &provider, NULL, 0, &text, NULL),
                 HU_ERR_INVALID_ARGUMENT);
    provider.vtable = NULL;
    HU_ASSERT_EQ(hu_multimodal_process_audio(&alloc, "a.wav", 5, &provider, NULL, 0, &text, &text_len),
                 HU_ERR_INVALID_ARGUMENT);
}

static void multimodal_route_local_media_dispatches_audio(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_audio_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err = hu_multimodal_route_local_media(&alloc, "track.m4a", 9, &provider, NULL, 0,
                                                     &text, &text_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(text);
    HU_ASSERT_STR_EQ(text, "Mock audio transcription");
    alloc.free(alloc.ctx, text, text_len + 1);
}

static void multimodal_route_local_media_unknown_extension(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_provider_t provider = {.ctx = NULL, .vtable = &stub_audio_provider_vtable};
    char *text = NULL;
    size_t text_len = 0;
    hu_error_t err =
        hu_multimodal_route_local_media(&alloc, "file.bin", 8, &provider, NULL, 0, &text, &text_len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_NULL(text);
}

void run_multimodal_audio_tests(void) {
    HU_TEST_SUITE("MultimodalAudio");
    HU_RUN_TEST(multimodal_process_audio_returns_mock_transcription);
    HU_RUN_TEST(multimodal_process_audio_rejects_bad_extension);
    HU_RUN_TEST(multimodal_process_audio_null_args);
    HU_RUN_TEST(multimodal_route_local_media_dispatches_audio);
    HU_RUN_TEST(multimodal_route_local_media_unknown_extension);
}
