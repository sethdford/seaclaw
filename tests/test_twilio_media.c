#include "test_framework.h"

static void twilio_media_placeholder(void) { HU_ASSERT(1); }

void run_twilio_media_tests(void) {
    HU_TEST_SUITE("TwilioMedia");
    HU_RUN_TEST(twilio_media_placeholder);
}
