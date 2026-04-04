#include "test_framework.h"
#include "human/voice/provider.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include <string.h>

static hu_config_t make_test_config(void) {
    hu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.voice.stt_provider = "google";
    cfg.voice.tts_provider = "cartesia";
    cfg.voice.tts_voice = "test-voice";
    cfg.voice.tts_model = "sonic-2024-12-12";
    return cfg;
}

static void factory_gemini_live_full_lifecycle(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = make_test_config();
    hu_voice_provider_extras_t extras = {.api_key = "test-key-123"};
    hu_voice_provider_t vp = {0};
    hu_error_t err = hu_voice_provider_create_from_config(&alloc, &cfg, "gemini_live", &extras, &vp);
    HU_ASSERT(err == HU_OK);
    HU_ASSERT(vp.vtable != NULL);
    HU_ASSERT(vp.ctx != NULL);
    err = vp.vtable->connect(vp.ctx);
    HU_ASSERT(err == HU_OK);
    if (vp.vtable->send_activity_start) {
        err = vp.vtable->send_activity_start(vp.ctx);
        HU_ASSERT(err == HU_OK);
    }
    const char pcm[] = {0, 1, 0, 2, 0, 3, 0, 4};
    err = vp.vtable->send_audio(vp.ctx, pcm, sizeof(pcm));
    HU_ASSERT(err == HU_OK);
    hu_voice_rt_event_t ev;
    memset(&ev, 0, sizeof(ev));
    err = vp.vtable->recv_event(vp.ctx, &alloc, &ev, 10);
    HU_ASSERT(err == HU_OK || err == HU_ERR_TIMEOUT);
    hu_voice_rt_event_free(&alloc, &ev);
    vp.vtable->disconnect(vp.ctx, &alloc);
}

static void factory_openai_realtime_full_lifecycle(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = make_test_config();
    hu_voice_provider_extras_t extras = {.api_key = "test-key-456"};
    hu_voice_provider_t vp = {0};
    hu_error_t err = hu_voice_provider_create_from_config(&alloc, &cfg, "openai_realtime", &extras, &vp);
    HU_ASSERT(err == HU_OK);
    HU_ASSERT(vp.vtable != NULL);
    err = vp.vtable->connect(vp.ctx);
    HU_ASSERT(err == HU_OK);
    if (vp.vtable->send_activity_start) {
        err = vp.vtable->send_activity_start(vp.ctx);
        HU_ASSERT(err == HU_OK);
    }
    const char pcm[] = {0, 1, 0, 2};
    err = vp.vtable->send_audio(vp.ctx, pcm, sizeof(pcm));
    HU_ASSERT(err == HU_OK);
    hu_voice_rt_event_t ev;
    memset(&ev, 0, sizeof(ev));
    err = vp.vtable->recv_event(vp.ctx, &alloc, &ev, 10);
    HU_ASSERT(err == HU_OK || err == HU_ERR_TIMEOUT);
    hu_voice_rt_event_free(&alloc, &ev);
    vp.vtable->disconnect(vp.ctx, &alloc);
}

static void factory_unsupported_mode_rejected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = make_test_config();
    hu_voice_provider_t vp = {0};
    hu_error_t err = hu_voice_provider_create_from_config(&alloc, &cfg, "foobar_mode", NULL, &vp);
    HU_ASSERT(err == HU_ERR_NOT_SUPPORTED);
}

static void factory_extras_only_lifecycle(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_config_t cfg = make_test_config();
    hu_voice_provider_extras_t extras = {
        .api_key = "extra-key",
        .voice_id = "extra-voice",
        .model_id = "extra-model",
    };
    hu_voice_provider_t vp = {0};
    hu_error_t err = hu_voice_provider_create_from_config(&alloc, &cfg, "gemini_live", &extras, &vp);
    HU_ASSERT(err == HU_OK);
    err = vp.vtable->connect(vp.ctx);
    HU_ASSERT(err == HU_OK);
    if (vp.vtable->send_activity_start) {
        err = vp.vtable->send_activity_start(vp.ctx);
        HU_ASSERT(err == HU_OK);
    }
    const char pcm[] = {0, 0};
    err = vp.vtable->send_audio(vp.ctx, pcm, sizeof(pcm));
    HU_ASSERT(err == HU_OK);
    vp.vtable->disconnect(vp.ctx, &alloc);
}

static void factory_null_args_rejected(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_voice_provider_t vp = {0};
    hu_error_t err = hu_voice_provider_create_from_config(NULL, NULL, NULL, NULL, &vp);
    HU_ASSERT(err != HU_OK);
    err = hu_voice_provider_create_from_config(&alloc, NULL, "gemini_live", NULL, &vp);
    HU_ASSERT(err != HU_OK);
}

void run_voice_factory_e2e_tests(void) {
    HU_TEST_SUITE("Voice Factory E2E");
    HU_RUN_TEST(factory_gemini_live_full_lifecycle);
    HU_RUN_TEST(factory_openai_realtime_full_lifecycle);
    HU_RUN_TEST(factory_unsupported_mode_rejected);
    HU_RUN_TEST(factory_extras_only_lifecycle);
    HU_RUN_TEST(factory_null_args_rejected);
}
