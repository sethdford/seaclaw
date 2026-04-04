#include "test_framework.h"

static void voice_streaming_e2e_placeholder(void) {
    HU_ASSERT(1);
}

void run_voice_streaming_e2e_tests(void) {
    HU_TEST_SUITE("VoiceStreamingE2E");
    HU_RUN_TEST(voice_streaming_e2e_placeholder);
}
