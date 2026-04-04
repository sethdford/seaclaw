/* Twilio Media Streams (voice/audio) channel adapter */
#include "human/channels/twilio_media.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct hu_twilio_media_ctx {
    hu_allocator_t *alloc;
    hu_twilio_media_config_t cfg;
    bool running;
} hu_twilio_media_ctx_t;

static hu_error_t twilio_media_start(void *ctx) {
    hu_twilio_media_ctx_t *c = (hu_twilio_media_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    c->running = true;
    return HU_OK;
}

static void twilio_media_stop(void *ctx) {
    hu_twilio_media_ctx_t *c = (hu_twilio_media_ctx_t *)ctx;
    if (!c)
        return;
    c->running = false;
}

static hu_error_t twilio_media_send(void *ctx, const char *target, size_t target_len,
                                    const char *message, size_t message_len, const char *const *media,
                                    size_t media_count) {
    (void)ctx;
    (void)target;
    (void)target_len;
    (void)message;
    (void)message_len;
    (void)media;
    (void)media_count;
#if defined(HU_IS_TEST) && HU_IS_TEST
    return HU_OK;
#else
    (void)0;
    return HU_OK;
#endif
}

static const char *twilio_media_name(void *ctx) {
    (void)ctx;
    return "twilio_media";
}

static bool twilio_media_health_check(void *ctx) {
    hu_twilio_media_ctx_t *c = (hu_twilio_media_ctx_t *)ctx;
    if (!c)
        return false;
    return c->running;
}

static const hu_channel_vtable_t twilio_media_vtable = {
    .start = twilio_media_start,
    .stop = twilio_media_stop,
    .send = twilio_media_send,
    .name = twilio_media_name,
    .health_check = twilio_media_health_check,
};

static void free_cfg_strings(hu_twilio_media_ctx_t *c, hu_allocator_t *alloc) {
    if (!c || !alloc)
        return;
    if (c->cfg.account_sid)
        alloc->free(alloc->ctx, c->cfg.account_sid, strlen(c->cfg.account_sid) + 1);
    if (c->cfg.auth_token)
        alloc->free(alloc->ctx, c->cfg.auth_token, strlen(c->cfg.auth_token) + 1);
    if (c->cfg.phone_number)
        alloc->free(alloc->ctx, c->cfg.phone_number, strlen(c->cfg.phone_number) + 1);
    if (c->cfg.voice_webhook_url)
        alloc->free(alloc->ctx, c->cfg.voice_webhook_url, strlen(c->cfg.voice_webhook_url) + 1);
    if (c->cfg.voice_provider)
        alloc->free(alloc->ctx, c->cfg.voice_provider, strlen(c->cfg.voice_provider) + 1);
    memset(&c->cfg, 0, sizeof(c->cfg));
}

/* ITU-T G.711 μ-law expansion (same values as a 256-entry hardware decode ROM). */
static int16_t hu_twilio_mulaw_decode_u8(uint8_t mulaw) {
    mulaw = (uint8_t)~mulaw;
    int t = ((mulaw & 0x0F) << 1) + 33;
    t <<= (mulaw >> 4) & 7;
    return (int16_t)((mulaw & 0x80) ? (33 - t) : (t - 33));
}

/* ITU-T G.711 μ-law compression; magnitude clamped to 8159 before encoding. */
static uint8_t hu_twilio_mulaw_encode_i16(int16_t linear) {
    int16_t sample = linear;
    int sign = (sample < 0) ? 0x80 : 0;
    if (sample < 0)
        sample = (int16_t)-sample;
    if (sample > 8159)
        sample = 8159;

    sample = (int16_t)(sample + 132); /* 0x84 bias */
    int exponent = 7;
    int exp_mask = 0x4000;
    while ((sample & exp_mask) == 0 && exponent > 0) {
        exp_mask >>= 1;
        exponent--;
    }
    int mantissa = (sample >> (exponent + 3)) & 0x0F;
    return (uint8_t)~(sign | (exponent << 4) | mantissa);
}

hu_error_t hu_twilio_media_create(hu_allocator_t *alloc, const hu_twilio_media_config_t *cfg,
                                  hu_channel_t *out) {
    if (!alloc || !cfg || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_twilio_media_ctx_t *ctx =
        (hu_twilio_media_ctx_t *)alloc->alloc(alloc->ctx, sizeof(hu_twilio_media_ctx_t));
    if (!ctx)
        return HU_ERR_OUT_OF_MEMORY;
    memset(ctx, 0, sizeof(*ctx));
    ctx->alloc = alloc;
    ctx->running = false;

    ctx->cfg.account_sid = hu_strdup(alloc, cfg->account_sid);
    ctx->cfg.auth_token = hu_strdup(alloc, cfg->auth_token);
    ctx->cfg.phone_number = hu_strdup(alloc, cfg->phone_number);
    ctx->cfg.voice_webhook_url = hu_strdup(alloc, cfg->voice_webhook_url);
    ctx->cfg.voice_provider = hu_strdup(alloc, cfg->voice_provider);

    if ((cfg->account_sid && !ctx->cfg.account_sid) || (cfg->auth_token && !ctx->cfg.auth_token) ||
        (cfg->phone_number && !ctx->cfg.phone_number) ||
        (cfg->voice_webhook_url && !ctx->cfg.voice_webhook_url) ||
        (cfg->voice_provider && !ctx->cfg.voice_provider)) {
        free_cfg_strings(ctx, alloc);
        alloc->free(alloc->ctx, ctx, sizeof(*ctx));
        return HU_ERR_OUT_OF_MEMORY;
    }

    out->ctx = ctx;
    out->vtable = &twilio_media_vtable;
    return HU_OK;
}

void hu_twilio_media_destroy(hu_channel_t *ch, hu_allocator_t *alloc) {
    if (!ch || !alloc)
        return;
    hu_twilio_media_ctx_t *ctx = (hu_twilio_media_ctx_t *)ch->ctx;
    if (ctx) {
        free_cfg_strings(ctx, alloc);
        alloc->free(alloc->ctx, ctx, sizeof(*ctx));
    }
    ch->ctx = NULL;
    ch->vtable = NULL;
}

hu_error_t hu_twilio_media_twiml(hu_allocator_t *alloc, const char *stream_url, size_t stream_url_len,
                                 char **out, size_t *out_len) {
    if (!alloc || !stream_url || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    static const char prefix[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                 "<Response><Connect><Stream url=\"";
    static const char suffix[] = "\"/></Connect></Response>";
    size_t need = sizeof(prefix) - 1 + stream_url_len + sizeof(suffix) - 1 + 1;
    char *buf = (char *)alloc->alloc(alloc->ctx, need);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    int n = snprintf(buf, need, "%s%.*s%s", prefix, (int)stream_url_len, stream_url, suffix);
    if (n < 0 || (size_t)n >= need) {
        alloc->free(alloc->ctx, buf, need);
        return HU_ERR_IO;
    }
    *out = buf;
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_twilio_media_mulaw_to_pcm(hu_allocator_t *alloc, const uint8_t *mulaw, size_t mulaw_len,
                                        int16_t **out, size_t *out_samples) {
    if (!alloc || !mulaw || !out || !out_samples)
        return HU_ERR_INVALID_ARGUMENT;
    if (mulaw_len == 0) {
        *out = NULL;
        *out_samples = 0;
        return HU_OK;
    }

    size_t pcm8k_len = mulaw_len;
    int16_t *pcm8k = (int16_t *)alloc->alloc(alloc->ctx, pcm8k_len * sizeof(int16_t));
    if (!pcm8k)
        return HU_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < mulaw_len; i++)
        pcm8k[i] = hu_twilio_mulaw_decode_u8(mulaw[i]);

    size_t upsampled = pcm8k_len * 2;
    int16_t *pcm16k = (int16_t *)alloc->alloc(alloc->ctx, upsampled * sizeof(int16_t));
    if (!pcm16k) {
        alloc->free(alloc->ctx, pcm8k, pcm8k_len * sizeof(int16_t));
        return HU_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < pcm8k_len; i++) {
        pcm16k[2 * i] = pcm8k[i];
        if (i + 1 < pcm8k_len) {
            int32_t a = pcm8k[i];
            int32_t b = pcm8k[i + 1];
            pcm16k[2 * i + 1] = (int16_t)((a + b) / 2);
        } else {
            pcm16k[2 * i + 1] = pcm8k[i];
        }
    }

    alloc->free(alloc->ctx, pcm8k, pcm8k_len * sizeof(int16_t));
    *out = pcm16k;
    *out_samples = upsampled;
    return HU_OK;
}

hu_error_t hu_twilio_media_pcm_to_mulaw(hu_allocator_t *alloc, const int16_t *pcm, size_t pcm_samples,
                                        uint8_t **out, size_t *out_len) {
    if (!alloc || !pcm || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    size_t n = pcm_samples / 3;
    if (n == 0) {
        *out = NULL;
        *out_len = 0;
        return HU_OK;
    }

    uint8_t *buf = (uint8_t *)alloc->alloc(alloc->ctx, n);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < n; i++)
        buf[i] = hu_twilio_mulaw_encode_i16(pcm[i * 3]);

    *out = buf;
    *out_len = n;
    return HU_OK;
}
