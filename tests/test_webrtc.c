#include "test_framework.h"
#include "human/voice/webrtc.h"

static void test_webrtc_create(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_webrtc_config_t config; memset(&config, 0, sizeof(config));
    config.audio_enabled = true;
    hu_webrtc_session_t *session = NULL;
    HU_ASSERT_EQ(hu_webrtc_session_create(&alloc, &config, &session), HU_OK);
    HU_ASSERT(session != NULL);
    hu_webrtc_session_destroy(session);
}

static void test_webrtc_connect(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_webrtc_config_t config; memset(&config, 0, sizeof(config));
    config.audio_enabled = true;
    hu_webrtc_session_t *session = NULL;
    hu_webrtc_session_create(&alloc, &config, &session);
    HU_ASSERT_EQ(hu_webrtc_connect(session, "v=0"), HU_OK);
    HU_ASSERT(session->connected);
    hu_webrtc_session_destroy(session);
}

static void test_webrtc_audio(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_webrtc_config_t config; memset(&config, 0, sizeof(config));
    config.audio_enabled = true;
    hu_webrtc_session_t *session = NULL;
    hu_webrtc_session_create(&alloc, &config, &session);
    hu_webrtc_connect(session, "sdp");
    unsigned char data[] = {0, 1, 2, 3};
    HU_ASSERT_EQ(hu_webrtc_send_audio(session, data, sizeof(data)), HU_OK);
    hu_webrtc_session_destroy(session);
}

void run_webrtc_tests(void) {
    HU_TEST_SUITE("WebRTC Voice");
    HU_RUN_TEST(test_webrtc_create);
    HU_RUN_TEST(test_webrtc_connect);
    HU_RUN_TEST(test_webrtc_audio);
}
