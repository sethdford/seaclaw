#include "human/tools/validation.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

void hu_tool_validator_init(hu_tool_validator_t *v, hu_validation_level_t default_level) {
    if (!v)
        return;
    memset(v, 0, sizeof(*v));
    v->default_level = default_level;
}

hu_error_t hu_tool_validator_add_rule(hu_tool_validator_t *v, const hu_validation_rule_t *rule) {
    if (!v || !rule)
        return HU_ERR_INVALID_ARGUMENT;
    if (v->rule_count >= HU_VALIDATION_MAX_RULES)
        return HU_ERR_OUT_OF_MEMORY;
    v->rules[v->rule_count++] = *rule;
    return HU_OK;
}

static const hu_validation_rule_t *find_rule(const hu_tool_validator_t *v, const char *tool_name,
                                             size_t tool_name_len) {
    for (size_t i = 0; i < v->rule_count; i++) {
        size_t rlen = strlen(v->rules[i].tool_name);
        if (rlen == tool_name_len && memcmp(v->rules[i].tool_name, tool_name, rlen) == 0)
            return &v->rules[i];
    }
    return NULL;
}

static bool check_schema(const hu_validation_rule_t *rule, const hu_tool_result_t *result,
                         char *reason, size_t reason_size) {
    if (!result->success && !result->output)
        return true;

    const char *output = result->output;
    size_t output_len = result->output_len;

    if (rule->require_non_empty && (!output || output_len == 0)) {
        snprintf(reason, reason_size, "output is empty but non-empty required");
        return false;
    }

    if (rule->max_output_len > 0 && output_len > rule->max_output_len) {
        snprintf(reason, reason_size, "output length %zu exceeds max %zu", output_len,
                 rule->max_output_len);
        return false;
    }

    if (rule->min_output_len > 0 && output_len < rule->min_output_len) {
        snprintf(reason, reason_size, "output length %zu below min %zu", output_len,
                 rule->min_output_len);
        return false;
    }

    if (rule->expected_type[0] != '\0') {
        if (strcmp(rule->expected_type, "json") == 0 && output && output_len > 0) {
            if (output[0] != '{' && output[0] != '[') {
                snprintf(reason, reason_size, "expected JSON but output doesn't start with { or [");
                return false;
            }
        }
        if (strcmp(rule->expected_type, "number") == 0 && output && output_len > 0) {
            bool has_digit = false;
            for (size_t i = 0; i < output_len && !has_digit; i++) {
                if (output[i] >= '0' && output[i] <= '9')
                    has_digit = true;
            }
            if (!has_digit) {
                snprintf(reason, reason_size, "expected number but no digits found");
                return false;
            }
        }
    }

    return true;
}

static bool check_semantic(const hu_validation_rule_t *rule, const hu_tool_result_t *result,
                           char *reason, size_t reason_size) {
    if (rule->contains_pattern[0] == '\0')
        return true;

    if (!result->output || result->output_len == 0)
        return true;

    size_t plen = strlen(rule->contains_pattern);
    if (plen > result->output_len) {
        snprintf(reason, reason_size, "output missing required pattern '%.128s'",
                 rule->contains_pattern);
        return false;
    }

    for (size_t i = 0; i <= result->output_len - plen; i++) {
        if (memcmp(result->output + i, rule->contains_pattern, plen) == 0)
            return true;
    }

    snprintf(reason, reason_size, "output missing required pattern '%.128s'",
             rule->contains_pattern);
    return false;
}

hu_error_t hu_tool_validator_check(hu_tool_validator_t *v, const char *tool_name,
                                   size_t tool_name_len, const hu_tool_result_t *result,
                                   hu_validation_result_t *out) {
    if (!v || !result || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));
    out->passed = true;
    out->schema_ok = true;
    out->semantic_ok = true;

    const hu_validation_rule_t *rule = tool_name ? find_rule(v, tool_name, tool_name_len) : NULL;
    hu_validation_level_t level = rule ? rule->level : v->default_level;

    if (level == HU_VALIDATE_NONE) {
        v->total_checks++;
        return HU_OK;
    }

    hu_validation_rule_t default_rule;
    if (!rule) {
        memset(&default_rule, 0, sizeof(default_rule));
        default_rule.require_non_empty = true;
        rule = &default_rule;
    }

    v->total_checks++;

    if (level == HU_VALIDATE_SCHEMA || level == HU_VALIDATE_FULL) {
        out->schema_ok = check_schema(rule, result, out->reason, sizeof(out->reason));
        if (out->schema_ok)
            v->schema_pass++;
        else
            v->schema_fail++;
    }

    if ((level == HU_VALIDATE_SEMANTIC || level == HU_VALIDATE_FULL) && out->schema_ok) {
        out->semantic_ok = check_semantic(rule, result, out->reason, sizeof(out->reason));
        if (out->semantic_ok)
            v->semantic_pass++;
        else
            v->semantic_fail++;
    }

    out->passed = out->schema_ok && out->semantic_ok;
    return HU_OK;
}

size_t hu_tool_validator_report(const hu_tool_validator_t *v, char *buf, size_t buf_size) {
    if (!v || !buf || buf_size == 0)
        return 0;
    int n = snprintf(buf, buf_size,
                     "Tool Validation Report\n"
                     "=====================\n"
                     "Total checks: %zu\n"
                     "Schema: %zu pass, %zu fail\n"
                     "Semantic: %zu pass, %zu fail\n"
                     "Rules configured: %zu\n",
                     v->total_checks, v->schema_pass, v->schema_fail, v->semantic_pass,
                     v->semantic_fail, v->rule_count);
    if (n < 0)
        return 0;
    return (size_t)n < buf_size ? (size_t)n : buf_size - 1;
}

/* ── Path / URL validation (security) ─────────────────────────────── */

#define HU_PATH_MAX 4096
#define HU_URL_MAX  8192

hu_error_t hu_tool_validate_path(const char *path, const char *workspace_dir,
                                 size_t workspace_dir_len) {
    if (!path || path[0] == '\0')
        return HU_ERR_TOOL_VALIDATION;

    size_t len = strlen(path);
    if (len > HU_PATH_MAX)
        return HU_ERR_TOOL_VALIDATION;

    const char *p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.') {
            bool sep_before = (p == path || p[-1] == '/' || p[-1] == '\\');
            bool sep_after = (p[2] == '/' || p[2] == '\\' || p[2] == '\0');
            if (sep_before && sep_after)
                return HU_ERR_TOOL_VALIDATION;
        }
        if (p[0] == '%' && p[1] && p[2]) {
            char c1 = (char)tolower((unsigned char)p[1]);
            char c2 = (char)tolower((unsigned char)p[2]);
            if (c1 == '2' && c2 == 'e') {
                if (p[3] == '%' && p[4] && p[5]) {
                    char c3 = (char)tolower((unsigned char)p[4]);
                    char c4 = (char)tolower((unsigned char)p[5]);
                    if (c3 == '2' && c4 == 'e')
                        return HU_ERR_TOOL_VALIDATION;
                }
            }
            if (c1 == '2' && c2 == 'f')
                return HU_ERR_TOOL_VALIDATION;
            if (c1 == '5' && c2 == 'c')
                return HU_ERR_TOOL_VALIDATION;
        }
        p++;
    }

    if (workspace_dir && workspace_dir_len > 0) {
        bool is_absolute =
            (path[0] == '/') ||
            (len >= 3 &&
             ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
             path[1] == ':' && (path[2] == '/' || path[2] == '\\'));
        if (is_absolute) {
            size_t ws_len = workspace_dir_len;
            while (ws_len > 0 && workspace_dir[ws_len - 1] == '/')
                ws_len--;
            if (len < ws_len)
                return HU_ERR_TOOL_VALIDATION;
            if (memcmp(path, workspace_dir, ws_len) != 0)
                return HU_ERR_TOOL_VALIDATION;
            if (path[ws_len] != '\0' && path[ws_len] != '/' && path[ws_len] != '\\')
                return HU_ERR_TOOL_VALIDATION;
        }
    }

    return HU_OK;
}

static bool parse_ipv4_private(const char *host, size_t len) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    size_t i = 0;
    if (len == 0)
        return true;
    while (i < len && host[i] >= '0' && host[i] <= '9') {
        a = a * 10 + (unsigned)(host[i] - '0');
        i++;
    }
    if (i >= len || host[i] != '.')
        return true;
    i++;
    while (i < len && host[i] >= '0' && host[i] <= '9') {
        b = b * 10 + (unsigned)(host[i] - '0');
        i++;
    }
    if (i >= len || host[i] != '.')
        return true;
    i++;
    while (i < len && host[i] >= '0' && host[i] <= '9') {
        c = c * 10 + (unsigned)(host[i] - '0');
        i++;
    }
    if (i >= len || host[i] != '.')
        return true;
    i++;
    while (i < len && host[i] >= '0' && host[i] <= '9') {
        d = d * 10 + (unsigned)(host[i] - '0');
        i++;
    }
    if (i != len)
        return true;
    (void)c;
    (void)d;
    if (a == 127)
        return true;
    if (a == 10)
        return true;
    if (a == 172 && b >= 16 && b <= 31)
        return true;
    if (a == 192 && b == 168)
        return true;
    return false;
}

static bool parse_ipv6_private(const char *host, size_t len) {
    if (len < 2)
        return true;
    if ((len == 3 || len == 4) && host[0] == ':' && host[1] == ':') {
        if (len == 3 && host[2] == '1')
            return true;
        if (len == 4 && host[2] == '1' && (host[3] == '\0' || host[3] == ']'))
            return true;
    }
    if (len >= 2) {
        char c0 = (char)tolower((unsigned char)host[0]);
        char c1 = (char)tolower((unsigned char)host[1]);
        if (c0 == 'f' && c1 == 'd')
            return true;
        if (c0 == 'f' && c1 == 'e')
            return true;
    }
    return false;
}

static void extract_host(const char *url, size_t url_len, const char **out_host, size_t *out_len) {
    *out_host = NULL;
    *out_len = 0;
    if (url_len < 8)
        return;
    const char *p = url + 8;
    const char *end = url + url_len;
    const char *start = p;
    while (p < end && *p && *p != '/' && *p != '?' && *p != '#') {
        if (*p == '[') {
            const char *bracket = p;
            p++;
            while (p < end && *p && *p != ']')
                p++;
            if (p < end && *p == ']') {
                *out_host = bracket + 1;
                *out_len = (size_t)(p - bracket - 1);
                return;
            }
            return;
        }
        if (*p == ':') {
            *out_host = start;
            *out_len = (size_t)(p - start);
            return;
        }
        p++;
    }
    *out_host = start;
    *out_len = (size_t)(p - start);
}

hu_error_t hu_tool_validate_url(const char *url) {
    if (!url || url[0] == '\0')
        return HU_ERR_TOOL_VALIDATION;

    size_t len = strlen(url);
    if (len > HU_URL_MAX)
        return HU_ERR_TOOL_VALIDATION;

    if (len < 8)
        return HU_ERR_TOOL_VALIDATION;
    if (tolower((unsigned char)url[0]) != 'h' || tolower((unsigned char)url[1]) != 't' ||
        tolower((unsigned char)url[2]) != 't' || tolower((unsigned char)url[3]) != 'p' ||
        url[4] != 's' || url[5] != ':' || url[6] != '/' || url[7] != '/')
        return HU_ERR_TOOL_VALIDATION;

    const char *host;
    size_t host_len;
    extract_host(url, len, &host, &host_len);
    if (!host || host_len == 0)
        return HU_ERR_TOOL_VALIDATION;

    size_t i;
    bool might_ipv4 = true;
    for (i = 0; i < host_len && might_ipv4; i++) {
        char c = host[i];
        if (c >= '0' && c <= '9')
            continue;
        if (c == '.')
            continue;
        might_ipv4 = false;
    }
    if (might_ipv4 && host_len <= 15) {
        if (parse_ipv4_private(host, host_len))
            return HU_ERR_TOOL_VALIDATION;
        return HU_OK;
    }

    if (host_len > 2) {
        bool has_colon = false;
        for (i = 0; i < host_len; i++)
            if (host[i] == ':') {
                has_colon = true;
                break;
            }
        if (has_colon && parse_ipv6_private(host, host_len))
            return HU_ERR_TOOL_VALIDATION;
    }

    return HU_OK;
}
