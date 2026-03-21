#include "webrtc_internal.h"
#include "human/crypto.h"
#include <stdio.h>
#include <string.h>

#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/srtp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#endif

struct hu_webrtc_dtls_state {
    hu_allocator_t *alloc;
#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
    SSL_CTX *ctx;
    EVP_PKEY *pkey;
    X509 *cert;
#endif
};

#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
static hu_error_t hu_dtls_format_fp(const uint8_t sha256[32], char fingerprint[96]) {
    if (hu_webrtc_sdp_format_fingerprint(sha256, fingerprint) != HU_OK)
        return HU_ERR_INTERNAL;
    return HU_OK;
}

static hu_error_t hu_dtls_gen_cert(SSL_CTX *ctx, EVP_PKEY **out_pkey, X509 **out_cert) {
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!pctx)
        return HU_ERR_OUT_OF_MEMORY;
    if (EVP_PKEY_keygen_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return HU_ERR_INTERNAL;
    }
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return HU_ERR_INTERNAL;
    }
    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return HU_ERR_INTERNAL;
    }
    EVP_PKEY_CTX_free(pctx);

    X509 *x = X509_new();
    if (!x) {
        EVP_PKEY_free(pkey);
        return HU_ERR_OUT_OF_MEMORY;
    }
    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_get_notBefore(x), 0);
    X509_gmtime_adj(X509_get_notAfter(x), 86400L * 365);
    X509_set_pubkey(x, pkey);
    X509_NAME *nm = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(nm, "CN", MBSTRING_ASC, (unsigned char *)"human-webrtc", -1, -1, 0);
    X509_set_issuer_name(x, nm);
    if (X509_sign(x, pkey, EVP_sha256()) <= 0) {
        X509_free(x);
        EVP_PKEY_free(pkey);
        return HU_ERR_INTERNAL;
    }
    if (SSL_CTX_use_certificate(ctx, x) <= 0 || SSL_CTX_use_PrivateKey(ctx, pkey) <= 0) {
        X509_free(x);
        EVP_PKEY_free(pkey);
        return HU_ERR_INTERNAL;
    }
    *out_pkey = pkey;
    *out_cert = x;
    return HU_OK;
}
#endif

hu_webrtc_dtls_state_t *hu_webrtc_dtls_create(hu_allocator_t *alloc, char fingerprint_sha256[96]) {
    if (!alloc || !fingerprint_sha256)
        return NULL;
    hu_webrtc_dtls_state_t *d = (hu_webrtc_dtls_state_t *)alloc->alloc(alloc->ctx, sizeof(*d));
    if (!d)
        return NULL;
    memset(d, 0, sizeof(*d));
    d->alloc = alloc;
#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
    SSL_CTX *ctx = SSL_CTX_new(DTLS_method());
    if (!ctx) {
        alloc->free(alloc->ctx, d, sizeof(*d));
        return NULL;
    }
    SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
    (void)SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL");
    if (SSL_CTX_set_tlsext_use_srtp(ctx, "SRTP_AES128_CM_SHA1_80") != 0) {
        SSL_CTX_free(ctx);
        alloc->free(alloc->ctx, d, sizeof(*d));
        return NULL;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    EVP_PKEY *pkey = NULL;
    X509 *cert = NULL;
    if (hu_dtls_gen_cert(ctx, &pkey, &cert) != HU_OK) {
        SSL_CTX_free(ctx);
        alloc->free(alloc->ctx, d, sizeof(*d));
        return NULL;
    }
    unsigned char *der = NULL;
    int derlen = i2d_X509(cert, &der);
    if (derlen <= 0 || !der) {
        X509_free(cert);
        EVP_PKEY_free(pkey);
        SSL_CTX_free(ctx);
        alloc->free(alloc->ctx, d, sizeof(*d));
        return NULL;
    }
    uint8_t sha[32];
    hu_sha256(der, (size_t)derlen, sha);
    OPENSSL_free(der);
    if (hu_dtls_format_fp(sha, fingerprint_sha256) != HU_OK) {
        X509_free(cert);
        EVP_PKEY_free(pkey);
        SSL_CTX_free(ctx);
        alloc->free(alloc->ctx, d, sizeof(*d));
        return NULL;
    }
    d->ctx = ctx;
    d->pkey = pkey;
    d->cert = cert;
#else
    {
        uint8_t z[32] = {0};
        if (hu_webrtc_sdp_format_fingerprint(z, fingerprint_sha256) != HU_OK) {
            alloc->free(alloc->ctx, d, sizeof(*d));
            return NULL;
        }
    }
#endif
    return d;
}

void hu_webrtc_dtls_destroy(hu_webrtc_dtls_state_t *dtls) {
    if (!dtls || !dtls->alloc)
        return;
#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
    if (dtls->cert)
        X509_free(dtls->cert);
    if (dtls->pkey)
        EVP_PKEY_free(dtls->pkey);
    if (dtls->ctx)
        SSL_CTX_free(dtls->ctx);
#endif
    hu_allocator_t *a = dtls->alloc;
    memset(dtls, 0, sizeof(*dtls));
    a->free(a->ctx, dtls, sizeof(*dtls));
}

#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
static int hu_dtls_handshake_loop(SSL *ssl) {
    for (int attempt = 0; attempt < 256; attempt++) {
        int r = SSL_do_handshake(ssl);
        if (r == 1)
            return 1;
        int err = SSL_get_error(ssl, r);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            continue;
        return 0;
    }
    return 0;
}
#endif

hu_error_t hu_webrtc_dtls_handshake(hu_webrtc_dtls_state_t *dtls, int udp_fd,
                                    const struct sockaddr *peer, socklen_t peer_len, bool dtls_as_client,
                                    hu_webrtc_dtls_srtp_material_t *material) {
    if (!material)
        return HU_ERR_INVALID_ARGUMENT;
    memset(material, 0, sizeof(*material));
#if defined(HU_HAS_TLS) && !defined(HU_IS_TEST)
    if (!dtls || !dtls->ctx || udp_fd < 0 || !peer)
        return HU_ERR_INVALID_ARGUMENT;
    BIO *bio = BIO_new_dgram(udp_fd, BIO_NOCLOSE);
    if (!bio)
        return HU_ERR_OUT_OF_MEMORY;
    BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_PEER, 0, (void *)peer);
    SSL *ssl = SSL_new(dtls->ctx);
    if (!ssl) {
        BIO_free(bio);
        return HU_ERR_OUT_OF_MEMORY;
    }
    SSL_set_bio(ssl, bio, bio);
    SSL_set_mtu(ssl, 1200);
    if (dtls_as_client)
        SSL_set_connect_state(ssl);
    else
        SSL_set_accept_state(ssl);
    if (!hu_dtls_handshake_loop(ssl)) {
        SSL_free(ssl);
        return HU_ERR_IO;
    }
    uint8_t buf[128];
    static const char label[] = "EXTRACTOR-dtls_srtp";
    if (!SSL_export_keying_material(ssl, buf, 60, label, (size_t)(sizeof(label) - 1), NULL, 0, 0)) {
        SSL_free(ssl);
        return HU_ERR_CRYPTO_ENCRYPT;
    }
    memcpy(material->client_key, buf, 16);
    memcpy(material->server_key, buf + 16, 16);
    memcpy(material->client_salt, buf + 32, 14);
    memcpy(material->server_salt, buf + 46, 14);
    material->valid = true;
    SSL_free(ssl);
    (void)peer_len;
    return HU_OK;
#else
    (void)dtls;
    (void)udp_fd;
    (void)peer;
    (void)peer_len;
    (void)dtls_as_client;
    material->valid = false;
    return HU_OK;
#endif
}
