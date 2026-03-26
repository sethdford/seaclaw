/*
 * Tests for Cartesia voice cloning (hu_voice_clone_* / hu_persona_set_voice_id).
 * Clone API validation and HU_IS_TEST mocks require HU_ENABLE_CARTESIA=ON.
 */
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/tts/voice_clone.h"
#include "test_helpers.h"
#include <string.h>

static void test_voice_clone_config_default(void) {
    hu_voice_clone_config_t cfg;
    memset(&cfg, 0xFF, sizeof(cfg));
    hu_voice_clone_config_default(&cfg);
    HU_ASSERT_NOT_NULL(cfg.language);
    HU_ASSERT_STR_EQ(cfg.language, "en");
    HU_ASSERT_EQ((long long)cfg.language_len, 2);
}

#if HU_ENABLE_CARTESIA

static void test_voice_clone_from_file_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_clone_result_t res;
    memset(&res, 0, sizeof(res));
    hu_error_t err =
        hu_voice_clone_from_file(&alloc, "test-key", 8, "/tmp/mock-sample.wav", NULL, &res);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(res.success);
    HU_ASSERT_EQ(strncmp(res.voice_id, "test-clone-", 11), 0);
}

static void test_voice_clone_from_bytes_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    uint8_t dummy[] = {0x52, 0x49, 0x46, 0x46};
    hu_voice_clone_result_t res;
    memset(&res, 0, sizeof(res));
    hu_error_t err = hu_voice_clone_from_bytes(&alloc, "test-key", 8, dummy, sizeof(dummy),
                                               "audio/wav", NULL, &res);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(res.success);
    HU_ASSERT_EQ(strncmp(res.voice_id, "test-clone-", 11), 0);
}

static void test_voice_clone_null_api_key(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_clone_result_t res;
    memset(&res, 0, sizeof(res));
    hu_error_t err = hu_voice_clone_from_file(&alloc, NULL, 0, "/tmp/x.wav", NULL, &res);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_voice_clone_null_output(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_voice_clone_from_file(&alloc, "test-key", 8, "/tmp/x.wav", NULL, NULL);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_voice_clone_empty_file(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_clone_result_t res;
    memset(&res, 0, sizeof(res));
    hu_error_t err = hu_voice_clone_from_file(&alloc, "test-key", 8, "", NULL, &res);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

static void test_voice_clone_empty_bytes(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_clone_result_t res;
    memset(&res, 0, sizeof(res));
    hu_error_t err =
        hu_voice_clone_from_bytes(&alloc, "test-key", 8, NULL, 4, "audio/wav", NULL, &res);
    HU_ASSERT_EQ(err, HU_ERR_INVALID_ARGUMENT);
}

#endif /* HU_ENABLE_CARTESIA */

static void test_persona_set_voice_id_mock(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_error_t err = hu_persona_set_voice_id(&alloc, "test_persona", 11, "test-voice-id", 13);
    HU_ASSERT_EQ(err, HU_OK);
}

void register_voice_clone_tests(void) {
    HU_TEST_SUITE("Voice clone");
    HU_RUN_TEST(test_voice_clone_config_default);
#if HU_ENABLE_CARTESIA
    HU_RUN_TEST(test_voice_clone_from_file_mock);
    HU_RUN_TEST(test_voice_clone_from_bytes_mock);
    HU_RUN_TEST(test_voice_clone_null_api_key);
    HU_RUN_TEST(test_voice_clone_null_output);
    HU_RUN_TEST(test_voice_clone_empty_file);
    HU_RUN_TEST(test_voice_clone_empty_bytes);
#endif
    HU_RUN_TEST(test_persona_set_voice_id_mock);
}
