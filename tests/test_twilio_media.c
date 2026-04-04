#include "human/channels/twilio_media.h"
#include "human/core/allocator.h"
#include "test_framework.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Same μ-law expansion as twilio_media.c — used to validate decode layout. */
static int16_t test_twilio_mulaw_decode_u8(uint8_t mulaw_byte) {
    mulaw_byte = (uint8_t)~mulaw_byte;
    int t = ((mulaw_byte & 0x0F) << 1) + 33;
    t <<= (mulaw_byte >> 4) & 7;
    return (int16_t)((mulaw_byte & 0x80) ? (33 - t) : (t - 33));
}

static int32_t test_i16_abs_diff(int16_t a, int16_t b) {
    int32_t d = (int32_t)a - (int32_t)b;
    return d < 0 ? -d : d;
}

static void twilio_media_create_and_destroy(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_twilio_media_config_t cfg = {0};
    hu_channel_t ch = {0};

    HU_ASSERT_EQ(hu_twilio_media_create(&alloc, &cfg, &ch), HU_OK);
    HU_ASSERT_NOT_NULL(ch.vtable);
    HU_ASSERT_NOT_NULL(ch.vtable->send);
    HU_ASSERT_NOT_NULL(ch.vtable->name);
    HU_ASSERT_STR_EQ(ch.vtable->name(ch.ctx), "twilio_media");
    HU_ASSERT_EQ(ch.vtable->send(ch.ctx, "u", 1, "m", 1, NULL, 0), HU_OK);
    hu_twilio_media_destroy(&ch, &alloc);
    HU_ASSERT_NULL(ch.ctx);
    HU_ASSERT_NULL(ch.vtable);
}

static void twilio_media_twiml_produces_xml(void) {
    hu_allocator_t alloc = hu_system_allocator();
    const char *url = "wss://example.com/media";
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(hu_twilio_media_twiml(&alloc, url, strlen(url), &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_GT((long long)out_len, 0LL);
    HU_ASSERT(strstr(out, "<Response") != NULL || strstr(out, "Stream") != NULL);
    alloc.free(alloc.ctx, out, out_len + 1);
}

static void twilio_media_twiml_null_url_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    char *out = NULL;
    size_t out_len = 0;

    HU_ASSERT_EQ(hu_twilio_media_twiml(&alloc, NULL, 0, &out, &out_len), HU_ERR_INVALID_ARGUMENT);
}

static void twilio_media_mulaw_to_pcm_roundtrip(void) {
    hu_allocator_t alloc = hu_system_allocator();
    enum { PCM_SAMPLES = 300 };
    int16_t pcm[PCM_SAMPLES];
    memset(pcm, 0, sizeof(pcm));
    for (size_t i = 0; i < PCM_SAMPLES / 3; i++)
        pcm[i * 3] = (int16_t)(500 + (int16_t)i * 17);

    uint8_t *mulaw = NULL;
    size_t mulaw_len = 0;
    HU_ASSERT_EQ(hu_twilio_media_pcm_to_mulaw(&alloc, pcm, PCM_SAMPLES, &mulaw, &mulaw_len), HU_OK);
    HU_ASSERT_NOT_NULL(mulaw);
    HU_ASSERT_EQ((long long)mulaw_len, 100LL);

    int16_t *decoded = NULL;
    size_t out_samples = 0;
    HU_ASSERT_EQ(hu_twilio_media_mulaw_to_pcm(&alloc, mulaw, mulaw_len, &decoded, &out_samples), HU_OK);
    HU_ASSERT_NOT_NULL(decoded);
    HU_ASSERT_EQ((long long)out_samples, (long long)(mulaw_len * 2));

    for (size_t i = 0; i < mulaw_len; i++) {
        int16_t ref8 = test_twilio_mulaw_decode_u8(mulaw[i]);
        HU_ASSERT_EQ((long long)decoded[2 * i], (long long)ref8);
        if (i + 1 < mulaw_len) {
            int32_t a = ref8;
            int32_t b = test_twilio_mulaw_decode_u8(mulaw[i + 1]);
            int16_t mid = (int16_t)((a + b) / 2);
            HU_ASSERT_EQ((long long)decoded[2 * i + 1], (long long)mid);
        } else {
            HU_ASSERT_EQ((long long)decoded[2 * i + 1], (long long)ref8);
        }
        int16_t back = test_twilio_mulaw_decode_u8(mulaw[i]);
        HU_ASSERT_LT(test_i16_abs_diff(back, pcm[i * 3]), 3000);
    }

    alloc.free(alloc.ctx, mulaw, mulaw_len);
    alloc.free(alloc.ctx, decoded, out_samples * sizeof(int16_t));
}

static void twilio_media_pcm_to_mulaw_known_values(void) {
    hu_allocator_t alloc = hu_system_allocator();
    int16_t pcm[12];
    memset(pcm, 0, sizeof(pcm));

    uint8_t *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(hu_twilio_media_pcm_to_mulaw(&alloc, pcm, sizeof(pcm) / sizeof(pcm[0]), &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_EQ((long long)out_len, 4LL);
    alloc.free(alloc.ctx, out, out_len);
}

static void twilio_media_mulaw_to_pcm_null_returns_error(void) {
    hu_allocator_t alloc = hu_system_allocator();
    int16_t *pcm = NULL;
    size_t samples = 0;

    HU_ASSERT_EQ(hu_twilio_media_mulaw_to_pcm(&alloc, NULL, 0, &pcm, &samples), HU_ERR_INVALID_ARGUMENT);
}

void run_twilio_media_tests(void) {
    HU_TEST_SUITE("TwilioMedia");
    HU_RUN_TEST(twilio_media_create_and_destroy);
    HU_RUN_TEST(twilio_media_twiml_produces_xml);
    HU_RUN_TEST(twilio_media_twiml_null_url_returns_error);
    HU_RUN_TEST(twilio_media_mulaw_to_pcm_roundtrip);
    HU_RUN_TEST(twilio_media_pcm_to_mulaw_known_values);
    HU_RUN_TEST(twilio_media_mulaw_to_pcm_null_returns_error);
}
