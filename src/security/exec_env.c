/*
 * Exec environment sanitization — blocks dangerous env vars and detects
 * visual spoofing in command approval prompts.
 */

#include "human/security/exec_env.h"
#include <stdint.h>
#include <string.h>

static const char *const BLOCKED_ENV_VARS[] = {
    /* JVM build-tool injection */
    "MAVEN_OPTS",
    "SBT_OPTS",
    "GRADLE_OPTS",
    "ANT_OPTS",
    "GRADLE_USER_HOME",

    /* glibc exploitation */
    "GLIBC_TUNABLES",

    /* .NET dependency hijack */
    "DOTNET_ADDITIONAL_DEPS",
    "DOTNET_STARTUP_HOOKS",

    /* Dynamic linker injection */
    "LD_PRELOAD",
    "LD_LIBRARY_PATH",
    "LD_AUDIT",
    "DYLD_INSERT_LIBRARIES",
    "DYLD_FORCE_FLAT_NAMESPACE",
    "DYLD_LIBRARY_PATH",
    "DYLD_FRAMEWORK_PATH",

    /* Python injection */
    "PYTHONSTARTUP",

    /* Node.js injection */
    "NODE_OPTIONS",

    /* Ruby injection */
    "RUBYOPT",

    /* Perl injection */
    "PERL5OPT",
    "PERL5LIB",
};

static const size_t BLOCKED_ENV_COUNT = sizeof(BLOCKED_ENV_VARS) / sizeof(BLOCKED_ENV_VARS[0]);

bool hu_exec_env_is_blocked(const char *name, size_t name_len) {
    if (!name || name_len == 0)
        return false;
    for (size_t i = 0; i < BLOCKED_ENV_COUNT; i++) {
        size_t blen = strlen(BLOCKED_ENV_VARS[i]);
        if (blen == name_len && memcmp(name, BLOCKED_ENV_VARS[i], blen) == 0)
            return true;
    }
    return false;
}

/* Binaries that can leak secrets when given certain arguments. */
static const char *const RISKY_BINS[] = {
    "jq", /* jq -n env dumps all env vars */
    "printenv",
    "env",
};

static const size_t RISKY_BINS_COUNT = sizeof(RISKY_BINS) / sizeof(RISKY_BINS[0]);

bool hu_exec_safe_bin_check(const char *bin_name, size_t name_len) {
    if (!bin_name || name_len == 0)
        return true;

    /* Strip path prefix to get basename */
    const char *base = bin_name;
    for (size_t i = 0; i < name_len; i++) {
        if (bin_name[i] == '/')
            base = bin_name + i + 1;
    }
    size_t base_len = name_len - (size_t)(base - bin_name);

    for (size_t i = 0; i < RISKY_BINS_COUNT; i++) {
        size_t rlen = strlen(RISKY_BINS[i]);
        if (rlen == base_len && memcmp(base, RISKY_BINS[i], rlen) == 0)
            return false;
    }
    return true;
}

bool hu_exec_has_visual_spoofing(const char *text, size_t text_len) {
    if (!text || text_len == 0)
        return false;

    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *end = p + text_len;

    while (p < end) {
        unsigned char c = *p;

        if (c < 0x80) {
            p++;
            continue;
        }

        /* Decode UTF-8 to get codepoint */
        uint32_t cp = 0;
        size_t seq_len = 0;

        if ((c & 0xE0) == 0xC0) {
            seq_len = 2;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            seq_len = 3;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            seq_len = 4;
            cp = c & 0x07;
        } else {
            p++;
            continue;
        }

        if (p + seq_len > end) {
            p++;
            continue;
        }

        for (size_t i = 1; i < seq_len; i++) {
            if ((p[i] & 0xC0) != 0x80) {
                seq_len = 1;
                break;
            }
            cp = (cp << 6) | (p[i] & 0x3F);
        }

        /* Zero-width characters */
        if (cp == 0x200B || /* ZERO WIDTH SPACE */
            cp == 0x200C || /* ZERO WIDTH NON-JOINER */
            cp == 0x200D || /* ZERO WIDTH JOINER */
            cp == 0xFEFF)   /* ZERO WIDTH NO-BREAK SPACE (BOM) */
            return true;

        /* Bidi overrides that can reverse displayed text */
        if (cp == 0x202A || /* LEFT-TO-RIGHT EMBEDDING */
            cp == 0x202B || /* RIGHT-TO-LEFT EMBEDDING */
            cp == 0x202C || /* POP DIRECTIONAL FORMATTING */
            cp == 0x202D || /* LEFT-TO-RIGHT OVERRIDE */
            cp == 0x202E || /* RIGHT-TO-LEFT OVERRIDE */
            cp == 0x2066 || /* LEFT-TO-RIGHT ISOLATE */
            cp == 0x2067 || /* RIGHT-TO-LEFT ISOLATE */
            cp == 0x2068 || /* FIRST STRONG ISOLATE */
            cp == 0x2069)   /* POP DIRECTIONAL ISOLATE */
            return true;

        /* Blank Hangul fillers — visually blank but non-space characters */
        if (cp == 0x3164 || /* HANGUL FILLER */
            cp == 0x115F || /* HANGUL CHOSEONG FILLER */
            cp == 0x1160 || /* HANGUL JUNGSEONG FILLER */
            cp == 0xFFA0)   /* HALFWIDTH HANGUL FILLER */
            return true;

        /* Invisible formatting characters */
        if (cp == 0x00AD || /* SOFT HYPHEN */
            cp == 0x034F || /* COMBINING GRAPHEME JOINER */
            cp == 0x061C || /* ARABIC LETTER MARK */
            cp == 0x180E)   /* MONGOLIAN VOWEL SEPARATOR */
            return true;

        p += seq_len;
    }

    return false;
}

size_t hu_exec_env_sanitize(char **env_pairs, size_t count) {
    if (!env_pairs || count == 0)
        return 0;

    size_t write = 0;
    for (size_t i = 0; i < count; i++) {
        if (!env_pairs[i])
            continue;
        const char *eq = strchr(env_pairs[i], '=');
        size_t name_len = eq ? (size_t)(eq - env_pairs[i]) : strlen(env_pairs[i]);
        if (!hu_exec_env_is_blocked(env_pairs[i], name_len)) {
            env_pairs[write++] = env_pairs[i];
        }
    }
    return write;
}
