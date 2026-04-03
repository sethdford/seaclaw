#include "human/oauth.h"
#include "human/core/error.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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

#ifndef HU_ENABLE_CURL
#ifdef HU_IS_TEST
    /* In test mode without curl: mock the response */
    const char *access_token = "test_access_token_from_mock";
    const char *token_type = "Bearer";

    out_token->access_token = (char *)alloc->alloc(alloc->ctx, strlen(access_token) + 1);
    if (!out_token->access_token)
        return HU_ERR_OUT_OF_MEMORY;
    strcpy(out_token->access_token, access_token);
    out_token->access_token_len = strlen(access_token);

    out_token->token_type = (char *)alloc->alloc(alloc->ctx, strlen(token_type) + 1);
    if (!out_token->token_type) {
        alloc->free(alloc->ctx, out_token->access_token, out_token->access_token_len + 1);
        out_token->access_token = NULL;
        return HU_ERR_OUT_OF_MEMORY;
    }
    strcpy(out_token->token_type, token_type);
    out_token->token_type_len = strlen(token_type);
    out_token->expires_at = (int64_t)time(NULL) + 3600;  /* 1 hour from now */
    return HU_OK;
#else
    return HU_ERR_NOT_SUPPORTED;  /* libcurl not available */
#endif
#else
    /* Build request body as form data */
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

    /* Make HTTP POST request to token_url with form-encoded body */
    hu_http_response_t resp;
    memset(&resp, 0, sizeof(resp));

    hu_error_t err = hu_http_request(alloc, config->token_url, "POST",
                                     "Content-Type: application/x-www-form-urlencoded\r\n",
                                     body, (size_t)written, &resp);
    alloc->free(alloc->ctx, body, body_size);

    if (err != HU_OK) {
        hu_http_response_free(alloc, &resp);
        return err;
    }

    /* Parse JSON response */
    hu_json_value_t *resp_obj = NULL;
    err = hu_json_parse(alloc, resp.body, resp.body_len, &resp_obj);
    hu_http_response_free(alloc, &resp);

    if (err != HU_OK || !resp_obj)
        return HU_ERR_PARSE;

    if (resp_obj->type != HU_JSON_OBJECT) {
        hu_json_free(alloc, resp_obj);
        return HU_ERR_PARSE;
    }

    /* Extract token fields from response */
    const char *access_token = hu_json_get_string(resp_obj, "access_token");
    if (!access_token) {
        hu_json_free(alloc, resp_obj);
        return HU_ERR_PARSE;  /* access_token is required */
    }

    const char *refresh_token = hu_json_get_string(resp_obj, "refresh_token");
    const char *token_type = hu_json_get_string(resp_obj, "token_type");
    double expires_in = hu_json_get_number(resp_obj, "expires_in", 0);

    /* Allocate and copy access_token */
    size_t access_len = strlen(access_token);
    out_token->access_token = (char *)alloc->alloc(alloc->ctx, access_len + 1);
    if (!out_token->access_token) {
        hu_json_free(alloc, resp_obj);
        return HU_ERR_OUT_OF_MEMORY;
    }
    strcpy(out_token->access_token, access_token);
    out_token->access_token_len = access_len;

    /* Allocate and copy refresh_token if present */
    if (refresh_token) {
        size_t refresh_len = strlen(refresh_token);
        out_token->refresh_token = (char *)alloc->alloc(alloc->ctx, refresh_len + 1);
        if (out_token->refresh_token) {
            strcpy(out_token->refresh_token, refresh_token);
            out_token->refresh_token_len = refresh_len;
        }
    }

    /* Allocate and copy token_type if present, default to "Bearer" */
    const char *tt = token_type ? token_type : "Bearer";
    size_t tt_len = strlen(tt);
    out_token->token_type = (char *)alloc->alloc(alloc->ctx, tt_len + 1);
    if (out_token->token_type) {
        strcpy(out_token->token_type, tt);
        out_token->token_type_len = tt_len;
    }

    /* Set expires_at if expires_in is present */
    if (expires_in > 0) {
        out_token->expires_at = (int64_t)time(NULL) + (int64_t)expires_in;
    }

    hu_json_free(alloc, resp_obj);
    return HU_OK;
#endif
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

    /* Load existing tokens file if it exists */
    hu_json_value_t *root = NULL;
    FILE *f = fopen(path, "rb");
    if (f) {
        struct stat st;
        if (stat(path, &st) == 0 && st.st_size > 0 && (size_t)st.st_size < 1024 * 1024) {
            size_t file_size = (size_t)st.st_size;
            char *buf = (char *)alloc->alloc(alloc->ctx, file_size + 1);
            if (buf) {
                size_t rd = fread(buf, 1, file_size, f);
                buf[rd] = '\0';
                hu_json_parse(alloc, buf, rd, &root);
                alloc->free(alloc->ctx, buf, file_size + 1);
            }
        }
        fclose(f);
    }

    /* If no valid root object, create a new one */
    if (!root || root->type != HU_JSON_OBJECT) {
        if (root) hu_json_free(alloc, root);
        root = hu_json_object_new(alloc);
        if (!root)
            return HU_ERR_OUT_OF_MEMORY;
    }

    /* Build token object */
    hu_json_value_t *token_obj = hu_json_object_new(alloc);
    if (!token_obj) {
        hu_json_free(alloc, root);
        return HU_ERR_OUT_OF_MEMORY;
    }

    /* Add token fields */
    if (token->access_token) {
        hu_json_value_t *v = hu_json_string_new(alloc, token->access_token, token->access_token_len);
        if (v) hu_json_object_set(alloc, token_obj, "access_token", v);
    }
    if (token->refresh_token) {
        hu_json_value_t *v = hu_json_string_new(alloc, token->refresh_token, token->refresh_token_len);
        if (v) hu_json_object_set(alloc, token_obj, "refresh_token", v);
    }
    if (token->expires_at != 0) {
        hu_json_value_t *v = hu_json_number_new(alloc, (double)token->expires_at);
        if (v) hu_json_object_set(alloc, token_obj, "expires_at", v);
    }
    if (token->token_type) {
        hu_json_value_t *v = hu_json_string_new(alloc, token->token_type, token->token_type_len);
        if (v) hu_json_object_set(alloc, token_obj, "token_type", v);
    }

    /* Set the server entry in root */
    hu_json_object_set(alloc, root, server_name, token_obj);

    /* Serialize to JSON */
    char *json_str = NULL;
    size_t json_len = 0;
    hu_error_t err = hu_json_stringify(alloc, root, &json_str, &json_len);
    hu_json_free(alloc, root);
    if (err != HU_OK)
        return err;

    /* Atomic write: temp file → rename */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp_%d", path, (int)getpid());

    FILE *tmp_f = fopen(tmp_path, "wb");
    if (!tmp_f) {
        alloc->free(alloc->ctx, json_str, json_len + 1);
        return HU_ERR_IO;
    }

    size_t written = fwrite(json_str, 1, json_len, tmp_f);
    fclose(tmp_f);
    alloc->free(alloc->ctx, json_str, json_len + 1);

    if (written != json_len) {
        unlink(tmp_path);
        return HU_ERR_IO;
    }

    /* Atomic rename */
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return HU_ERR_IO;
    }

    return HU_OK;
}

hu_error_t hu_mcp_oauth_token_load(hu_allocator_t *alloc, const char *path,
                                const char *server_name, hu_oauth_token_t *out_token) {
    if (!alloc || !path || !server_name || !out_token)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out_token, 0, sizeof(*out_token));

    /* Check if file exists */
    struct stat st;
    if (stat(path, &st) != 0)
        return HU_ERR_NOT_FOUND;
    if (!S_ISREG(st.st_mode) || st.st_size <= 0)
        return HU_ERR_NOT_FOUND;
    if ((size_t)st.st_size > 1024 * 1024)
        return HU_ERR_IO;

    /* Read file */
    FILE *f = fopen(path, "rb");
    if (!f)
        return HU_ERR_IO;

    size_t file_size = (size_t)st.st_size;
    char *buf = (char *)alloc->alloc(alloc->ctx, file_size + 1);
    if (!buf) {
        fclose(f);
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t rd = fread(buf, 1, file_size, f);
    fclose(f);
    buf[rd] = '\0';

    /* Parse JSON */
    hu_json_value_t *root = NULL;
    hu_error_t err = hu_json_parse(alloc, buf, rd, &root);
    alloc->free(alloc->ctx, buf, file_size + 1);
    if (err != HU_OK || !root)
        return HU_ERR_PARSE;

    if (root->type != HU_JSON_OBJECT) {
        hu_json_free(alloc, root);
        return HU_ERR_PARSE;
    }

    /* Find server entry */
    hu_json_value_t *token_obj = hu_json_object_get(root, server_name);
    if (!token_obj || token_obj->type != HU_JSON_OBJECT) {
        hu_json_free(alloc, root);
        return HU_ERR_NOT_FOUND;
    }

    /* Extract token fields */
    const char *access_token = hu_json_get_string(token_obj, "access_token");
    const char *refresh_token = hu_json_get_string(token_obj, "refresh_token");
    const char *token_type = hu_json_get_string(token_obj, "token_type");
    double expires_at_d = hu_json_get_number(token_obj, "expires_at", 0);

    /* Allocate and copy access_token (required) */
    if (access_token) {
        size_t len = strlen(access_token);
        out_token->access_token = (char *)alloc->alloc(alloc->ctx, len + 1);
        if (out_token->access_token) {
            strcpy(out_token->access_token, access_token);
            out_token->access_token_len = len;
        }
    }

    /* Allocate and copy refresh_token (optional) */
    if (refresh_token) {
        size_t len = strlen(refresh_token);
        out_token->refresh_token = (char *)alloc->alloc(alloc->ctx, len + 1);
        if (out_token->refresh_token) {
            strcpy(out_token->refresh_token, refresh_token);
            out_token->refresh_token_len = len;
        }
    }

    /* Allocate and copy token_type (optional) */
    if (token_type) {
        size_t len = strlen(token_type);
        out_token->token_type = (char *)alloc->alloc(alloc->ctx, len + 1);
        if (out_token->token_type) {
            strcpy(out_token->token_type, token_type);
            out_token->token_type_len = len;
        }
    }

    out_token->expires_at = (int64_t)expires_at_d;

    hu_json_free(alloc, root);
    return HU_OK;
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
