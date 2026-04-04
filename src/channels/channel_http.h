/*
 * channel_http.h — Shared HTTP helpers for messaging channels.
 * Eliminates duplicated URL building and auth header construction.
 */
#ifndef HUMAN_CHANNELS_CHANNEL_HTTP_H
#define HUMAN_CHANNELS_CHANNEL_HTTP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Build an Authorization header string into buf.
 * Format: "<scheme> <token>"
 *
 * @param buf        Output buffer
 * @param buf_size   Size of output buffer
 * @param scheme     Auth scheme (e.g., "Bot", "Bearer")
 * @param token      Token/credentials (pointer and length)
 * @param token_len  Length of token
 * @return           Number of bytes written, or -1 on error (buffer too small)
 */
int hu_channel_http_build_auth(char *buf, size_t buf_size, const char *scheme, const char *token,
                               size_t token_len);

/**
 * Build an API URL into buf.
 * Format: "<base><path>" where path is printf-style formatted.
 *
 * @param buf        Output buffer
 * @param buf_size   Size of output buffer
 * @param base       Base URL (e.g., "https://api.telegram.org/bot")
 * @param path_fmt   Path format string (printf-style, e.g., "%s/sendMessage")
 * @param ...        Format arguments (variadic)
 * @return           Number of bytes written, or -1 on error (buffer too small)
 */
int hu_channel_http_build_url(char *buf, size_t buf_size, const char *base, const char *path_fmt,
                              ...);

#ifdef __cplusplus
}
#endif

#endif /* HUMAN_CHANNELS_CHANNEL_HTTP_H */
