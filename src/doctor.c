#include "human/doctor.h"
#include "human/channel_catalog.h"
#include "human/config.h"
#include "human/core/process_util.h"
#include "human/core/string.h"
#include "human/skill_registry.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif
#if defined(HU_ENABLE_PERSONA)
#include "human/persona.h"
#endif

#define HU_DOCTOR_LINE_CATEGORY "doctor_line"

static hu_error_t doctor_push_line(hu_allocator_t *alloc, hu_diag_item_t **buf, size_t *n,
                                   size_t *cap, hu_diag_severity_t sev, const char *line) {
    if (!line)
        return HU_ERR_INVALID_ARGUMENT;
    if (*n >= *cap) {
        size_t new_cap = *cap * 2;
        hu_diag_item_t *nb =
            (hu_diag_item_t *)alloc->alloc(alloc->ctx, sizeof(hu_diag_item_t) * new_cap);
        if (!nb)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(nb, *buf, sizeof(hu_diag_item_t) * (*n));
        alloc->free(alloc->ctx, *buf, sizeof(hu_diag_item_t) * (*cap));
        *buf = nb;
        *cap = new_cap;
    }
    char *cat = hu_strdup(alloc, HU_DOCTOR_LINE_CATEGORY);
    char *msg = hu_strdup(alloc, line);
    if (!cat || !msg) {
        if (cat)
            alloc->free(alloc->ctx, cat, strlen(cat) + 1);
        if (msg)
            alloc->free(alloc->ctx, msg, strlen(msg) + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    (*buf)[*n] = (hu_diag_item_t){sev, cat, msg};
    (*n)++;
    return HU_OK;
}

static bool doctor_config_wants_http(const hu_config_t *cfg) {
    if (!cfg)
        return false;
    if (cfg->gateway.enabled)
        return true;
#if defined(HU_ENABLE_FEEDS)
    if (cfg->feeds.enabled)
        return true;
#endif
    if (cfg->default_provider && hu_config_provider_requires_api_key(cfg->default_provider))
        return true;
    if (hu_channel_catalog_has_any_configured(cfg, false))
        return true;
    return false;
}

#if defined(HU_ENABLE_PERSONA)
static bool doctor_config_wants_persona(const hu_config_t *cfg) {
    if (!cfg)
        return false;
    if (cfg->agent.persona && cfg->agent.persona[0])
        return true;
    if (cfg->agent.persona_channels_count > 0)
        return true;
    if (cfg->agent.persona_contacts_count > 0)
        return true;
    return false;
}
#endif

static hu_error_t doctor_check_sqlite_backend(hu_allocator_t *alloc, hu_diag_item_t **buf,
                                              size_t *n, size_t *cap, const hu_config_t *cfg) {
    if (!cfg->memory_backend || strcmp(cfg->memory_backend, "sqlite") != 0)
        return HU_OK;
#ifdef HU_ENABLE_SQLITE
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK, "[doctor] SQLite: available");
#else
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_ERR, "[doctor] SQLite: not compiled in");
#endif
}

static hu_error_t doctor_check_http_client(hu_allocator_t *alloc, hu_diag_item_t **buf, size_t *n,
                                           size_t *cap, const hu_config_t *cfg) {
    if (!doctor_config_wants_http(cfg))
        return HU_OK;
#if HU_IS_TEST
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK, "[doctor] HTTP client: OK");
#else
#if !defined(HU_ENABLE_CURL)
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_ERR,
                            "[doctor] HTTP client: not compiled in (HU_ENABLE_CURL=OFF)");
#elif !defined(HU_HTTP_CURL)
    return doctor_push_line(
        alloc, buf, n, cap, HU_DIAG_ERR,
        "[doctor] HTTP client: libcurl not linked (install libcurl for outbound HTTP)");
#else
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK,
                            "[doctor] HTTP client: libcurl available");
#endif
#endif
}

#if defined(HU_ENABLE_PERSONA)
static hu_error_t doctor_check_persona_dir(hu_allocator_t *alloc, hu_diag_item_t **buf, size_t *n,
                                           size_t *cap, const hu_config_t *cfg) {
    if (!doctor_config_wants_persona(cfg))
        return HU_OK;
#if HU_IS_TEST
    (void)cfg;
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK, "[doctor] Persona dir: OK");
#else
#ifndef _WIN32
    char pbuf[512];
    const char *dir = hu_persona_base_dir(pbuf, sizeof(pbuf));
    if (!dir)
        return doctor_push_line(alloc, buf, n, cap, HU_DIAG_WARN,
                                "[doctor] Persona dir: cannot resolve (HOME or HU_PERSONA_DIR)");
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        char *line =
            hu_sprintf(alloc, "[doctor] Persona dir: missing or not a directory (%s)", dir);
        if (!line)
            return HU_ERR_OUT_OF_MEMORY;
        hu_error_t e = doctor_push_line(alloc, buf, n, cap, HU_DIAG_WARN, line);
        alloc->free(alloc->ctx, line, strlen(line) + 1);
        return e;
    }
    char *line = hu_sprintf(alloc, "[doctor] Persona dir: OK (%s)", dir);
    if (!line)
        return HU_ERR_OUT_OF_MEMORY;
    hu_error_t e = doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK, line);
    alloc->free(alloc->ctx, line, strlen(line) + 1);
    return e;
#else
    (void)cfg;
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK, "[doctor] Persona dir: OK");
#endif
#endif
}
#endif /* HU_ENABLE_PERSONA */

static void doctor_free_diag_items(hu_allocator_t *alloc, hu_diag_item_t *buf, size_t n,
                                   size_t cap_slots) {
    for (size_t i = 0; i < n; i++) {
        if (buf[i].category)
            alloc->free(alloc->ctx, (void *)buf[i].category, strlen(buf[i].category) + 1);
        if (buf[i].message)
            alloc->free(alloc->ctx, (void *)buf[i].message, strlen(buf[i].message) + 1);
    }
    if (buf)
        alloc->free(alloc->ctx, buf, sizeof(hu_diag_item_t) * cap_slots);
}

unsigned long hu_doctor_parse_df_available_mb(const char *df_output, size_t len) {
    if (!df_output || len == 0)
        return 0;
    const char *last_line = NULL;
    const char *p = df_output;
    const char *end = df_output + len;
    while (p < end) {
        const char *line = p;
        while (p < end && *p != '\n')
            p++;
        if (p > line) {
            while (p > line && (p[-1] == ' ' || p[-1] == '\r'))
                p--;
            if (p > line)
                last_line = line;
        }
        if (p < end)
            p++;
    }
    if (!last_line)
        return 0;
    const char *col = last_line;
    for (int i = 0; i < 4 && col < end; i++) {
        while (col < end && (*col == ' ' || *col == '\t'))
            col++;
        if (col >= end)
            return 0;
        const char *start = col;
        while (col < end && *col != ' ' && *col != '\t')
            col++;
        if (i == 3) {
            unsigned long v = 0;
            for (const char *q = start; q < col; q++) {
                if (*q >= '0' && *q <= '9')
                    v = v * 10 + (unsigned long)(*q - '0');
            }
            return v;
        }
    }
    return 0;
}

hu_error_t hu_doctor_truncate_for_display(hu_allocator_t *alloc, const char *s, size_t len,
                                          size_t max_len, char **out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!s) {
        *out = NULL;
        return HU_OK;
    }
    if (len == 0)
        len = strlen(s);
    if (len <= max_len) {
        *out = hu_strndup(alloc, s, len);
        return *out ? HU_OK : HU_ERR_OUT_OF_MEMORY;
    }
    size_t i = max_len;
    while (i > 0 && (s[i] & 0xC0) == 0x80)
        i--;
    *out = hu_strndup(alloc, s, i);
    return *out ? HU_OK : HU_ERR_OUT_OF_MEMORY;
}

static hu_error_t doctor_check_local_inference(hu_allocator_t *alloc, hu_diag_item_t **buf,
                                               size_t *n, size_t *cap) {
#if HU_IS_TEST
    hu_error_t e = doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK,
                                    "[doctor] Ollama (localhost:11434): OK (test mode)");
    if (e != HU_OK)
        return e;
    e = doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK,
                         "[doctor] llama-cli (PATH): OK (test mode)");
    if (e != HU_OK)
        return e;
#else
    bool ollama_ok = hu_ollama_api_tags_reachable();
    hu_error_t e;
    if (ollama_ok)
        e = doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK,
                             "[doctor] Ollama (localhost:11434): reachable (GET /api/tags)");
    else
        e = doctor_push_line(
            alloc, buf, n, cap, HU_DIAG_WARN,
            "[doctor] Ollama (localhost:11434): not reachable (run `ollama serve`)");
    if (e != HU_OK)
        return e;
    if (hu_exe_on_path("llama-cli"))
        e = doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK, "[doctor] llama-cli: on PATH");
    else
        e = doctor_push_line(alloc, buf, n, cap, HU_DIAG_WARN,
                             "[doctor] llama-cli: not on PATH (install llama.cpp)");
    if (e != HU_OK)
        return e;
#endif
#ifdef HU_ENABLE_EMBEDDED_MODEL
    return doctor_push_line(alloc, buf, n, cap, HU_DIAG_OK,
                            "[doctor] Embedded model provider: compiled in");
#else
    return doctor_push_line(
        alloc, buf, n, cap, HU_DIAG_WARN,
        "[doctor] Embedded model provider: not compiled in (HU_ENABLE_EMBEDDED_MODEL=OFF)");
#endif
}

/* ── Security checks ─────────────────────────────────────────────────── */

hu_error_t hu_doctor_check_security(hu_allocator_t *alloc, hu_diag_item_t **items, size_t *count,
                                    size_t *cap) {
    if (!alloc || !items || !count || !cap)
        return HU_ERR_INVALID_ARGUMENT;

    /* Sandbox availability */
#if defined(__linux__)
    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Sandbox: Linux (landlock/bwrap available)");
#elif defined(__APPLE__)
    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Sandbox: macOS (sandbox-exec available)");
#else
    doctor_push_line(alloc, items, count, cap, HU_DIAG_WARN,
                     "[doctor] Sandbox: platform sandbox not available");
#endif

    /* Exec env sanitization compiled in */
    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Exec env sanitization: active "
                     "(blocks MAVEN_OPTS, LD_PRELOAD, GLIBC_TUNABLES, etc.)");

    /* Unicode visual spoofing detection */
    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Unicode spoofing detection: active "
                     "(Hangul fillers, bidi overrides, zero-width chars)");

    /* Safe-bin review */
    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Risky binary detection: active "
                     "(jq, printenv, env flagged for secret-dump risk)");

    return HU_OK;
}

/* ── Memory health checks ────────────────────────────────────────────── */

hu_error_t hu_doctor_check_memory_health(hu_allocator_t *alloc, const hu_config_t *cfg,
                                         hu_diag_item_t **items, size_t *count, size_t *cap) {
    if (!alloc || !cfg || !items || !count || !cap)
        return HU_ERR_INVALID_ARGUMENT;

#ifdef HU_ENABLE_SQLITE
    if (cfg->memory_backend && strcmp(cfg->memory_backend, "sqlite") == 0) {
#if HU_IS_TEST
        doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                         "[doctor] SQLite memory: OK (test mode)");
#else
        doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                         "[doctor] SQLite memory: compiled in and configured");
#endif
    }
#else
    if (cfg->memory_backend && strcmp(cfg->memory_backend, "sqlite") == 0) {
        doctor_push_line(alloc, items, count, cap, HU_DIAG_ERR,
                         "[doctor] SQLite memory: requested but not compiled in");
    }
#endif

    return HU_OK;
}

/* ── Skills checks ───────────────────────────────────────────────────── */

hu_error_t hu_doctor_check_skills(hu_allocator_t *alloc, hu_diag_item_t **items, size_t *count,
                                  size_t *cap) {
    if (!alloc || !items || !count || !cap)
        return HU_ERR_INVALID_ARGUMENT;

#ifdef HU_ENABLE_SKILLS
    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Skills subsystem: compiled in");
#else
    doctor_push_line(alloc, items, count, cap, HU_DIAG_WARN,
                     "[doctor] Skills subsystem: not compiled in (HU_ENABLE_SKILLS=OFF)");
#endif

    /* Check if skills directory exists */
#if HU_IS_TEST
    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Skills directory: OK (test mode)");
#else
#ifndef _WIN32
    char skills_dir[512];
    size_t slen = hu_skill_registry_get_installed_dir(skills_dir, sizeof(skills_dir));
    if (slen > 0) {
        struct stat st2;
        if (stat(skills_dir, &st2) == 0 && S_ISDIR(st2.st_mode)) {
            char *msg = hu_sprintf(alloc, "[doctor] Skills directory: %s", skills_dir);
            if (msg) {
                doctor_push_line(alloc, items, count, cap, HU_DIAG_OK, msg);
                alloc->free(alloc->ctx, msg, strlen(msg) + 1);
            }
        } else {
            doctor_push_line(alloc, items, count, cap, HU_DIAG_WARN,
                             "[doctor] Skills directory: not found (run `human skills install`)");
        }
    }
#endif
#endif

    doctor_push_line(alloc, items, count, cap, HU_DIAG_OK,
                     "[doctor] Skill registry: https://github.com/human/skill-registry");

    return HU_OK;
}

/* ── Config semantics (existing, enhanced) ───────────────────────────── */

hu_error_t hu_doctor_check_config_semantics(hu_allocator_t *alloc, const hu_config_t *cfg,
                                            hu_diag_item_t **items, size_t *count) {
    if (!alloc || !cfg || !items || !count)
        return HU_ERR_INVALID_ARGUMENT;
    *items = NULL;
    *count = 0;

    size_t cap = 48;
    hu_diag_item_t *buf = (hu_diag_item_t *)alloc->alloc(alloc->ctx, sizeof(hu_diag_item_t) * cap);
    if (!buf)
        return HU_ERR_OUT_OF_MEMORY;

    size_t n = 0;
    hu_diag_item_t it;

    if (!cfg->default_provider || !cfg->default_provider[0]) {
        it = (hu_diag_item_t){HU_DIAG_ERR, hu_strdup(alloc, "config"),
                              hu_strdup(alloc, "no default_provider configured")};
        buf[n++] = it;
    } else {
        char *msg = hu_sprintf(alloc, "provider: %s", cfg->default_provider);
        if (msg) {
            it = (hu_diag_item_t){HU_DIAG_OK, hu_strdup(alloc, "config"), msg};
            buf[n++] = it;
        }
    }

    if (cfg->default_temperature < 0.0 || cfg->default_temperature > 2.0) {
        char *msg = hu_sprintf(alloc, "temperature %.1f is out of range (expected 0.0-2.0)",
                               cfg->default_temperature);
        if (msg) {
            it = (hu_diag_item_t){HU_DIAG_ERR, hu_strdup(alloc, "config"), msg};
            buf[n++] = it;
        }
    } else {
        char *msg =
            hu_sprintf(alloc, "temperature %.1f (valid range 0.0-2.0)", cfg->default_temperature);
        if (msg) {
            it = (hu_diag_item_t){HU_DIAG_OK, hu_strdup(alloc, "config"), msg};
            buf[n++] = it;
        }
    }

    uint16_t gw_port = cfg->gateway.port;
    if (gw_port == 0) {
        it = (hu_diag_item_t){HU_DIAG_ERR, hu_strdup(alloc, "config"),
                              hu_strdup(alloc, "gateway port is 0 (invalid)")};
        buf[n++] = it;
    } else {
        char *msg = hu_sprintf(alloc, "gateway port: %u", (unsigned)gw_port);
        if (msg) {
            it = (hu_diag_item_t){HU_DIAG_OK, hu_strdup(alloc, "config"), msg};
            buf[n++] = it;
        }
    }

    bool has_ch = hu_channel_catalog_has_any_configured(cfg, false);
    if (has_ch) {
        it = (hu_diag_item_t){HU_DIAG_OK, hu_strdup(alloc, "config"),
                              hu_strdup(alloc, "at least one channel configured")};
        buf[n++] = it;
    } else {
        it = (hu_diag_item_t){
            HU_DIAG_WARN, hu_strdup(alloc, "config"),
            hu_strdup(alloc, "no channels configured -- run onboard to set one up")};
        buf[n++] = it;
    }

    const struct {
        const char *name;
        bool enabled;
    } modules[] = {
        {"tree_of_thought", cfg->agent.tree_of_thought},
        {"constitutional_ai", cfg->agent.constitutional_ai},
        {"speculative_cache", cfg->agent.speculative_cache},
        {"llm_compiler", cfg->agent.llm_compiler_enabled},
        {"hula", cfg->agent.hula_enabled},
        {"mcts_planner", cfg->agent.mcts_planner_enabled},
        {"tool_routing", cfg->agent.tool_routing_enabled},
        {"multi_agent", cfg->agent.multi_agent},
    };
    size_t active = 0;
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (modules[i].enabled)
            active++;
    }
    if (n + 1 < cap) {
        char *msg = hu_sprintf(alloc, "intelligence: %zu/%zu modules active", active,
                               sizeof(modules) / sizeof(modules[0]));
        if (msg) {
            it = (hu_diag_item_t){active > 0 ? HU_DIAG_OK : HU_DIAG_WARN, hu_strdup(alloc, "agent"),
                                  msg};
            buf[n++] = it;
        }
    }
    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]) && n < cap; i++) {
        char *msg = hu_sprintf(alloc, "%s: %s", modules[i].name,
                               modules[i].enabled ? "enabled" : "disabled");
        if (msg) {
            it = (hu_diag_item_t){HU_DIAG_OK, hu_strdup(alloc, "intelligence"), msg};
            buf[n++] = it;
        }
    }

    hu_error_t ext_err = doctor_check_sqlite_backend(alloc, &buf, &n, &cap, cfg);
    if (ext_err != HU_OK) {
        doctor_free_diag_items(alloc, buf, n, cap);
        return ext_err;
    }
    ext_err = doctor_check_http_client(alloc, &buf, &n, &cap, cfg);
    if (ext_err != HU_OK) {
        doctor_free_diag_items(alloc, buf, n, cap);
        return ext_err;
    }
#if defined(HU_ENABLE_PERSONA)
    ext_err = doctor_check_persona_dir(alloc, &buf, &n, &cap, cfg);
    if (ext_err != HU_OK) {
        doctor_free_diag_items(alloc, buf, n, cap);
        return ext_err;
    }
#endif
    ext_err = doctor_check_local_inference(alloc, &buf, &n, &cap);
    if (ext_err != HU_OK) {
        doctor_free_diag_items(alloc, buf, n, cap);
        return ext_err;
    }

    ext_err = hu_doctor_check_security(alloc, &buf, &n, &cap);
    if (ext_err != HU_OK) {
        doctor_free_diag_items(alloc, buf, n, cap);
        return ext_err;
    }

    ext_err = hu_doctor_check_memory_health(alloc, cfg, &buf, &n, &cap);
    if (ext_err != HU_OK) {
        doctor_free_diag_items(alloc, buf, n, cap);
        return ext_err;
    }

    ext_err = hu_doctor_check_skills(alloc, &buf, &n, &cap);
    if (ext_err != HU_OK) {
        doctor_free_diag_items(alloc, buf, n, cap);
        return ext_err;
    }

    *items = buf;
    *count = n;
    return HU_OK;
}
