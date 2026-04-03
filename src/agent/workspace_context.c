#include "human/agent/workspace_context.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HU_IS_TEST
#include <sys/stat.h>
#include <unistd.h>
#endif

/* Helper: read first line of a file for basic parsing */
static char *workspace_read_file_line(hu_allocator_t *alloc, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    char line[512] = {0};
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return NULL;
    }
    fclose(f);

    /* Remove trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
        line[len - 1] = '\0';

    return hu_strndup(alloc, line, strlen(line));
}

/* Extract JSON string value for a key (very basic parsing for small fields) */
static char *workspace_extract_json_field(hu_allocator_t *alloc, const char *json, size_t json_len,
                                          const char *key) {
    (void)json_len;
    if (!json || !key)
        return NULL;

    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char *pos = strstr(json, search_key);
    if (!pos)
        return NULL;

    pos += strlen(search_key);
    while (*pos && (*pos == ':' || *pos == ' '))
        pos++;

    if (*pos != '"')
        return NULL;

    pos++; /* skip opening quote */
    const char *end = strchr(pos, '"');
    if (!end)
        return NULL;

    size_t len = end - pos;
    return hu_strndup(alloc, pos, len);
}

/* Extract TOML value (simplistic: key = "value" or key = "value" at line start) */
static char *workspace_extract_toml_field(hu_allocator_t *alloc, const char *content,
                                          size_t content_len, const char *key) {
    (void)content_len;
    if (!content || !key)
        return NULL;

    /* Search for key = "value" pattern */
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "%s", key);

    const char *pos = strstr(content, pattern);
    if (!pos)
        return NULL;

    /* Find the opening quote */
    pos += strlen(pattern);
    while (*pos && *pos != '"')
        pos++;

    if (*pos != '"')
        return NULL;

    pos++; /* skip opening quote */
    const char *end = strchr(pos, '"');
    if (!end)
        return NULL;

    size_t len = end - pos;
    return hu_strndup(alloc, pos, len);
}

/* Read file content (up to max_len) */
static char *workspace_read_file(hu_allocator_t *alloc, const char *path, size_t max_len) {
    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    char *buf = (char *)alloc->alloc(alloc->ctx, max_len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, max_len, f);
    buf[n] = '\0';
    fclose(f);

    return buf;
}

/* Detect Node.js project: look for package.json */
static hu_error_t workspace_detect_nodejs(hu_allocator_t *alloc, const char *workspace_dir,
                                          hu_workspace_context_t *out) {
    char path[512];
    snprintf(path, sizeof(path), "%s/package.json", workspace_dir);

#if HU_IS_TEST
    if (access(path, F_OK) != 0)
        return HU_ERR_NOT_FOUND;
#else
    FILE *f = fopen(path, "r");
    if (!f)
        return HU_ERR_NOT_FOUND;
    fclose(f);
#endif

    char *content = workspace_read_file(alloc, path, 4096);
    if (!content)
        return HU_ERR_IO;

    out->project_type = hu_strndup(alloc, "nodejs", 6);
    out->project_type_len = 6;

    /* Extract name and version */
    char *name = workspace_extract_json_field(alloc, content, strlen(content), "name");
    char *version = workspace_extract_json_field(alloc, content, strlen(content), "version");

    if (name) {
        out->project_name = name;
        out->project_name_len = strlen(name);
    }

    /* Build summary */
    char summary[256] = {0};
    if (name && version) {
        snprintf(summary, sizeof(summary), "Node.js project '%s' v%s", name, version);
    } else if (name) {
        snprintf(summary, sizeof(summary), "Node.js project '%s'", name);
    } else {
        snprintf(summary, sizeof(summary), "Node.js project");
    }

    out->summary = hu_strndup(alloc, summary, strlen(summary));
    out->summary_len = strlen(out->summary);

    if (version)
        alloc->free(alloc->ctx, version, strlen(version) + 1);
    alloc->free(alloc->ctx, content, 4097);
    return HU_OK;
}

/* Detect Rust project: look for Cargo.toml */
static hu_error_t workspace_detect_rust(hu_allocator_t *alloc, const char *workspace_dir,
                                        hu_workspace_context_t *out) {
    char path[512];
    snprintf(path, sizeof(path), "%s/Cargo.toml", workspace_dir);

#if HU_IS_TEST
    if (access(path, F_OK) != 0)
        return HU_ERR_NOT_FOUND;
#else
    FILE *f = fopen(path, "r");
    if (!f)
        return HU_ERR_NOT_FOUND;
    fclose(f);
#endif

    char *content = workspace_read_file(alloc, path, 4096);
    if (!content)
        return HU_ERR_IO;

    out->project_type = hu_strndup(alloc, "rust", 4);
    out->project_type_len = 4;

    /* Extract name and version from TOML */
    char *name = workspace_extract_toml_field(alloc, content, strlen(content), "name");
    char *version = workspace_extract_toml_field(alloc, content, strlen(content), "version");

    if (name) {
        out->project_name = name;
        out->project_name_len = strlen(name);
    }

    /* Build summary */
    char summary[256] = {0};
    if (name && version) {
        snprintf(summary, sizeof(summary), "Rust project '%s' v%s", name, version);
    } else if (name) {
        snprintf(summary, sizeof(summary), "Rust project '%s'", name);
    } else {
        snprintf(summary, sizeof(summary), "Rust project");
    }

    out->summary = hu_strndup(alloc, summary, strlen(summary));
    out->summary_len = strlen(out->summary);

    if (version)
        alloc->free(alloc->ctx, version, strlen(version) + 1);
    alloc->free(alloc->ctx, content, 4097);
    return HU_OK;
}

/* Detect Go project: look for go.mod */
static hu_error_t workspace_detect_go(hu_allocator_t *alloc, const char *workspace_dir,
                                      hu_workspace_context_t *out) {
    char path[512];
    snprintf(path, sizeof(path), "%s/go.mod", workspace_dir);

#if HU_IS_TEST
    if (access(path, F_OK) != 0)
        return HU_ERR_NOT_FOUND;
#else
    FILE *f = fopen(path, "r");
    if (!f)
        return HU_ERR_NOT_FOUND;
    fclose(f);
#endif

    char *line = workspace_read_file_line(alloc, path);
    if (!line) {
        out->project_type = hu_strndup(alloc, "go", 2);
        out->project_type_len = 2;
        out->summary = hu_strndup(alloc, "Go project", 10);
        out->summary_len = 10;
        return HU_OK;
    }

    out->project_type = hu_strndup(alloc, "go", 2);
    out->project_type_len = 2;

    /* Extract module name from "module X" */
    const char *start = strstr(line, "module ");
    if (start) {
        start += 7; /* skip "module " */
        char summary[256];
        snprintf(summary, sizeof(summary), "Go project '%s'", start);
        out->summary = hu_strndup(alloc, summary, strlen(summary));
        out->summary_len = strlen(out->summary);
        out->project_name = hu_strndup(alloc, start, strlen(start));
        out->project_name_len = strlen(start);
    } else {
        out->summary = hu_strndup(alloc, "Go project", 10);
        out->summary_len = 10;
    }

    alloc->free(alloc->ctx, line, strlen(line) + 1);
    return HU_OK;
}

/* Detect Python project: look for pyproject.toml or setup.py */
static hu_error_t workspace_detect_python(hu_allocator_t *alloc, const char *workspace_dir,
                                          hu_workspace_context_t *out) {
    char path[512];
    snprintf(path, sizeof(path), "%s/pyproject.toml", workspace_dir);

#if HU_IS_TEST
    int has_pyproject = (access(path, F_OK) == 0);
#else
    FILE *f = fopen(path, "r");
    int has_pyproject = (f != NULL);
    if (f)
        fclose(f);
#endif

    if (!has_pyproject) {
        snprintf(path, sizeof(path), "%s/setup.py", workspace_dir);
#if HU_IS_TEST
        if (access(path, F_OK) != 0)
            return HU_ERR_NOT_FOUND;
#else
        f = fopen(path, "r");
        if (!f)
            return HU_ERR_NOT_FOUND;
        fclose(f);
#endif
    }

    out->project_type = hu_strndup(alloc, "python", 6);
    out->project_type_len = 6;

    char *content = workspace_read_file(alloc, path, 4096);
    char *name = NULL;
    if (content) {
        name = workspace_extract_toml_field(alloc, content, strlen(content), "name");
        alloc->free(alloc->ctx, content, 4097);
    }

    char summary[256];
    if (name) {
        snprintf(summary, sizeof(summary), "Python project '%s'", name);
        out->project_name = name;
        out->project_name_len = strlen(name);
    } else {
        snprintf(summary, sizeof(summary), "Python project");
    }

    out->summary = hu_strndup(alloc, summary, strlen(summary));
    out->summary_len = strlen(out->summary);

    return HU_OK;
}

/* Detect C/C++ project: look for CMakeLists.txt */
static hu_error_t workspace_detect_c(hu_allocator_t *alloc, const char *workspace_dir,
                                     hu_workspace_context_t *out) {
    char path[512];
    snprintf(path, sizeof(path), "%s/CMakeLists.txt", workspace_dir);

#if HU_IS_TEST
    if (access(path, F_OK) != 0)
        return HU_ERR_NOT_FOUND;
#else
    FILE *f = fopen(path, "r");
    if (!f)
        return HU_ERR_NOT_FOUND;
    fclose(f);
#endif

    out->project_type = hu_strndup(alloc, "c", 1);
    out->project_type_len = 1;

    char *content = workspace_read_file(alloc, path, 2048);
    char *name = NULL;
    if (content) {
        /* Look for project(name ...) directive */
        const char *p = strstr(content, "project(");
        if (p) {
            p += 8; /* skip "project(" */
            const char *end = strchr(p, ' ');
            if (end) {
                size_t len = end - p;
                name = hu_strndup(alloc, p, len);
            }
        }
        alloc->free(alloc->ctx, content, 2049);
    }

    char summary[256];
    if (name) {
        snprintf(summary, sizeof(summary), "C/C++ project '%s'", name);
        out->project_name = name;
        out->project_name_len = strlen(name);
    } else {
        snprintf(summary, sizeof(summary), "C/C++ project");
    }

    out->summary = hu_strndup(alloc, summary, strlen(summary));
    out->summary_len = strlen(out->summary);

    return HU_OK;
}

/* Detect Make project: look for Makefile */
static hu_error_t workspace_detect_make(hu_allocator_t *alloc, const char *workspace_dir,
                                        hu_workspace_context_t *out) {
    char path[512];
    snprintf(path, sizeof(path), "%s/Makefile", workspace_dir);

#if HU_IS_TEST
    if (access(path, F_OK) != 0)
        return HU_ERR_NOT_FOUND;
#else
    FILE *f = fopen(path, "r");
    if (!f)
        return HU_ERR_NOT_FOUND;
    fclose(f);
#endif

    out->project_type = hu_strndup(alloc, "make", 4);
    out->project_type_len = 4;
    out->summary = hu_strndup(alloc, "Make project", 12);
    out->summary_len = 12;

    return HU_OK;
}

hu_error_t hu_workspace_context_detect(hu_allocator_t *alloc, const char *workspace_dir,
                                       hu_workspace_context_t *out) {
    if (!alloc || !workspace_dir || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    /* Try detectors in order of popularity/priority */
    hu_error_t err;

    err = workspace_detect_nodejs(alloc, workspace_dir, out);
    if (err == HU_OK)
        return HU_OK;

    err = workspace_detect_python(alloc, workspace_dir, out);
    if (err == HU_OK)
        return HU_OK;

    err = workspace_detect_rust(alloc, workspace_dir, out);
    if (err == HU_OK)
        return HU_OK;

    err = workspace_detect_go(alloc, workspace_dir, out);
    if (err == HU_OK)
        return HU_OK;

    err = workspace_detect_c(alloc, workspace_dir, out);
    if (err == HU_OK)
        return HU_OK;

    err = workspace_detect_make(alloc, workspace_dir, out);
    if (err == HU_OK)
        return HU_OK;

    /* Default to unknown */
    out->project_type = hu_strndup(alloc, "unknown", 7);
    out->project_type_len = 7;
    out->summary = hu_strndup(alloc, "Unknown project", 15);
    out->summary_len = 15;

    return HU_OK;
}

void hu_workspace_context_free(hu_allocator_t *alloc, hu_workspace_context_t *ctx) {
    if (!alloc || !ctx)
        return;

    if (ctx->project_type)
        alloc->free(alloc->ctx, ctx->project_type, ctx->project_type_len + 1);
    if (ctx->project_name)
        alloc->free(alloc->ctx, ctx->project_name, ctx->project_name_len + 1);
    if (ctx->summary)
        alloc->free(alloc->ctx, ctx->summary, ctx->summary_len + 1);

    memset(ctx, 0, sizeof(*ctx));
}
