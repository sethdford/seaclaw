#include "human/oauth.h"
#include "human/core/error.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if HU_ENABLE_TLS
#include <openssl/sha.h>
#endif

/* ── Internal SHA-256 ─────────────────────────────────────────────────── */

#if HU_ENABLE_TLS

static hu_error_t oauth_sha256(const uint8_t *input, size_t input_len, uint8_t *out32) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    if (!SHA256_Init(&sha256))
        return HU_ERR_CRYPTO_ENCRYPT;
    if (!SHA256_Update(&sha256, input, input_len))
        return HU_ERR_CRYPTO_ENCRYPT;
    if (!SHA256_Final(digest, &sha256))
        return HU_ERR_CRYPTO_ENCRYPT;
    memcpy(out32, digest, 32);
    return HU_OK;
}

#else  /* !HU_ENABLE_TLS */

/* Minimal embedded SHA-256 for testing (simple reference implementation) */
#ifdef HU_IS_TEST
static hu_error_t oauth_sha256(const uint8_t *input, size_t input_len, uint8_t *out32) {
    /* Deterministic mock: FNV-1a-style hash of full input into 32 bytes */
    memset(out32, 0, 32);
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < input_len; i++) {
        h ^= input[i];
        h *= 0x01000193u;
        out32[i % 32] ^= (uint8_t)(h & 0xFF);
        out32[(i + 7) % 32] ^= (uint8_t)((h >> 8) & 0xFF);
    }
    return HU_OK;
}
#else
static hu_error_t oauth_sha256(const uint8_t *input, size_t input_len, uint8_t *out32) {
    (void)input;
    (void)input_len;
    (void)out32;
    return HU_ERR_NOT_SUPPORTED;
}
#endif

#endif  /* HU_ENABLE_TLS */

/* ── Base64url Encoding ────────────────────────────────────────────────── */

hu_error_t hu_mcp_base64url_encode(const uint8_t *input, size_t input_len, char *output,
                                size_t output_size, size_t *out_len) {
    if (!input || !output || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    size_t out_idx = 0;

    for (size_t i = 0; i < input_len; i += 3) {
        uint32_t b = (uint32_t)input[i] << 16;
        if (i + 1 < input_len)
            b |= (uint32_t)input[i + 1] << 8;
        if (i + 2 < input_len)
            b |= (uint32_t)input[i + 2];

        if (out_idx + 4 > output_size)
            return HU_ERR_INVALID_ARGUMENT;

        output[out_idx++] = base64_chars[(b >> 18) & 0x3F];
        output[out_idx++] = base64_chars[(b >> 12) & 0x3F];

        if (i + 1 < input_len) {
            output[out_idx++] = base64_chars[(b >> 6) & 0x3F];
        }
        if (i + 2 < input_len) {
            output[out_idx++] = base64_chars[b & 0x3F];
        }
    }

    if (out_idx >= output_size)
        return HU_ERR_INVALID_ARGUMENT;
    output[out_idx] = '\0';
    *out_len = out_idx;
    return HU_OK;
}

/* ── PKCE ──────────────────────────────────────────────────────────────── */

hu_error_t hu_mcp_oauth_pkce_generate(hu_oauth_pkce_t *out) {
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;

    /* Generate 96 random bits -> 128 base64url chars (43-128 chosen chars from [A-Za-z0-9-._~]) */
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    const size_t charset_len = 64;
    const size_t verifier_len = 96;  /* RFC 7636 recommends 43-128; use 96 */

    for (size_t i = 0; i < verifier_len; i++) {
        uint32_t rnd = (uint32_t)rand();
        size_t idx = rnd % charset_len;
        out->verifier[i] = charset[idx];
    }
    out->verifier[verifier_len] = '\0';

    return HU_OK;
}

hu_error_t hu_mcp_oauth_pkce_challenge(const char *verifier, char *challenge_out,
                                   size_t challenge_size) {
    if (!verifier || !challenge_out || challenge_size < 48)
        return HU_ERR_INVALID_ARGUMENT;

    size_t verifier_len = strlen(verifier);

    /* SHA256(verifier) -> 32 bytes */
    uint8_t digest[32];
    hu_error_t err = oauth_sha256((const uint8_t *)verifier, verifier_len, digest);
    if (err != HU_OK)
        return err;

    /* Base64url(digest) */
    size_t encoded_len = 0;
    return hu_mcp_base64url_encode(digest, 32, challenge_out, challenge_size, &encoded_len);
}

/* ── Authorization URL ─────────────────────────────────────────────────── */

hu_error_t hu_mcp_oauth_build_auth_url(hu_allocator_t *alloc, const hu_oauth_config_t *config,
                                       const hu_oauth_pkce_t *pkce, const char *state,
                                       char **out_url, size_t *out_url_len) {
    if (!alloc || !config || !pkce || !state || !out_url || !out_url_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out_url = NULL;
    *out_url_len = 0;

    /* Estimate URL size */
    size_t url_size = config->auth_url_len + config->client_id_len + config->redirect_uri_len +
                      config->scopes_len + strlen(state) + 128 + 256;  /* buffer for params */

    char *url = (char *)alloc->alloc(alloc->ctx, url_size);
    if (!url)
        return HU_ERR_OUT_OF_MEMORY;

    /* Build URL: {auth_url}?client_id={cid}&redirect_uri={uri}&scope={scopes}&state={state}&code_challenge={challenge}&code_challenge_method=S256 */
    int written = snprintf(url, url_size,
                           "%.*s?"
                           "client_id=%.*s&"
                           "redirect_uri=%.*s&"
                           "scope=%.*s&"
                           "state=%s&"
                           "code_challenge=%s&"
                           "code_challenge_method=S256",
                           (int)config->auth_url_len, config->auth_url,
                           (int)config->client_id_len, config->client_id,
                           (int)config->redirect_uri_len, config->redirect_uri,
                           (int)config->scopes_len, config->scopes, state, pkce->challenge);

    if (written < 0 || (size_t)written >= url_size) {
        alloc->free(alloc->ctx, url, url_size);
        return HU_ERR_INVALID_ARGUMENT;
    }

    *out_url = url;
    *out_url_len = (size_t)written;
    return HU_OK;
}

/* ── Token Exchange ────────────────────────────────────────────────────── */

hu_error_t hu_mcp_oauth_exchange_code(hu_allocator_t *alloc, const hu_oauth_config_t *config,
                                  const hu_oauth_pkce_t *pkce, const char *code,
                                  hu_oauth_token_t *out_token) {
    if (!alloc || !config || !pkce || !code || !out_token)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out_token, 0, sizeof(*out_token));

    /* Build request body as form data:
       grant_type=authorization_code&code=...&client_id=...&code_verifier=...&redirect_uri=... */
    size_t body_size = 512 + config->client_id_len + config->redirect_uri_len + strlen(code) + 128;
    char *body = (char *)alloc->alloc(alloc->ctx, body_size);
    if (!body)
        return HU_ERR_OUT_OF_MEMORY;

    int written = snprintf(body, body_size,
                           "grant_type=authorization_code&"
                           "code=%s&"
                           "client_id=%.*s&"
                           "code_verifier=%s&"
                           "redirect_uri=%.*s",
                           code, (int)config->client_id_len, config->client_id, pkce->verifier,
                           (int)config->redirect_uri_len, config->redirect_uri);

    if (written < 0 || (size_t)written >= body_size) {
        alloc->free(alloc->ctx, body, body_size);
        return HU_ERR_INVALID_ARGUMENT;
    }

    /* TODO: Make HTTP POST request to config->token_url with body
       For now, stub that returns HU_ERR_NOT_SUPPORTED */
    alloc->free(alloc->ctx, body, body_size);
    return HU_ERR_NOT_SUPPORTED;  /* HTTP client needed */
}

/* ── Token Lifecycle ───────────────────────────────────────────────────── */

bool hu_mcp_oauth_token_is_expired(const hu_oauth_token_t *token) {
    if (!token || token->expires_at == 0)
        return false;  /* Never expires */
    time_t now = time(NULL);
    return (int64_t)now >= token->expires_at;
}

hu_error_t hu_mcp_oauth_token_save(hu_allocator_t *alloc, const char *path,
                                const char *server_name, const hu_oauth_token_t *token) {
    if (!alloc || !path || !server_name || !token)
        return HU_ERR_INVALID_ARGUMENT;

    /* TODO: Load existing tokens.json, add/update server_name entry, save back
       For now, stub */
    (void)token;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_mcp_oauth_token_load(hu_allocator_t *alloc, const char *path,
                                const char *server_name, hu_oauth_token_t *out_token) {
    if (!alloc || !path || !server_name || !out_token)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out_token, 0, sizeof(*out_token));

    /* TODO: Load tokens.json, find server_name key, parse token fields
       For now, stub */
    return HU_ERR_NOT_FOUND;
}

void hu_mcp_oauth_token_free(hu_allocator_t *alloc, hu_oauth_token_t *token) {
    if (!alloc || !token)
        return;

    if (token->access_token) {
        alloc->free(alloc->ctx, token->access_token, token->access_token_len + 1);
        token->access_token = NULL;
    }
    if (token->refresh_token) {
        alloc->free(alloc->ctx, token->refresh_token, token->refresh_token_len + 1);
        token->refresh_token = NULL;
    }
    if (token->token_type) {
        alloc->free(alloc->ctx, token->token_type, token->token_type_len + 1);
        token->token_type = NULL;
    }
}
