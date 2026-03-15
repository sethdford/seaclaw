#include "human/core/allocator.h"
#include "human/multimodal.h"
#include "test_framework.h"
#include <string.h>

static void test_multimodal_detect_image(void) {
    hu_modality_t out = HU_MODALITY_TEXT;
    hu_error_t err = hu_multimodal_detect_type("image/png", 9, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(out, HU_MODALITY_IMAGE);
}

static void test_multimodal_detect_audio(void) {
    hu_modality_t out = HU_MODALITY_TEXT;
    hu_error_t err = hu_multimodal_detect_type("audio/mp3", 9, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(out, HU_MODALITY_AUDIO);
}

static void test_multimodal_detect_video(void) {
    hu_modality_t out = HU_MODALITY_TEXT;
    hu_error_t err = hu_multimodal_detect_type("video/mp4", 9, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(out, HU_MODALITY_VIDEO);
}

static void test_multimodal_detect_text(void) {
    hu_modality_t out = HU_MODALITY_IMAGE;
    hu_error_t err = hu_multimodal_detect_type("text/plain", 10, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(out, HU_MODALITY_TEXT);
}

static void test_multimodal_needs_fallback_no_image(void) {
    hu_provider_capabilities_t caps = {
        .supports_image = false,
        .supports_audio = true,
        .supports_video = false,
        .supports_streaming = false,
    };
    bool needs_fallback = false;
    hu_error_t err = hu_multimodal_needs_fallback(&caps, HU_MODALITY_IMAGE, &needs_fallback);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(needs_fallback);
}

static void test_multimodal_modality_name(void) {
    HU_ASSERT_STR_EQ(hu_modality_name(HU_MODALITY_TEXT), "text");
    HU_ASSERT_STR_EQ(hu_modality_name(HU_MODALITY_IMAGE), "image");
    HU_ASSERT_STR_EQ(hu_modality_name(HU_MODALITY_AUDIO), "audio");
    HU_ASSERT_STR_EQ(hu_modality_name(HU_MODALITY_VIDEO), "video");
}

static void test_multimodal_null_args_returns_error(void) {
    hu_modality_t out;
    HU_ASSERT_EQ(hu_multimodal_detect_type(NULL, 9, &out), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_detect_type("image/png", 9, NULL), HU_ERR_INVALID_ARGUMENT);

    hu_provider_capabilities_t caps = {0};
    bool needs = false;
    HU_ASSERT_EQ(hu_multimodal_needs_fallback(NULL, HU_MODALITY_IMAGE, &needs),
                 HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_multimodal_needs_fallback(&caps, HU_MODALITY_IMAGE, NULL),
                 HU_ERR_INVALID_ARGUMENT);
}

void run_multimodal_pipeline_tests(void) {
    HU_TEST_SUITE("MultimodalPipeline");
    HU_RUN_TEST(test_multimodal_detect_image);
    HU_RUN_TEST(test_multimodal_detect_audio);
    HU_RUN_TEST(test_multimodal_detect_video);
    HU_RUN_TEST(test_multimodal_detect_text);
    HU_RUN_TEST(test_multimodal_needs_fallback_no_image);
    HU_RUN_TEST(test_multimodal_modality_name);
    HU_RUN_TEST(test_multimodal_null_args_returns_error);
}
