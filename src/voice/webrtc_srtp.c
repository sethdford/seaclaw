#include "webrtc_internal.h"
#include <string.h>

/* ── SHA-1 (RFC 3174) — for HMAC-SHA1-80 ─────────────────────────────────── */

typedef struct hu_srtp_sha1_ctx {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} hu_srtp_sha1_ctx_t;

static uint32_t hu_srtp_sha1_rotl(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void hu_srtp_sha1_transform(uint32_t st[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
    for (int i = 16; i < 80; i++)
        w[i] = hu_srtp_sha1_rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = st[0], b = st[1], c = st[2], d = st[3], e = st[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999U;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }
        uint32_t t = hu_srtp_sha1_rotl(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = hu_srtp_sha1_rotl(b, 30);
        b = a;
        a = t;
    }
    st[0] += a;
    st[1] += b;
    st[2] += c;
    st[3] += d;
    st[4] += e;
}

static void hu_srtp_sha1_init(hu_srtp_sha1_ctx_t *ctx) {
    ctx->state[0] = 0x67452301U;
    ctx->state[1] = 0xEFCDAB89U;
    ctx->state[2] = 0x98BADCFEU;
    ctx->state[3] = 0x10325476U;
    ctx->state[4] = 0xC3D2E1F0U;
    ctx->count = 0;
    memset(ctx->buffer, 0, 64);
}

static void hu_srtp_sha1_update(hu_srtp_sha1_ctx_t *ctx, const uint8_t *data, size_t len) {
    size_t i = 0;
    size_t idx = (size_t)(ctx->count & 63);
    ctx->count += len;
    if (idx) {
        size_t fill = 64 - idx;
        if (len < fill) {
            memcpy(ctx->buffer + idx, data, len);
            return;
        }
        memcpy(ctx->buffer + idx, data, fill);
        hu_srtp_sha1_transform(ctx->state, ctx->buffer);
        i = fill;
    }
    for (; i + 64 <= len; i += 64)
        hu_srtp_sha1_transform(ctx->state, data + i);
    if (i < len)
        memcpy(ctx->buffer, data + i, len - i);
}

static void hu_srtp_sha1_final(hu_srtp_sha1_ctx_t *ctx, uint8_t out[20]) {
    uint64_t bits = ctx->count * 8;
    size_t idx = (size_t)(ctx->count & 63);
    ctx->buffer[idx++] = 0x80;
    if (idx > 56) {
        memset(ctx->buffer + idx, 0, 64 - idx);
        hu_srtp_sha1_transform(ctx->state, ctx->buffer);
        idx = 0;
    }
    memset(ctx->buffer + idx, 0, 56 - idx);
    for (int j = 0; j < 8; j++)
        ctx->buffer[56 + j] = (uint8_t)(bits >> (56 - j * 8));
    hu_srtp_sha1_transform(ctx->state, ctx->buffer);
    for (int j = 0; j < 5; j++) {
        out[j * 4] = (uint8_t)(ctx->state[j] >> 24);
        out[j * 4 + 1] = (uint8_t)(ctx->state[j] >> 16);
        out[j * 4 + 2] = (uint8_t)(ctx->state[j] >> 8);
        out[j * 4 + 3] = (uint8_t)(ctx->state[j]);
    }
}

static void hu_srtp_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
                              uint8_t mac[20]) {
    uint8_t kbuf[64];
    uint8_t ipad[64];
    uint8_t opad[64];
    memset(kbuf, 0, sizeof(kbuf));
    if (key_len <= 64)
        memcpy(kbuf, key, key_len);
    else {
        hu_srtp_sha1_ctx_t cx;
        hu_srtp_sha1_init(&cx);
        hu_srtp_sha1_update(&cx, key, key_len);
        hu_srtp_sha1_final(&cx, kbuf);
    }
    for (int i = 0; i < 64; i++) {
        ipad[i] = (uint8_t)(kbuf[i] ^ 0x36);
        opad[i] = (uint8_t)(kbuf[i] ^ 0x5c);
    }
    hu_srtp_sha1_ctx_t c1;
    hu_srtp_sha1_init(&c1);
    hu_srtp_sha1_update(&c1, ipad, 64);
    hu_srtp_sha1_update(&c1, data, data_len);
    uint8_t inner[20];
    hu_srtp_sha1_final(&c1, inner);
    hu_srtp_sha1_ctx_t c2;
    hu_srtp_sha1_init(&c2);
    hu_srtp_sha1_update(&c2, opad, 64);
    hu_srtp_sha1_update(&c2, inner, 20);
    hu_srtp_sha1_final(&c2, mac);
}

/* ── AES-128 block encrypt (FIPS-197) — encrypt direction only ───────────── */

static uint8_t hu_srtp_aes_sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

static uint8_t hu_srtp_aes_rcon[10] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

static void hu_srtp_aes_key_expand(const uint8_t key[16], uint32_t w[44]) {
    for (int i = 0; i < 4; i++)
        w[i] = ((uint32_t)key[4 * i] << 24) | ((uint32_t)key[4 * i + 1] << 16) | ((uint32_t)key[4 * i + 2] << 8) |
               key[4 * i + 3];
    for (int i = 4; i < 44; i++) {
        uint32_t t = w[i - 1];
        if (i % 4 == 0) {
            t = ((t << 8) | (t >> 24));
            t = ((uint32_t)hu_srtp_aes_sbox[(t >> 24) & 0xff] << 24) |
                ((uint32_t)hu_srtp_aes_sbox[(t >> 16) & 0xff] << 16) |
                ((uint32_t)hu_srtp_aes_sbox[(t >> 8) & 0xff] << 8) | hu_srtp_aes_sbox[t & 0xff];
            t ^= (uint32_t)hu_srtp_aes_rcon[i / 4 - 1] << 24;
        }
        w[i] = w[i - 4] ^ t;
    }
}

static void hu_srtp_aes_add_round_key(uint8_t s[16], const uint32_t *w, int round) {
    for (int c = 0; c < 4; c++) {
        uint32_t k = w[round * 4 + c];
        s[c * 4] ^= (uint8_t)(k >> 24);
        s[c * 4 + 1] ^= (uint8_t)(k >> 16);
        s[c * 4 + 2] ^= (uint8_t)(k >> 8);
        s[c * 4 + 3] ^= (uint8_t)k;
    }
}

static void hu_srtp_aes_sub_bytes(uint8_t s[16]) {
    for (int i = 0; i < 16; i++)
        s[i] = hu_srtp_aes_sbox[s[i]];
}

static void hu_srtp_aes_shift_rows(uint8_t s[16]) {
    uint8_t t;
    t = s[1];
    s[1] = s[5];
    s[5] = s[9];
    s[9] = s[13];
    s[13] = t;
    t = s[2];
    s[2] = s[10];
    s[10] = t;
    t = s[6];
    s[6] = s[14];
    s[14] = t;
    t = s[15];
    s[15] = s[11];
    s[11] = s[7];
    s[7] = s[3];
    s[3] = t;
}

static uint8_t hu_srtp_aes_xtime(uint8_t x) { return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b)); }

static void hu_srtp_aes_mix_columns(uint8_t s[16]) {
    for (int c = 0; c < 4; c++) {
        int i = 4 * c;
        uint8_t a0 = s[i], a1 = s[i + 1], a2 = s[i + 2], a3 = s[i + 3];
        uint8_t r0 = (uint8_t)(hu_srtp_aes_xtime(a0) ^ hu_srtp_aes_xtime(a1) ^ a1 ^ a2 ^ a3);
        uint8_t r1 = (uint8_t)(a0 ^ hu_srtp_aes_xtime(a1) ^ hu_srtp_aes_xtime(a2) ^ a2 ^ a3);
        uint8_t r2 = (uint8_t)(a0 ^ a1 ^ hu_srtp_aes_xtime(a2) ^ hu_srtp_aes_xtime(a3) ^ a3);
        uint8_t r3 = (uint8_t)(hu_srtp_aes_xtime(a0) ^ a0 ^ a1 ^ a2 ^ hu_srtp_aes_xtime(a3));
        s[i] = r0;
        s[i + 1] = r1;
        s[i + 2] = r2;
        s[i + 3] = r3;
    }
}

static void hu_srtp_aes128_encrypt_block(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    uint32_t w[44];
    hu_srtp_aes_key_expand(key, w);
    uint8_t s[16];
    memcpy(s, in, 16);
    hu_srtp_aes_add_round_key(s, w, 0);
    for (int r = 1; r <= 9; r++) {
        hu_srtp_aes_sub_bytes(s);
        hu_srtp_aes_shift_rows(s);
        hu_srtp_aes_mix_columns(s);
        hu_srtp_aes_add_round_key(s, w, r);
    }
    hu_srtp_aes_sub_bytes(s);
    hu_srtp_aes_shift_rows(s);
    hu_srtp_aes_add_round_key(s, w, 10);
    memcpy(out, s, 16);
}

/* ── SRTP AES-CM PRF / packet IV (RFC 3711 §4.1.1, §4.3.3, Appendix B.3) ─── */

static void hu_srtp_aes_cm_prf(const uint8_t master_key[16], const uint8_t x14[14], uint8_t *out, size_t out_len) {
    uint8_t counter[16];
    memset(counter, 0, 16);
    memcpy(counter, x14, 14);
    /* x * 2^16: low 16 bits of the 128-bit AES block input are zero */
    while (out_len > 0) {
        uint8_t ks[16];
        hu_srtp_aes128_encrypt_block(master_key, counter, ks);
        size_t n = out_len < 16 ? out_len : 16;
        memcpy(out, ks, n);
        out += n;
        out_len -= n;
        for (int i = 15; i >= 0; i--) {
            if (++counter[i] != 0)
                break;
        }
    }
}

static void hu_srtp_kdf_derive(const uint8_t mkey[16], const uint8_t msalt[14], uint8_t label,
                               uint8_t *out, size_t out_len) {
    /* RFC 3711 Appendix B.3: initial derivation (index 0) XOR label into octet 7 of master salt. */
    uint8_t x[14];
    memcpy(x, msalt, 14);
    x[7] ^= label;
    hu_srtp_aes_cm_prf(mkey, x, out, out_len);
}

static void hu_srtp_derive_session_keys(const uint8_t mkey[16], const uint8_t msalt[14], uint8_t k_e[16],
                                        uint8_t k_s[14], uint8_t k_a[20]) {
    hu_srtp_kdf_derive(mkey, msalt, 0x00, k_e, 16);
    hu_srtp_kdf_derive(mkey, msalt, 0x02, k_s, 14);
    hu_srtp_kdf_derive(mkey, msalt, 0x01, k_a, 20);
}

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__
static void hu_srtp_packet_iv(uint8_t iv[16], const uint8_t salt[14], uint32_t ssrc, uint32_t roc,
                              uint16_t seq) {
    unsigned __int128 k_s = 0;
    for (int i = 0; i < 14; i++)
        k_s = (k_s << 8) | salt[i];
    unsigned __int128 ivv =
        (k_s << 16) ^ ((unsigned __int128)ssrc << 64) ^
        ((((unsigned __int128)roc << 16) | seq) << 16);
    for (int b = 0; b < 16; b++)
        iv[b] = (uint8_t)(ivv >> (120 - 8 * b));
}
#else
static void hu_srtp_packet_iv(uint8_t iv[16], const uint8_t salt[14], uint32_t ssrc, uint32_t roc,
                              uint16_t seq) {
    memset(iv, 0, 16);
    memcpy(iv, salt, 14);
    iv[4] ^= (uint8_t)(ssrc >> 24);
    iv[5] ^= (uint8_t)(ssrc >> 16);
    iv[6] ^= (uint8_t)(ssrc >> 8);
    iv[7] ^= (uint8_t)ssrc;
    uint64_t i = ((uint64_t)roc << 16) | seq;
    iv[8] ^= (uint8_t)(i >> 40);
    iv[9] ^= (uint8_t)(i >> 32);
    iv[10] ^= (uint8_t)(i >> 24);
    iv[11] ^= (uint8_t)(i >> 16);
    iv[12] ^= (uint8_t)(i >> 8);
    iv[13] ^= (uint8_t)i;
}
#endif

static void hu_srtp_ctr_xor_payload(const uint8_t k_e[16], const uint8_t iv[16], uint8_t *payload,
                                    size_t payload_len) {
    uint8_t ctr[16];
    memcpy(ctr, iv, 16);
    size_t off = 0;
    while (off < payload_len) {
        uint8_t ks[16];
        hu_srtp_aes128_encrypt_block(k_e, ctr, ks);
        size_t chunk = payload_len - off;
        if (chunk > 16)
            chunk = 16;
        for (size_t j = 0; j < chunk; j++)
            payload[off + j] ^= ks[j];
        off += chunk;
        for (int i = 15; i >= 0; i--) {
            if (++ctr[i] != 0)
                break;
        }
    }
}

static void hu_srtp_auth_m(const uint8_t *rtp, size_t rtp_len, uint32_t roc, uint8_t *m, size_t *m_len) {
    memcpy(m, rtp, rtp_len);
    m[rtp_len] = (uint8_t)(roc >> 24);
    m[rtp_len + 1] = (uint8_t)(roc >> 16);
    m[rtp_len + 2] = (uint8_t)(roc >> 8);
    m[rtp_len + 3] = (uint8_t)roc;
    *m_len = rtp_len + 4;
}

struct hu_webrtc_srtp_state {
    hu_allocator_t *alloc;
    uint8_t tx_k_e[16], tx_k_s[14], tx_k_a[20];
    uint8_t rx_k_e[16], rx_k_s[14], rx_k_a[20];
    uint32_t ssrc;
    uint32_t tx_roc;
    uint16_t tx_seq;
    uint32_t rx_roc;
    uint16_t rx_last_seq;
    bool has_rx_seq;
    bool ready;
};

hu_webrtc_srtp_state_t *hu_webrtc_srtp_create(hu_allocator_t *alloc) {
    if (!alloc)
        return NULL;
    hu_webrtc_srtp_state_t *s = (hu_webrtc_srtp_state_t *)alloc->alloc(alloc->ctx, sizeof(*s));
    if (!s)
        return NULL;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    return s;
}

void hu_webrtc_srtp_destroy(hu_webrtc_srtp_state_t *s) {
    if (!s || !s->alloc)
        return;
    hu_allocator_t *a = s->alloc;
    memset(s, 0, sizeof(*s));
    a->free(a->ctx, s, sizeof(*s));
}

hu_error_t hu_webrtc_srtp_init_keys(hu_webrtc_srtp_state_t *s, const uint8_t tx_mkey[16],
                                    const uint8_t tx_msalt[14], const uint8_t rx_mkey[16],
                                    const uint8_t rx_msalt[14], uint32_t ssrc) {
    if (!s)
        return HU_ERR_INVALID_ARGUMENT;
    hu_srtp_derive_session_keys(tx_mkey, tx_msalt, s->tx_k_e, s->tx_k_s, s->tx_k_a);
    hu_srtp_derive_session_keys(rx_mkey, rx_msalt, s->rx_k_e, s->rx_k_s, s->rx_k_a);
    s->ssrc = ssrc;
    s->tx_seq = 0;
    s->tx_roc = 0;
    s->rx_roc = 0;
    s->rx_last_seq = 0;
    s->has_rx_seq = false;
    s->ready = true;
    return HU_OK;
}

hu_error_t hu_webrtc_srtp_protect(hu_webrtc_srtp_state_t *s, const uint8_t *rtp, size_t rtp_len,
                                  uint8_t *out, size_t out_cap, size_t *out_len) {
    if (!s || !rtp || !out || !out_len || rtp_len < 12)
        return HU_ERR_INVALID_ARGUMENT;
    if (!s->ready || rtp_len + 10 > out_cap)
        return HU_ERR_INVALID_ARGUMENT;
    memcpy(out, rtp, rtp_len);
    uint16_t seq = (uint16_t)((rtp[2] << 8) | rtp[3]);
    uint32_t pkt_roc = s->tx_roc;
    uint8_t iv[16];
    hu_srtp_packet_iv(iv, s->tx_k_s, s->ssrc, pkt_roc, seq);
    hu_srtp_ctr_xor_payload(s->tx_k_e, iv, out + 12, rtp_len - 12);
    uint8_t m[2048];
    if (rtp_len + 4 > sizeof(m))
        return HU_ERR_INVALID_ARGUMENT;
    size_t ml = 0;
    hu_srtp_auth_m(out, rtp_len, pkt_roc, m, &ml);
    uint8_t mac[20];
    hu_srtp_hmac_sha1(s->tx_k_a, 20, m, ml, mac);
    memcpy(out + rtp_len, mac, 10);
    *out_len = rtp_len + 10;
    if (seq == 0xFFFFU)
        s->tx_roc++;
    s->tx_seq = seq;
    return HU_OK;
}

static void hu_srtp_update_rx_roc(hu_webrtc_srtp_state_t *s, uint16_t seq) {
    if (!s->has_rx_seq) {
        s->rx_last_seq = seq;
        s->has_rx_seq = true;
        return;
    }
    if (seq == s->rx_last_seq)
        return;
    if (((uint16_t)(seq - s->rx_last_seq)) < 0x8000 && seq < s->rx_last_seq)
        s->rx_roc++;
    else if (seq > s->rx_last_seq && (seq - s->rx_last_seq) > 0x8000)
        s->rx_roc--;
    s->rx_last_seq = seq;
}

hu_error_t hu_webrtc_srtp_unprotect(hu_webrtc_srtp_state_t *s, const uint8_t *srtp, size_t srtp_len,
                                    uint8_t *out, size_t out_cap, size_t *out_len) {
    if (!s || !srtp || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (!s->ready || srtp_len < 12 + 10 || srtp_len > out_cap)
        return HU_ERR_INVALID_ARGUMENT;
    size_t rtp_len = srtp_len - 10;
    memcpy(out, srtp, rtp_len);
    uint16_t seq = (uint16_t)((out[2] << 8) | out[3]);
    hu_srtp_update_rx_roc(s, seq);
    uint32_t roc = s->rx_roc;
    uint8_t iv[16];
    hu_srtp_packet_iv(iv, s->rx_k_s, s->ssrc, roc, seq);
    uint8_t m[2048];
    size_t ml = 0;
    hu_srtp_auth_m(out, rtp_len, roc, m, &ml);
    uint8_t mac[20];
    hu_srtp_hmac_sha1(s->rx_k_a, 20, m, ml, mac);
    if (memcmp(mac, srtp + rtp_len, 10) != 0)
        return HU_ERR_CRYPTO_DECRYPT;
    hu_srtp_ctr_xor_payload(s->rx_k_e, iv, out + 12, rtp_len - 12);
    *out_len = rtp_len;
    return HU_OK;
}

/* RFC 3711 B.3 cipher key test vector */
hu_error_t hu_webrtc_srtp_roundtrip_test(void) {
    const uint8_t mkey[16] = {0xE1, 0xF9, 0x7A, 0x0D, 0x3E, 0x01, 0x8B, 0xE0,
                              0xD6, 0x4F, 0xA3, 0x2C, 0x06, 0xDE, 0x41, 0x39};
    const uint8_t msalt[14] = {0x0E, 0xC6, 0x75, 0xAD, 0x49, 0x8A, 0xFE, 0xEB,
                               0xB6, 0x96, 0x0B, 0x3A, 0xAB, 0xE6};
    uint8_t k_e[16], k_s[14], k_a[20];
    hu_srtp_derive_session_keys(mkey, msalt, k_e, k_s, k_a);
    const uint8_t expect_k_e[16] = {0xC6, 0x1E, 0x7A, 0x93, 0x74, 0x4F, 0x39, 0xEE,
                                      0x10, 0x73, 0x4A, 0xFE, 0x3F, 0xF7, 0xA0, 0x87};
    if (memcmp(k_e, expect_k_e, 16) != 0)
        return HU_ERR_INTERNAL;
    const uint8_t expect_k_s[14] = {0x30, 0xCB, 0xBC, 0x08, 0x86, 0x3D, 0x8C, 0x85,
                                    0xD4, 0x9D, 0xB3, 0x4A, 0x9A, 0xE1};
    if (memcmp(k_s, expect_k_s, 14) != 0)
        return HU_ERR_INTERNAL;
    uint8_t rtp[20];
    memset(rtp, 0, sizeof(rtp));
    rtp[0] = 0x80;
    rtp[1] = 111;
    rtp[2] = 0;
    rtp[3] = 1;
    rtp[8] = 0x12;
    rtp[9] = 0x34;
    rtp[10] = 0x56;
    rtp[11] = 0x78;
    memcpy(rtp + 12, "abcd", 4);
    uint8_t obuf[64], rbuf[64];
    size_t olen = 0, rlen = 0;
    hu_allocator_t a = hu_system_allocator();
    hu_webrtc_srtp_state_t *st = hu_webrtc_srtp_create(&a);
    if (!st)
        return HU_ERR_OUT_OF_MEMORY;
    if (hu_webrtc_srtp_init_keys(st, mkey, msalt, mkey, msalt, 0x12345678) != HU_OK) {
        hu_webrtc_srtp_destroy(st);
        return HU_ERR_INTERNAL;
    }
    if (hu_webrtc_srtp_protect(st, rtp, sizeof(rtp), obuf, sizeof(obuf), &olen) != HU_OK) {
        hu_webrtc_srtp_destroy(st);
        return HU_ERR_INTERNAL;
    }
    if (hu_webrtc_srtp_unprotect(st, obuf, olen, rbuf, sizeof(rbuf), &rlen) != HU_OK) {
        hu_webrtc_srtp_destroy(st);
        return HU_ERR_INTERNAL;
    }
    hu_webrtc_srtp_destroy(st);
    if (rlen != sizeof(rtp) || memcmp(rbuf, rtp, sizeof(rtp)) != 0)
        return HU_ERR_INTERNAL;
    return HU_OK;
}
