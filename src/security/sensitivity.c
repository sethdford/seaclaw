#include "human/security/sensitivity.h"
#include <ctype.h>
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────────── */

static bool ci_contains(const char *haystack, size_t haystack_len, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen > haystack_len)
        return false;
    for (size_t i = 0; i <= haystack_len - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

static bool ends_with(const char *s, size_t s_len, const char *suffix) {
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s_len)
        return false;
    for (size_t i = 0; i < suffix_len; i++) {
        if (tolower((unsigned char)s[s_len - suffix_len + i]) !=
            tolower((unsigned char)suffix[i]))
            return false;
    }
    return true;
}

/* Check if a sequence of digits of exactly `count` length starts at pos */
static bool is_digit_run(const char *s, size_t len, size_t pos, size_t count) {
    if (pos + count > len)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (!isdigit((unsigned char)s[pos + i]))
            return false;
    }
    return true;
}

/* ── S3 keyword detection (secrets, keys, credentials) ────────────────── */

static const char *S3_KEYWORDS[] = {
    "private key", "secret key", "api key", "ssh key",
    "access token", "refresh token", "bearer token",
    "password is", "my password", "the password",
    "social security number", "bank account number",
    "credit card number", "routing number",
    "BEGIN RSA PRIVATE", "BEGIN EC PRIVATE", "BEGIN PRIVATE KEY",
    "BEGIN OPENSSH PRIVATE",
};
#define S3_KEYWORD_COUNT (sizeof(S3_KEYWORDS) / sizeof(S3_KEYWORDS[0]))

/* ── S2 keyword detection (PII, semi-private) ─────────────────────────── */

static const char *S2_KEYWORDS[] = {
    "date of birth", "social security", "phone number",
    "home address", "mailing address", "street address",
    "bank account", "credit card", "debit card",
    "driver's license", "drivers license", "passport number",
    "medical record", "health insurance", "diagnosis",
    "salary", "compensation", "pay stub", "tax return",
    "my ssn", "my email is",
};
#define S2_KEYWORD_COUNT (sizeof(S2_KEYWORDS) / sizeof(S2_KEYWORDS[0]))

/* ── Pattern detection ────────────────────────────────────────────────── */

/* SSN pattern: 3 digits, separator, 2 digits, separator, 4 digits */
static bool has_ssn_pattern(const char *msg, size_t len) {
    for (size_t i = 0; i + 10 < len; i++) {
        if (is_digit_run(msg, len, i, 3) &&
            (msg[i + 3] == '-' || msg[i + 3] == ' ') &&
            is_digit_run(msg, len, i + 4, 2) &&
            (msg[i + 6] == '-' || msg[i + 6] == ' ') &&
            is_digit_run(msg, len, i + 7, 4)) {
            return true;
        }
    }
    return false;
}

/* Credit card: 4 groups of 4 digits separated by spaces or dashes */
static bool has_credit_card_pattern(const char *msg, size_t len) {
    for (size_t i = 0; i + 18 < len; i++) {
        if (is_digit_run(msg, len, i, 4) &&
            (msg[i + 4] == '-' || msg[i + 4] == ' ') &&
            is_digit_run(msg, len, i + 5, 4) &&
            (msg[i + 9] == '-' || msg[i + 9] == ' ') &&
            is_digit_run(msg, len, i + 10, 4) &&
            (msg[i + 14] == '-' || msg[i + 14] == ' ') &&
            is_digit_run(msg, len, i + 15, 4)) {
            return true;
        }
    }
    return false;
}

/* Private key header: -----BEGIN ... PRIVATE KEY----- */
static bool has_private_key_header(const char *msg, size_t len) {
    return ci_contains(msg, len, "-----BEGIN") &&
           ci_contains(msg, len, "PRIVATE KEY-----");
}

/* ── S3 path patterns ─────────────────────────────────────────────────── */

static const char *S3_PATH_SEGMENTS[] = {
    ".ssh/",     ".env",      "credentials",
    "id_rsa",    "id_dsa",    "id_ecdsa",  "id_ed25519",
    ".gnupg/",   ".aws/",     "secrets/",  "vault/",
};
#define S3_PATH_SEGMENT_COUNT (sizeof(S3_PATH_SEGMENTS) / sizeof(S3_PATH_SEGMENTS[0]))

static const char *S3_PATH_EXTENSIONS[] = {
    ".pem", ".key", ".p12", ".pfx", ".jks", ".keystore",
};
#define S3_PATH_EXT_COUNT (sizeof(S3_PATH_EXTENSIONS) / sizeof(S3_PATH_EXTENSIONS[0]))

/* ── S3 tool patterns ─────────────────────────────────────────────────── */

static const char *S3_TOOL_SEGMENTS[] = {
    "vault", "keychain", "credential", "secret",
    "ssh", "gpg", "pgp",
};
#define S3_TOOL_SEGMENT_COUNT (sizeof(S3_TOOL_SEGMENTS) / sizeof(S3_TOOL_SEGMENTS[0]))

/* ── Public API ───────────────────────────────────────────────────────── */

hu_sensitivity_result_t hu_sensitivity_classify_message(const char *msg, size_t msg_len) {
    hu_sensitivity_result_t result = {HU_SENSITIVITY_S1, NULL};
    if (!msg || msg_len == 0)
        return result;

    /* S3: check for secret/key keywords */
    for (size_t i = 0; i < S3_KEYWORD_COUNT; i++) {
        if (ci_contains(msg, msg_len, S3_KEYWORDS[i])) {
            result.level = HU_SENSITIVITY_S3;
            result.reason = S3_KEYWORDS[i];
            return result;
        }
    }

    /* S3: check for private key headers */
    if (has_private_key_header(msg, msg_len)) {
        result.level = HU_SENSITIVITY_S3;
        result.reason = "private key block detected";
        return result;
    }

    /* S3: SSN pattern */
    if (has_ssn_pattern(msg, msg_len)) {
        result.level = HU_SENSITIVITY_S3;
        result.reason = "SSN pattern detected";
        return result;
    }

    /* S2: PII keywords */
    for (size_t i = 0; i < S2_KEYWORD_COUNT; i++) {
        if (ci_contains(msg, msg_len, S2_KEYWORDS[i])) {
            result.level = HU_SENSITIVITY_S2;
            result.reason = S2_KEYWORDS[i];
            return result;
        }
    }

    /* S2: credit card pattern */
    if (has_credit_card_pattern(msg, msg_len)) {
        result.level = HU_SENSITIVITY_S2;
        result.reason = "credit card pattern detected";
        return result;
    }

    return result;
}

hu_sensitivity_result_t hu_sensitivity_classify_path(const char *path, size_t path_len) {
    hu_sensitivity_result_t result = {HU_SENSITIVITY_S1, NULL};
    if (!path || path_len == 0)
        return result;

    for (size_t i = 0; i < S3_PATH_SEGMENT_COUNT; i++) {
        if (ci_contains(path, path_len, S3_PATH_SEGMENTS[i])) {
            result.level = HU_SENSITIVITY_S3;
            result.reason = S3_PATH_SEGMENTS[i];
            return result;
        }
    }

    for (size_t i = 0; i < S3_PATH_EXT_COUNT; i++) {
        if (ends_with(path, path_len, S3_PATH_EXTENSIONS[i])) {
            result.level = HU_SENSITIVITY_S3;
            result.reason = S3_PATH_EXTENSIONS[i];
            return result;
        }
    }

    return result;
}

hu_sensitivity_result_t hu_sensitivity_classify_tool(const char *tool_name, size_t tool_len) {
    hu_sensitivity_result_t result = {HU_SENSITIVITY_S1, NULL};
    if (!tool_name || tool_len == 0)
        return result;

    for (size_t i = 0; i < S3_TOOL_SEGMENT_COUNT; i++) {
        if (ci_contains(tool_name, tool_len, S3_TOOL_SEGMENTS[i])) {
            result.level = HU_SENSITIVITY_S3;
            result.reason = S3_TOOL_SEGMENTS[i];
            return result;
        }
    }

    return result;
}

hu_sensitivity_result_t hu_sensitivity_merge(const hu_sensitivity_result_t *a,
                                             const hu_sensitivity_result_t *b) {
    if (!a && !b)
        return (hu_sensitivity_result_t){HU_SENSITIVITY_S1, NULL};
    if (!a)
        return *b;
    if (!b)
        return *a;
    return a->level >= b->level ? *a : *b;
}

bool hu_sensitivity_requires_local(hu_sensitivity_level_t level) {
    return level >= HU_SENSITIVITY_S3;
}

const char *hu_sensitivity_level_str(hu_sensitivity_level_t level) {
    switch (level) {
    case HU_SENSITIVITY_S1:
        return "S1";
    case HU_SENSITIVITY_S2:
        return "S2";
    case HU_SENSITIVITY_S3:
        return "S3";
    default:
        return "unknown";
    }
}
