#ifndef HU_OAUTH_H
#define HU_OAUTH_H

#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ──────────────────────────────────────────────────────────────────────────
 * OAuth 2.0 with PKCE (Proof Key for Code Exchange) for MCP server authentication.
 *
 * Implements RFC 7636 (PKCE) with S256 challenge method for remote MCP servers
 * that require OAuth authentication (e.g., GitHub, Slack).
 * ────────────────────────────────────────────────────────────────────────── */

/**
 * OAuth token structure holding access token and optional refresh token.
 */
typedef struct hu_oauth_token {
    char *access_token;
    size_t access_token_len;
    char *refresh_token;      /* optional */
    size_t refresh_token_len;
    int64_t expires_at;       /* unix timestamp, 0 = never */
    char *token_type;         /* "Bearer" */
    size_t token_type_len;
} hu_oauth_token_t;

/**
 * PKCE state: verifier (for request) and challenge (for server).
 * Verifier is 43-128 random characters from [A-Za-z0-9-._~].
 * Challenge is base64url(sha256(verifier)).
 */
typedef struct hu_oauth_pkce {
    char verifier[128];       /* nul-terminated verifier string */
    char challenge[64];       /* nul-terminated base64url challenge */
} hu_oauth_pkce_t;

/**
 * OAuth configuration for a server.
 * All string fields are expected to be caller-owned or arena-allocated.
 */
typedef struct hu_oauth_config {
    char *client_id;
    size_t client_id_len;
    char *auth_url;           /* authorization endpoint */
    size_t auth_url_len;
    char *token_url;          /* token exchange endpoint */
    size_t token_url_len;
    char *redirect_uri;       /* callback URI (e.g., "http://localhost:8888/callback") */
    size_t redirect_uri_len;
    char *scopes;             /* space-separated scopes */
    size_t scopes_len;
} hu_oauth_config_t;

/* ── PKCE ─────────────────────────────────────────────────────────────── */

/**
 * Generate a new PKCE verifier (43-128 random characters from [A-Za-z0-9-._~]).
 * Result is stored in out->verifier (nul-terminated).
 */
hu_error_t hu_mcp_oauth_pkce_generate(hu_oauth_pkce_t *out);

/**
 * Compute S256 challenge: base64url(sha256(verifier)).
 * Caller must provide verifier and pre-allocated challenge buffer.
 * Result stored in challenge_out (nul-terminated).
 */
hu_error_t hu_mcp_oauth_pkce_challenge(const char *verifier, char *challenge_out,
                                   size_t challenge_size);

/* ── Authorization URL ────────────────────────────────────────────────── */

/**
 * Build authorization URL with PKCE parameters.
 * URL format: {auth_url}?client_id={cid}&redirect_uri={uri}&scope={scopes}&state={state}&code_challenge={challenge}&code_challenge_method=S256
 *
 * @param alloc       Allocator for URL string
 * @param config      OAuth configuration
 * @param pkce        PKCE state (for challenge)
 * @param state       CSRF state token (caller-provided)
 * @param out_url     Receives allocated URL string (caller frees with alloc)
 * @param out_url_len Receives URL length
 * @return HU_OK on success
 */
hu_error_t hu_mcp_oauth_build_auth_url(hu_allocator_t *alloc, const hu_oauth_config_t *config,
                                       const hu_oauth_pkce_t *pkce, const char *state,
                                       char **out_url, size_t *out_url_len);

/* ── Token Exchange ───────────────────────────────────────────────────── */

/**
 * Exchange authorization code for tokens via HTTP POST to token_url.
 * Makes request: POST {token_url} with form data:
 *   grant_type=authorization_code
 *   code={code}
 *   client_id={client_id}
 *   code_verifier={pkce.verifier}
 *   redirect_uri={redirect_uri}
 *
 * Expects JSON response with fields:
 *   access_token (required)
 *   refresh_token (optional)
 *   expires_in (optional, seconds from now)
 *   token_type (optional, default "Bearer")
 *
 * @param alloc       Allocator for token fields
 * @param config      OAuth configuration
 * @param pkce        PKCE state (for verifier)
 * @param code        Authorization code from OAuth provider
 * @param out_token   Receives token (caller must call hu_mcp_oauth_token_free)
 * @return HU_OK on success, HU_ERR_IO on HTTP error, HU_ERR_PARSE on invalid response
 */
hu_error_t hu_mcp_oauth_exchange_code(hu_allocator_t *alloc, const hu_oauth_config_t *config,
                                  const hu_oauth_pkce_t *pkce, const char *code,
                                  hu_oauth_token_t *out_token);

/* ── Token Lifecycle ──────────────────────────────────────────────────── */

/**
 * Check if token is expired.
 * Returns true if expires_at is set and <= current unix timestamp.
 */
bool hu_mcp_oauth_token_is_expired(const hu_oauth_token_t *token);

/**
 * Save token to ~/.human/oauth_tokens.json under [server_name] key.
 * File format is JSON: { "server1": {...}, "server2": {...} }
 *
 * @param alloc       Allocator for temporary buffers
 * @param path        Path to token cache file (typically ~/.human/oauth_tokens.json)
 * @param server_name Server identifier for this token
 * @param token       Token to save
 * @return HU_OK on success
 */
hu_error_t hu_mcp_oauth_token_save(hu_allocator_t *alloc, const char *path,
                                const char *server_name, const hu_oauth_token_t *token);

/**
 * Load token from ~/.human/oauth_tokens.json under [server_name] key.
 *
 * @param alloc       Allocator for token fields (caller must call hu_mcp_oauth_token_free)
 * @param path        Path to token cache file
 * @param server_name Server identifier
 * @param out_token   Receives token
 * @return HU_OK on success, HU_ERR_NOT_FOUND if server_name not in file
 */
hu_error_t hu_mcp_oauth_token_load(hu_allocator_t *alloc, const char *path,
                                const char *server_name, hu_oauth_token_t *out_token);

/**
 * Free all allocated fields in token (access_token, refresh_token, token_type).
 * Safe to call multiple times on same token.
 */
void hu_mcp_oauth_token_free(hu_allocator_t *alloc, hu_oauth_token_t *token);

/* ── Base64url Encoding ───────────────────────────────────────────────── */

/**
 * Encode binary data to base64url format.
 * Base64url uses - instead of +, _ instead of /, and no padding.
 *
 * @param input       Binary data to encode
 * @param input_len   Length of binary data
 * @param output      Buffer for output (caller must allocate)
 * @param output_size Size of output buffer
 * @param out_len     Receives length of encoded data (excluding nul terminator)
 * @return HU_OK on success, HU_ERR_INVALID_ARGUMENT if output too small
 */
hu_error_t hu_mcp_base64url_encode(const uint8_t *input, size_t input_len, char *output,
                                size_t output_size, size_t *out_len);

#endif /* HU_OAUTH_H */
