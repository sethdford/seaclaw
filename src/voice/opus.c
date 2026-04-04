#include "human/voice/opus.h"
#ifndef HU_ENABLE_OPUS
hu_error_t hu_opus_encoder_create(hu_allocator_t *a, int r, int c, hu_opus_encoder_t **o) {
    (void)a;
    (void)r;
    (void)c;
    (void)o;
    return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_opus_encode(hu_opus_encoder_t *e, const int16_t *p, size_t f, uint8_t *o, size_t c,
                          size_t *l) {
    (void)e;
    (void)p;
    (void)f;
    (void)o;
    (void)c;
    (void)l;
    return HU_ERR_NOT_SUPPORTED;
}
void hu_opus_encoder_destroy(hu_opus_encoder_t *e, hu_allocator_t *a) {
    (void)e;
    (void)a;
}
hu_error_t hu_opus_decoder_create(hu_allocator_t *a, int r, int c, hu_opus_decoder_t **o) {
    (void)a;
    (void)r;
    (void)c;
    (void)o;
    return HU_ERR_NOT_SUPPORTED;
}
hu_error_t hu_opus_decode(hu_opus_decoder_t *d, const uint8_t *b, size_t l, int16_t *o, size_t m,
                          size_t *s) {
    (void)d;
    (void)b;
    (void)l;
    (void)o;
    (void)m;
    (void)s;
    return HU_ERR_NOT_SUPPORTED;
}
void hu_opus_decoder_destroy(hu_opus_decoder_t *d, hu_allocator_t *a) {
    (void)d;
    (void)a;
}
#endif
