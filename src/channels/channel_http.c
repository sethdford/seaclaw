/*
 * channel_http.c — Shared HTTP helpers for messaging channels.
 * Eliminates duplicated URL building and auth header construction.
 */
#include "channel_http.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int hu_channel_http_build_auth(char *buf, size_t buf_size, const char *scheme, const char *token,
                               size_t token_len) {
    if (!buf || buf_size == 0 || !scheme || !token)
        return -1;

    int n = snprintf(buf, buf_size, "Authorization: %s %.*s", scheme, (int)token_len, token);
    if (n < 0 || (size_t)n >= buf_size)
        return -1;
    return n;
}

int hu_channel_http_build_url(char *buf, size_t buf_size, const char *base, const char *path_fmt,
                              ...) {
    if (!buf || buf_size == 0 || !base || !path_fmt)
        return -1;

    /* First, write the base URL */
    size_t base_len = strlen(base);
    if (base_len >= buf_size)
        return -1;
    memcpy(buf, base, base_len);

    /* Then, format the path into the remaining buffer */
    va_list args;
    va_start(args, path_fmt);
    int n = vsnprintf(buf + base_len, buf_size - base_len, path_fmt, args);
    va_end(args);

    if (n < 0 || (size_t)(base_len + n) >= buf_size)
        return -1;

    return (int)(base_len + n);
}
