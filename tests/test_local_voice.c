#include "human/core/allocator.h"
#include "human/voice/local_stt.h"
#include "human/voice/local_tts.h"
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

static void local_stt_transcribe_mock_returns_text(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_local_stt_config_t cfg = {.endpoint = "http://localhost:8000/v1/audio/transcriptions"};
    char *txt = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_local_stt_transcribe(&alloc, &cfg, "/tmp/fake.wav", &txt, &len), HU_OK);
    HU_ASSERT_NOT_NULL(txt);
    HU_ASSERT_STR_EQ(txt, "Hello world");
    HU_ASSERT_EQ(len, strlen("Hello world"));
    alloc.free(alloc.ctx, txt, len + 1);
}

static void local_stt_transcribe_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *txt = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_local_stt_transcribe(NULL, NULL, "x.wav", &txt, &len), HU_ERR_INVALID_ARGUMENT);
    hu_local_stt_config_t cfg = {.endpoint = "http://localhost/x"};
    HU_ASSERT_EQ(hu_local_stt_transcribe(&alloc, &cfg, NULL, &txt, &len), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_local_stt_transcribe(&alloc, &cfg, "", &txt, &len), HU_ERR_INVALID_ARGUMENT);
}

static void local_tts_synthesize_mock_returns_path(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_local_tts_config_t cfg = {.endpoint = "http://localhost:8880/v1/audio/speech"};
    char *path = NULL;
    HU_ASSERT_EQ(hu_local_tts_synthesize(&alloc, &cfg, "hello", &path), HU_OK);
    HU_ASSERT_NOT_NULL(path);
    HU_ASSERT(strlen(path) > 0);
    FILE *f = fopen(path, "rb");
    HU_ASSERT_NOT_NULL(f);
    fclose(f);
    (void)remove(path);
    alloc.free(alloc.ctx, path, strlen(path) + 1);
}

static void local_tts_synthesize_null_args_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *path = NULL;
    HU_ASSERT_EQ(hu_local_tts_synthesize(NULL, NULL, "hi", &path), HU_ERR_INVALID_ARGUMENT);
    hu_local_tts_config_t cfg = {.endpoint = "http://x"};
    HU_ASSERT_EQ(hu_local_tts_synthesize(&alloc, &cfg, NULL, &path), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_local_tts_synthesize(&alloc, &cfg, "", &path), HU_ERR_INVALID_ARGUMENT);
}

void run_local_voice_tests(void) {
    HU_TEST_SUITE("LocalVoice");
    HU_RUN_TEST(local_stt_transcribe_mock_returns_text);
    HU_RUN_TEST(local_stt_transcribe_null_args_returns_error);
    HU_RUN_TEST(local_tts_synthesize_mock_returns_path);
    HU_RUN_TEST(local_tts_synthesize_null_args_returns_error);
}
