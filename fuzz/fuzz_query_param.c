/* libFuzzer harness for URL query parameter parsing.
 * Reproduces the query_param_value logic used in the gateway OAuth callback
 * handler. Since query_param_value is static in gateway.c, this harness
 * re-implements the same algorithm to fuzz it in isolation.
 * Goal: find crashes or OOB reads in query string parsing. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#define FUZZ_MAX_INPUT 4096

static const char *query_param_value_fuzz(const char *path, const char *key, size_t *out_len) {
    const char *q = strchr(path, '?');
    if (!q)
        return NULL;
    const char *p = q + 1;
    size_t klen = strlen(key);
    while (*p) {
        if (strncasecmp(p, key, klen) == 0 && p[klen] == '=') {
            p += klen + 1;
            const char *end = strchr(p, '&');
            if (end) {
                *out_len = (size_t)(end - p);
                return p;
            }
            *out_len = strlen(p);
            return p;
        }
        p = strchr(p, '&');
        if (!p)
            break;
        p++;
    }
    return NULL;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2 || size > FUZZ_MAX_INPUT)
        return 0;

    char buf[FUZZ_MAX_INPUT + 1];
    memcpy(buf, data, size);
    buf[size] = '\0';

    size_t out_len = 0;
    (void)query_param_value_fuzz(buf, "code", &out_len);
    (void)query_param_value_fuzz(buf, "state", &out_len);
    (void)query_param_value_fuzz(buf, "error", &out_len);

    return 0;
}
