#include "human/core/allocator.h"
#include "human/multimodal.h"
#include "human/multimodal/image.h"
#include "human/provider.h"
#include "test_framework.h"
#include <string.h>

static void test_base64_encode_empty(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    hu_error_t err = hu_multimodal_encode_base64(&alloc, "", 0, &out, &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(len, 0u);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, len + 1);
}

static void test_base64_encode_hello(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    hu_error_t err = hu_multimodal_encode_base64(&alloc, "Hello", 5, &out, &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(out, "SGVsbG8=");
    alloc.free(alloc.ctx, out, len + 1);
}

static void test_base64_encode_padding(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    hu_multimodal_encode_base64(&alloc, "a", 1, &out, &len);
    HU_ASSERT_STR_EQ(out, "YQ==");
    alloc.free(alloc.ctx, out, len + 1);

    hu_multimodal_encode_base64(&alloc, "ab", 2, &out, &len);
    HU_ASSERT_STR_EQ(out, "YWI=");
    alloc.free(alloc.ctx, out, len + 1);

    hu_multimodal_encode_base64(&alloc, "abc", 3, &out, &len);
    HU_ASSERT_STR_EQ(out, "YWJj");
    alloc.free(alloc.ctx, out, len + 1);
}

static void test_base64_decode_roundtrip_hello(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char msg[] = "Hello";
    char *enc = NULL;
    size_t enc_len = 0;
    HU_ASSERT_EQ(hu_multimodal_encode_base64(&alloc, msg, sizeof(msg) - 1, &enc, &enc_len), HU_OK);
    void *dec = NULL;
    size_t dec_len = 0;
    HU_ASSERT_EQ(hu_multimodal_decode_base64(&alloc, enc, enc_len, &dec, &dec_len), HU_OK);
    HU_ASSERT_EQ(dec_len, 5u);
    HU_ASSERT(memcmp(dec, msg, 5) == 0);
    alloc.free(alloc.ctx, enc, enc_len + 1);
    alloc.free(alloc.ctx, dec, dec_len);
}

static void test_base64_decode_invalid_char_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    void *dec = NULL;
    size_t dec_len = 0;
    hu_error_t err = hu_multimodal_decode_base64(&alloc, "SGVsbG8!", 8, &dec, &dec_len);
    HU_ASSERT(err != HU_OK);
    HU_ASSERT(dec == NULL);
}

static void test_detect_mime_png(void) {
    unsigned char png[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    HU_ASSERT_STR_EQ(hu_multimodal_detect_mime(png, 8), "image/png");
}

static void test_detect_mime_jpeg(void) {
    unsigned char jpg[] = {0xFF, 0xD8, 0xFF, 0xE0};
    HU_ASSERT_STR_EQ(hu_multimodal_detect_mime(jpg, 4), "image/jpeg");
}

static void test_detect_mime_unknown(void) {
    unsigned char unk[] = {0x00, 0x01};
    HU_ASSERT_STR_EQ(hu_multimodal_detect_mime(unk, 2), "application/octet-stream");
}

static void test_parse_markers_single(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "Look at [IMAGE:/tmp/photo.png] this image";
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err = hu_multimodal_parse_markers(&alloc, text, strlen(text), &refs, &ref_count,
                                                 &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ref_count, 1u);
    HU_ASSERT_EQ(refs[0].type, HU_IMAGE_REF_LOCAL);
    HU_ASSERT_STR_EQ(refs[0].value, "/tmp/photo.png");
    for (size_t i = 0; i < ref_count; i++)
        alloc.free(alloc.ctx, (void *)refs[i].value, refs[i].value_len + 1);
    alloc.free(alloc.ctx, refs, ref_count * sizeof(hu_image_ref_t));
    alloc.free(alloc.ctx, cleaned, cleaned_len + 1);
}

static void test_parse_markers_url(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "See [IMG:https://example.com/img.jpg]";
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err = hu_multimodal_parse_markers(&alloc, text, strlen(text), &refs, &ref_count,
                                                 &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ref_count, 1u);
    HU_ASSERT_EQ(refs[0].type, HU_IMAGE_REF_URL);
    for (size_t i = 0; i < ref_count; i++)
        alloc.free(alloc.ctx, (void *)refs[i].value, refs[i].value_len + 1);
    alloc.free(alloc.ctx, refs, ref_count * sizeof(hu_image_ref_t));
    alloc.free(alloc.ctx, cleaned, cleaned_len + 1);
}

static void test_encode_image_mock(void) {
#if defined(HU_IS_TEST) && HU_IS_TEST
    hu_allocator_t alloc = hu_system_allocator();
    char *data_uri = NULL;
    size_t len = 0;
    hu_error_t err = hu_multimodal_encode_image(&alloc, "/nonexistent.png", &data_uri, &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_STR_EQ(data_uri, "data:image/png;base64,iVBORw0KGgo=");
    alloc.free(alloc.ctx, data_uri, len + 1);
#endif
}

static void test_build_openai_image(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *json = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_multimodal_build_openai_image(&alloc, "data:image/png;base64,abc123", 28,
                                                      &json, &json_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(json);
    HU_ASSERT(json_len > 0);
    alloc.free(alloc.ctx, json, json_len + 1);
}

static void test_build_anthropic_image(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *json = NULL;
    size_t json_len = 0;
    hu_error_t err =
        hu_multimodal_build_anthropic_image(&alloc, "image/png", "abc123", 6, &json, &json_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(json);
    alloc.free(alloc.ctx, json, json_len + 1);
}

static void test_build_gemini_image(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *json = NULL;
    size_t json_len = 0;
    hu_error_t err =
        hu_multimodal_build_gemini_image(&alloc, "image/jpeg", "xyz789", 6, &json, &json_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(json);
    alloc.free(alloc.ctx, json, json_len + 1);
}

static void test_encode_image_raw(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    hu_error_t err = hu_multimodal_encode_image_raw("Hi", 2, &out);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_STR_EQ(out, "SGk=");
    alloc.free(alloc.ctx, out, strlen(out) + 1);
}

static void test_detect_mime_webp(void) {
    unsigned char webp[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'};
    HU_ASSERT_STR_EQ(hu_multimodal_detect_mime(webp, 12), "image/webp");
}

static void test_detect_mime_gif(void) {
    unsigned char gif[] = {'G', 'I', 'F', '8'};
    HU_ASSERT_STR_EQ(hu_multimodal_detect_mime(gif, 4), "image/gif");
}

static void test_detect_mime_bmp(void) {
    unsigned char bmp[] = {'B', 'M'};
    HU_ASSERT_STR_EQ(hu_multimodal_detect_mime(bmp, 2), "image/bmp");
}

static void test_parse_markers_multiple(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "[IMAGE:/tmp/a.png] and [PHOTO:/tmp/b.jpg]";
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err = hu_multimodal_parse_markers(&alloc, text, strlen(text), &refs, &ref_count,
                                                 &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ref_count, 2u);
    HU_ASSERT_STR_EQ(refs[0].value, "/tmp/a.png");
    HU_ASSERT_STR_EQ(refs[1].value, "/tmp/b.jpg");
    for (size_t i = 0; i < ref_count; i++)
        alloc.free(alloc.ctx, (void *)refs[i].value, refs[i].value_len + 1);
    alloc.free(alloc.ctx, refs, ref_count * sizeof(hu_image_ref_t));
    alloc.free(alloc.ctx, cleaned, cleaned_len + 1);
}

static void test_parse_markers_case_insensitive(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "[image:/tmp/x.png][Image:/tmp/y.png][IMAGE:/tmp/z.png]";
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err = hu_multimodal_parse_markers(&alloc, text, strlen(text), &refs, &ref_count,
                                                 &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ref_count, 3u);
    HU_ASSERT_STR_EQ(refs[0].value, "/tmp/x.png");
    HU_ASSERT_STR_EQ(refs[1].value, "/tmp/y.png");
    HU_ASSERT_STR_EQ(refs[2].value, "/tmp/z.png");
    for (size_t i = 0; i < ref_count; i++)
        alloc.free(alloc.ctx, (void *)refs[i].value, refs[i].value_len + 1);
    alloc.free(alloc.ctx, refs, ref_count * sizeof(hu_image_ref_t));
    alloc.free(alloc.ctx, cleaned, cleaned_len + 1);
}

static void test_parse_markers_img_alias(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "[IMG:https://example.com/pic.jpg]";
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err = hu_multimodal_parse_markers(&alloc, text, strlen(text), &refs, &ref_count,
                                                 &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ref_count, 1u);
    HU_ASSERT_EQ(refs[0].type, HU_IMAGE_REF_URL);
    for (size_t i = 0; i < ref_count; i++)
        alloc.free(alloc.ctx, (void *)refs[i].value, refs[i].value_len + 1);
    alloc.free(alloc.ctx, refs, ref_count * sizeof(hu_image_ref_t));
    alloc.free(alloc.ctx, cleaned, cleaned_len + 1);
}

static void test_parse_markers_data_uri(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "[IMAGE:data:image/png;base64,iVBORw0KGgo=]";
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err = hu_multimodal_parse_markers(&alloc, text, strlen(text), &refs, &ref_count,
                                                 &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ref_count, 1u);
    HU_ASSERT_EQ(refs[0].type, HU_IMAGE_REF_DATA_URI);
    for (size_t i = 0; i < ref_count; i++)
        alloc.free(alloc.ctx, (void *)refs[i].value, refs[i].value_len + 1);
    alloc.free(alloc.ctx, refs, ref_count * sizeof(hu_image_ref_t));
    alloc.free(alloc.ctx, cleaned, cleaned_len + 1);
}

static void test_parse_markers_none(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *text = "Plain text with [bracket] but no IMAGE:";
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err = hu_multimodal_parse_markers(&alloc, text, strlen(text), &refs, &ref_count,
                                                 &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_EQ(ref_count, 0u);
    HU_ASSERT_NOT_NULL(cleaned);
    if (refs)
        alloc.free(alloc.ctx, refs, 4 * sizeof(hu_image_ref_t));
    alloc.free(alloc.ctx, cleaned, cleaned_len + 1);
}

static void test_parse_markers_null_alloc(void) {
    hu_image_ref_t *refs = NULL;
    size_t ref_count = 0;
    char *cleaned = NULL;
    size_t cleaned_len = 0;
    hu_error_t err =
        hu_multimodal_parse_markers(NULL, "x", 1, &refs, &ref_count, &cleaned, &cleaned_len);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_content_part_audio_create(void) {
    hu_content_part_t part = {0};
    part.tag = HU_CONTENT_PART_AUDIO_BASE64;
    part.data.audio_base64.data = "SGVsbG8=";
    part.data.audio_base64.data_len = 8;
    part.data.audio_base64.media_type = "audio/wav";
    part.data.audio_base64.media_type_len = 9;

    HU_ASSERT_EQ(part.tag, HU_CONTENT_PART_AUDIO_BASE64);
    HU_ASSERT_NOT_NULL(part.data.audio_base64.data);
    HU_ASSERT_EQ(part.data.audio_base64.data_len, 8u);
    HU_ASSERT_STR_EQ(part.data.audio_base64.media_type, "audio/wav");
    HU_ASSERT_EQ(part.data.audio_base64.media_type_len, 9u);
}

static void test_content_part_video_create(void) {
    hu_content_part_t part = {0};
    part.tag = HU_CONTENT_PART_VIDEO_URL;
    part.data.video_url.url = "https://example.com/video.mp4";
    part.data.video_url.url_len = 28;
    part.data.video_url.media_type = "video/mp4";
    part.data.video_url.media_type_len = 9;

    HU_ASSERT_EQ(part.tag, HU_CONTENT_PART_VIDEO_URL);
    HU_ASSERT_NOT_NULL(part.data.video_url.url);
    HU_ASSERT_EQ(part.data.video_url.url_len, 28u);
    HU_ASSERT_STR_EQ(part.data.video_url.media_type, "video/mp4");
    HU_ASSERT_EQ(part.data.video_url.media_type_len, 9u);
}

static void test_multimodal_detect_audio_mime_wav(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("file.wav", 8), "audio/wav");
}

static void test_multimodal_detect_audio_mime_mp3(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("track.mp3", 9), "audio/mpeg");
}

static void test_multimodal_detect_audio_mime_ogg(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("sound.ogg", 9), "audio/ogg");
}

static void test_multimodal_detect_audio_mime_m4a(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("song.m4a", 8), "audio/mp4");
}

static void test_multimodal_detect_audio_mime_caf(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("rec.caf", 7), "audio/x-caf");
}

static void test_multimodal_detect_audio_mime_flac(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("lossless.flac", 13), "audio/flac");
}

static void test_multimodal_detect_audio_mime_unknown(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("file.xyz", 8), "audio/wav");
}

static void test_multimodal_detect_audio_mime_path(void) {
    HU_ASSERT_STR_EQ(hu_multimodal_detect_audio_mime("/tmp/recordings/voice.wav", 26), "audio/wav");
}

/* --- human/multimodal/image.h (header bytes + extension + vision prompt) --- */

static void test_image_detect_format_png_extension(void) {
    HU_ASSERT_EQ(hu_image_detect_format("photo.png", 9), HU_IMAGE_PNG);
}

static void test_image_detect_format_jpeg_extension(void) {
    HU_ASSERT_EQ(hu_image_detect_format("/tmp/x.JPEG", 11), HU_IMAGE_JPEG);
    HU_ASSERT_EQ(hu_image_detect_format("img.jpg", 7), HU_IMAGE_JPEG);
}

static void test_image_detect_format_unknown_extension(void) {
    HU_ASSERT_EQ(hu_image_detect_format("data.bin", 8), HU_IMAGE_UNKNOWN_FORMAT);
}

static void test_image_detect_format_null_returns_unknown(void) {
    HU_ASSERT_EQ(hu_image_detect_format(NULL, 4), HU_IMAGE_UNKNOWN_FORMAT);
}

static void test_image_mime_type_supported_formats(void) {
    HU_ASSERT_STR_EQ(hu_image_mime_type(HU_IMAGE_JPEG), "image/jpeg");
    HU_ASSERT_STR_EQ(hu_image_mime_type(HU_IMAGE_PNG), "image/png");
    HU_ASSERT_STR_EQ(hu_image_mime_type(HU_IMAGE_GIF), "image/gif");
    HU_ASSERT_STR_EQ(hu_image_mime_type(HU_IMAGE_WEBP), "image/webp");
    HU_ASSERT_STR_EQ(hu_image_mime_type(HU_IMAGE_UNKNOWN_FORMAT), "application/octet-stream");
}

static void test_image_parse_header_png_dimensions(void) {
    /* Minimal PNG signature + IHDR chunk prefix so width/height at offsets 16 and 20 are read. */
    unsigned char png[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
                           'I',  'H', 'D', 'R', 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x14};
    hu_image_metadata_t meta = {0};
    HU_ASSERT_EQ(hu_image_parse_header(png, sizeof(png), &meta), HU_OK);
    HU_ASSERT_EQ(meta.format, HU_IMAGE_PNG);
    HU_ASSERT_EQ(meta.width, 10u);
    HU_ASSERT_EQ(meta.height, 20u);
}

static void test_image_parse_header_jpeg_sof_dimensions(void) {
    /* FF D8 FF then SOF0 segment with height/width. */
    unsigned char jpg[] = {0xFF, 0xD8, 0xFF, 0xC0, 0x00, 0x11, 0x08,
                           0x00, 0x0A, /* height 10 */
                           0x01, 0xF4 /* width 500 */};
    hu_image_metadata_t meta = {0};
    HU_ASSERT_EQ(hu_image_parse_header(jpg, sizeof(jpg), &meta), HU_OK);
    HU_ASSERT_EQ(meta.format, HU_IMAGE_JPEG);
    HU_ASSERT_EQ(meta.height, 10u);
    HU_ASSERT_EQ(meta.width, 500u);
}

static void test_image_parse_header_truncated_fewer_than_eight_bytes(void) {
    unsigned char few[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A};
    hu_image_metadata_t meta = {0};
    HU_ASSERT_EQ(hu_image_parse_header(few, sizeof(few), &meta), HU_ERR_PARSE);
    HU_ASSERT_EQ(meta.format, HU_IMAGE_UNKNOWN_FORMAT);
}

static void test_image_parse_header_png_signature_short_ok_dims_zero(void) {
    unsigned char png_sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    hu_image_metadata_t meta = {0};
    HU_ASSERT_EQ(hu_image_parse_header(png_sig, sizeof(png_sig), &meta), HU_OK);
    HU_ASSERT_EQ(meta.format, HU_IMAGE_PNG);
    HU_ASSERT_EQ(meta.width, 0u);
    HU_ASSERT_EQ(meta.height, 0u);
}

static void test_image_parse_header_unknown_magic(void) {
    unsigned char unk[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    hu_image_metadata_t meta = {0};
    HU_ASSERT_EQ(hu_image_parse_header(unk, sizeof(unk), &meta), HU_ERR_PARSE);
}

static void test_image_parse_header_null_args(void) {
    hu_image_metadata_t meta = {0};
    HU_ASSERT_EQ(hu_image_parse_header(NULL, 8, &meta), HU_ERR_INVALID_ARGUMENT);
    unsigned char buf[8] = {0};
    HU_ASSERT_EQ(hu_image_parse_header(buf, 8, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void test_image_build_vision_prompt_valid(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *prompt = NULL;
    size_t len = 0;
    hu_error_t err =
        hu_image_build_vision_prompt(&alloc, "user context", 12, HU_IMAGE_PNG, &prompt, &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_NOT_NULL(prompt);
    HU_ASSERT_TRUE(len > 0);
    HU_ASSERT(strstr(prompt, "user context") != NULL);
    HU_ASSERT(strstr(prompt, "image/png") != NULL);
    alloc.free(alloc.ctx, prompt, len + 1);
}

static void test_image_build_vision_prompt_null_allocator(void) {
    char *prompt = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_image_build_vision_prompt(NULL, "ctx", 3, HU_IMAGE_JPEG, &prompt, &len),
                 HU_ERR_INVALID_ARGUMENT);
}

static void test_image_metadata_deinit_no_mime_is_safe(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_image_metadata_t meta = {0};
    meta.mime_type = NULL;
    hu_image_metadata_deinit(&meta, &alloc);
}

void run_multimodal_tests(void) {
    HU_TEST_SUITE("Multimodal");
    HU_RUN_TEST(test_base64_encode_empty);
    HU_RUN_TEST(test_base64_encode_hello);
    HU_RUN_TEST(test_base64_encode_padding);
    HU_RUN_TEST(test_base64_decode_roundtrip_hello);
    HU_RUN_TEST(test_base64_decode_invalid_char_returns_error);
    HU_RUN_TEST(test_detect_mime_png);
    HU_RUN_TEST(test_detect_mime_jpeg);
    HU_RUN_TEST(test_detect_mime_unknown);
    HU_RUN_TEST(test_parse_markers_single);
    HU_RUN_TEST(test_parse_markers_url);
    HU_RUN_TEST(test_encode_image_mock);
    HU_RUN_TEST(test_build_openai_image);
    HU_RUN_TEST(test_build_anthropic_image);
    HU_RUN_TEST(test_build_gemini_image);
    HU_RUN_TEST(test_encode_image_raw);
    HU_RUN_TEST(test_detect_mime_webp);
    HU_RUN_TEST(test_detect_mime_gif);
    HU_RUN_TEST(test_detect_mime_bmp);
    HU_RUN_TEST(test_parse_markers_multiple);
    HU_RUN_TEST(test_parse_markers_case_insensitive);
    HU_RUN_TEST(test_parse_markers_img_alias);
    HU_RUN_TEST(test_parse_markers_data_uri);
    HU_RUN_TEST(test_parse_markers_none);
    HU_RUN_TEST(test_parse_markers_null_alloc);
    HU_RUN_TEST(test_content_part_audio_create);
    HU_RUN_TEST(test_content_part_video_create);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_wav);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_mp3);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_ogg);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_m4a);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_caf);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_flac);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_unknown);
    HU_RUN_TEST(test_multimodal_detect_audio_mime_path);
    HU_RUN_TEST(test_image_detect_format_png_extension);
    HU_RUN_TEST(test_image_detect_format_jpeg_extension);
    HU_RUN_TEST(test_image_detect_format_unknown_extension);
    HU_RUN_TEST(test_image_detect_format_null_returns_unknown);
    HU_RUN_TEST(test_image_mime_type_supported_formats);
    HU_RUN_TEST(test_image_parse_header_png_dimensions);
    HU_RUN_TEST(test_image_parse_header_jpeg_sof_dimensions);
    HU_RUN_TEST(test_image_parse_header_truncated_fewer_than_eight_bytes);
    HU_RUN_TEST(test_image_parse_header_png_signature_short_ok_dims_zero);
    HU_RUN_TEST(test_image_parse_header_unknown_magic);
    HU_RUN_TEST(test_image_parse_header_null_args);
    HU_RUN_TEST(test_image_build_vision_prompt_valid);
    HU_RUN_TEST(test_image_build_vision_prompt_null_allocator);
    HU_RUN_TEST(test_image_metadata_deinit_no_mime_is_safe);
}
