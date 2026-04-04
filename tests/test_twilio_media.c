#include "human/channels/twilio_media.h"
#include "human/channel.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

static void *ta_alloc(void *ctx, size_t size) {
    (void)ctx;
    return malloc(size);
}
static void *ta_realloc(void *ctx, void *ptr, size_t old_size, size_t new_size) {
    (void)ctx;
    (void)old_size;
    return realloc(ptr, new_size);
}
static void ta_free(void *ctx, void *ptr, size_t size) {
    (void)ctx;
    (void)size;
    free(ptr);
}
static hu_allocator_t test_alloc = {.alloc = ta_alloc, .realloc = ta_realloc, .free = ta_free, .ctx = NULL};

static void test_twilio_media_twiml_formats_stream(void) {
    char buf[512];
    size_t len = 0;
    hu_error_t err =
        hu_twilio_media_twiml("wss://example.com/stream", buf, sizeof(buf), &len);
    HU_ASSERT_EQ(err, HU_OK);
    HU_ASSERT_TRUE(len > 0u);
    HU_ASSERT_TRUE(strstr(buf, "wss://example.com/stream") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "<Stream") != NULL);
    HU_ASSERT_TRUE(strstr(buf, "</Connect>") != NULL);
}

static void test_twilio_media_mulaw_pcm_round_trip_8k_path(void) {
    uint8_t mulaw_in[4] = {0xFF, 0xD5, 0xAA, 0x80};
    int16_t pcm16[16];
    size_t pcm_len = 0;
    HU_ASSERT_EQ(hu_twilio_media_mulaw_to_pcm(mulaw_in, 4, pcm16, 16, &pcm_len), HU_OK);
    HU_ASSERT_EQ(pcm_len, 8u);

    uint8_t mulaw_back[8];
    size_t mulaw_len = 0;
    /* Use 16 kHz buffer as pseudo 24 kHz path: take first 6 samples -> 2 mulaw bytes */
    HU_ASSERT_EQ(hu_twilio_media_pcm_to_mulaw(pcm16, 6, mulaw_back, sizeof(mulaw_back), &mulaw_len),
                 HU_OK);
    HU_ASSERT_EQ(mulaw_len, 2u);
}

static void test_twilio_media_create_destroy(void) {
    hu_twilio_media_config_t cfg = {
        .account_sid = "ACtest",
        .auth_token = "tok",
        .voice_webhook_url = "https://example.com/hook",
        .voice_provider = "gemini_live",
        .input_sample_rate = 8000,
        .output_sample_rate = 8000,
    };
    hu_channel_t ch = {0};
    HU_ASSERT_EQ(hu_twilio_media_create(&test_alloc, &cfg, &ch), HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable);
    HU_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "twilio_media");
    HU_ASSERT_TRUE(ch.vtable->health_check(ch.ctx));
    HU_ASSERT_EQ(ch.vtable->send(ch.ctx, "x", 1, "m", 1, NULL, 0), HU_OK);
    hu_twilio_media_destroy(&ch);
    HU_ASSERT_NULL(ch.ctx);
}

void run_twilio_media_tests(void) {
    HU_TEST_SUITE("twilio_media");
    HU_RUN_TEST(test_twilio_media_twiml_formats_stream);
    HU_RUN_TEST(test_twilio_media_mulaw_pcm_round_trip_8k_path);
    HU_RUN_TEST(test_twilio_media_create_destroy);
}
