#include "human/channels/twilio_media.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <string.h>

static void twilio_media_create_null_args(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_twilio_media_config_t cfg = {0};
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_twilio_media_create(NULL, &cfg, &ch), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_twilio_media_create(&a, NULL, &ch), HU_ERR_INVALID_ARGUMENT);
    HU_ASSERT_EQ(hu_twilio_media_create(&a, &cfg, NULL), HU_ERR_INVALID_ARGUMENT);
}

static void twilio_media_create_and_destroy(void) {
    hu_allocator_t a = hu_system_allocator();
    hu_twilio_media_config_t cfg = {.account_sid = "AC123", .auth_token = "tok", .phone_number = "+1555"};
    hu_channel_t ch;
    HU_ASSERT_EQ(hu_twilio_media_create(&a, &cfg, &ch), HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable);
    HU_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "twilio_media");
    hu_twilio_media_destroy(&ch, &a);
}

static void twilio_media_twiml_generation(void) {
    hu_allocator_t a = hu_system_allocator();
    char *out = NULL;
    size_t len = 0;
    HU_ASSERT_EQ(hu_twilio_media_twiml(&a, "wss://example.com/stream", 24, &out, &len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT(strstr(out, "Stream") != NULL);
    a.free(a.ctx, out, len + 1);
}

static void twilio_media_mulaw_to_pcm_basic(void) {
    hu_allocator_t a = hu_system_allocator();
    uint8_t mulaw[] = {0xFF, 0x7F, 0x00, 0x80};
    int16_t *pcm = NULL;
    size_t samples = 0;
    HU_ASSERT_EQ(hu_twilio_media_mulaw_to_pcm(&a, mulaw, 4, &pcm, &samples), HU_OK);
    HU_ASSERT_NOT_NULL(pcm);
    HU_ASSERT_EQ(samples, 8);
    a.free(a.ctx, pcm, samples * sizeof(int16_t));
}

void run_twilio_media_impl_tests(void) {
    HU_TEST_SUITE("TwilioMedia");
    HU_RUN_TEST(twilio_media_create_null_args);
    HU_RUN_TEST(twilio_media_create_and_destroy);
    HU_RUN_TEST(twilio_media_twiml_generation);
    HU_RUN_TEST(twilio_media_mulaw_to_pcm_basic);
}
