/*
 * Skill registry — fetch, search, install, uninstall, update from remote registry.
 * Under HU_IS_TEST, all network and filesystem operations return mock/OK.
 */
#include "human/skill_registry.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void entry_free(hu_allocator_t *a, hu_skill_registry_entry_t *e) {
    if (!a || !e)
        return;
    if (e->name)
        a->free(a->ctx, e->name, strlen(e->name) + 1);
    if (e->description)
        a->free(a->ctx, e->description, strlen(e->description) + 1);
    if (e->version)
        a->free(a->ctx, e->version, strlen(e->version) + 1);
    if (e->author)
        a->free(a->ctx, e->author, strlen(e->author) + 1);
    if (e->url)
        a->free(a->ctx, e->url, strlen(e->url) + 1);
    if (e->tags)
        a->free(a->ctx, e->tags, strlen(e->tags) + 1);
    memset(e, 0, sizeof(*e));
}

#ifdef HU_IS_TEST
/* Mock implementations — no network, no filesystem */
hu_error_t hu_skill_registry_search(hu_allocator_t *alloc, const char *query,
                                    hu_skill_registry_entry_t **out_entries, size_t *out_count) {
    (void)query;
    if (!alloc || !out_entries || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_entries = NULL;
    *out_count = 0;

    hu_skill_registry_entry_t *arr = (hu_skill_registry_entry_t *)alloc->alloc(
        alloc->ctx, 2 * sizeof(hu_skill_registry_entry_t));
    if (!arr)
        return HU_ERR_OUT_OF_MEMORY;
    memset(arr, 0, 2 * sizeof(hu_skill_registry_entry_t));

    arr[0].name = hu_strdup(alloc, "code-review");
    arr[0].description = hu_strdup(alloc, "Automated code review");
    arr[0].version = hu_strdup(alloc, "1.0.0");
    arr[0].author = hu_strdup(alloc, "human");
    arr[0].url =
        hu_strdup(alloc, "https://github.com/human/skill-registry/tree/main/skills/code-review");
    arr[0].tags = hu_strdup(alloc, "development,review");
    arr[1].name = hu_strdup(alloc, "email-digest");
    arr[1].description = hu_strdup(alloc, "Daily email digest");
    arr[1].version = hu_strdup(alloc, "1.0.0");
    arr[1].author = hu_strdup(alloc, "human");
    arr[1].url =
        hu_strdup(alloc, "https://github.com/human/skill-registry/tree/main/skills/email-digest");
    arr[1].tags = hu_strdup(alloc, "email,productivity");

    *out_entries = arr;
    *out_count = 2;
    return HU_OK;
}

void hu_skill_registry_entries_free(hu_allocator_t *alloc, hu_skill_registry_entry_t *entries,
                                    size_t count) {
    if (!alloc || !entries)
        return;
    for (size_t i = 0; i < count; i++)
        entry_free(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(hu_skill_registry_entry_t));
}

/* install/uninstall/update/publish: local filesystem only.
   Under HU_IS_TEST, validate input then return HU_OK with no side effects. */

hu_error_t hu_skill_registry_install(hu_allocator_t *alloc, const char *source_path) {
    if (!alloc || !source_path || !source_path[0])
        return HU_ERR_INVALID_ARGUMENT;
    return HU_OK;
}

hu_error_t hu_skill_registry_uninstall(const char *name) {
    if (!name || !name[0])
        return HU_ERR_INVALID_ARGUMENT;
    return HU_OK;
}

hu_error_t hu_skill_registry_update(hu_allocator_t *alloc, const char *source_path) {
    if (!alloc || !source_path || !source_path[0])
        return HU_ERR_INVALID_ARGUMENT;
    return HU_OK;
}

hu_error_t hu_skill_registry_publish(hu_allocator_t *alloc, const char *skill_dir) {
    if (!alloc || !skill_dir)
        return HU_ERR_INVALID_ARGUMENT;
    return HU_OK;
}

size_t hu_skill_registry_get_installed_dir(char *out, size_t out_len) {
    if (!out || out_len == 0)
        return 0;
    const char *home = getenv("HOME");
    if (!home) {
        out[0] = '\0';
        return 0;
    }
    int n = snprintf(out, out_len, "%s/.human/skills", home);
    return (n > 0 && (size_t)n < out_len) ? (size_t)n : 0;
}

#else /* !HU_IS_TEST */

static bool matches_query(const char *q, const char *name, const char *desc, const char *tags) {
    if (!q || !q[0])
        return true;
    if (!name && !desc && !tags)
        return false;
    const char *fields[] = {name, desc, tags};
    for (int i = 0; i < 3; i++) {
        const char *v = fields[i];
        if (!v)
            continue;
        size_t ql = strlen(q);
        size_t vl = strlen(v);
        for (size_t j = 0; j + ql <= vl; j++) {
            if (strncasecmp(v + j, q, ql) == 0)
                return true;
        }
    }
    return false;
}

hu_error_t hu_skill_registry_search(hu_allocator_t *alloc, const char *query,
                                    hu_skill_registry_entry_t **out_entries, size_t *out_count) {
    if (!alloc || !out_entries || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_entries = NULL;
    *out_count = 0;

    hu_http_response_t resp = {0};
    hu_error_t err = hu_http_get(alloc, HU_SKILL_REGISTRY_URL, NULL, &resp);
    if (err != HU_OK || !resp.body || resp.body_len == 0) {
        hu_http_response_free(alloc, &resp);
        return err != HU_OK ? err : HU_ERR_PROVIDER_RESPONSE;
    }

    hu_json_value_t *root = NULL;
    err = hu_json_parse(alloc, resp.body, resp.body_len, &root);
    hu_http_response_free(alloc, &resp);
    if (err != HU_OK || !root || root->type != HU_JSON_ARRAY) {
        if (root)
            hu_json_free(alloc, root);
        return HU_ERR_PARSE;
    }

    size_t cap = 16;
    hu_skill_registry_entry_t *arr = (hu_skill_registry_entry_t *)alloc->alloc(
        alloc->ctx, cap * sizeof(hu_skill_registry_entry_t));
    if (!arr) {
        hu_json_free(alloc, root);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memset(arr, 0, cap * sizeof(hu_skill_registry_entry_t));

    size_t count = 0;
    for (size_t i = 0; i < root->data.array.len && count < 256; i++) {
        hu_json_value_t *obj = root->data.array.items[i];
        if (!obj || obj->type != HU_JSON_OBJECT)
            continue;

        const char *name = hu_json_get_string(obj, "name");
        const char *desc = hu_json_get_string(obj, "description");
        const char *tags_str = NULL;
        hu_json_value_t *tags_val = hu_json_object_get(obj, "tags");
        if (tags_val) {
            if (tags_val->type == HU_JSON_STRING && tags_val->data.string.ptr)
                tags_str = tags_val->data.string.ptr;
            else if (tags_val->type == HU_JSON_ARRAY && tags_val->data.array.len > 0) {
                char tags_buf[256];
                size_t pos = 0;
                for (size_t j = 0; j < tags_val->data.array.len && pos < sizeof(tags_buf) - 2;
                     j++) {
                    hu_json_value_t *t = tags_val->data.array.items[j];
                    if (t && t->type == HU_JSON_STRING && t->data.string.ptr) {
                        if (j > 0) {
                            tags_buf[pos++] = ',';
                            tags_buf[pos++] = ' ';
                        }
                        size_t len = t->data.string.len;
                        if (pos + len >= sizeof(tags_buf))
                            len = sizeof(tags_buf) - pos - 1;
                        memcpy(tags_buf + pos, t->data.string.ptr, len);
                        pos += len;
                    }
                }
                tags_buf[pos] = '\0';
                if (pos > 0)
                    tags_str = tags_buf; /* only for match, not stored */
            }
        }
        if (!name)
            continue;
        if (query && query[0] && !matches_query(query, name, desc, tags_str))
            continue;

        if (count >= cap) {
            size_t new_cap = cap * 2;
            hu_skill_registry_entry_t *n = (hu_skill_registry_entry_t *)alloc->realloc(
                alloc->ctx, arr, cap * sizeof(hu_skill_registry_entry_t),
                new_cap * sizeof(hu_skill_registry_entry_t));
            if (!n) {
                for (size_t k = 0; k < count; k++)
                    entry_free(alloc, &arr[k]);
                alloc->free(alloc->ctx, arr, cap * sizeof(hu_skill_registry_entry_t));
                hu_json_free(alloc, root);
                return HU_ERR_OUT_OF_MEMORY;
            }
            arr = n;
            cap = new_cap;
            memset(arr + count, 0, (cap - count) * sizeof(hu_skill_registry_entry_t));
        }

        hu_skill_registry_entry_t *e = &arr[count];
        e->name = name ? hu_strdup(alloc, name) : NULL;
        e->description = desc ? hu_strdup(alloc, desc) : hu_strdup(alloc, "");
        {
            const char *v = hu_json_get_string(obj, "version");
            e->version = hu_strdup(alloc, v && v[0] ? v : "1.0.0");
        }
        {
            const char *a = hu_json_get_string(obj, "author");
            e->author = hu_strdup(alloc, a && a[0] ? a : "");
        }
        {
            const char *u = hu_json_get_string(obj, "url");
            e->url = hu_strdup(alloc, u && u[0] ? u : "");
        }
        e->tags = tags_str ? hu_strdup(alloc, tags_str) : NULL;
        if (!e->name) {
            entry_free(alloc, e);
            continue;
        }
        count++;
    }

    hu_json_free(alloc, root);
    *out_entries = arr;
    *out_count = count;
    return HU_OK;
}

void hu_skill_registry_entries_free(hu_allocator_t *alloc, hu_skill_registry_entry_t *entries,
                                    size_t count) {
    if (!alloc || !entries)
        return;
    for (size_t i = 0; i < count; i++)
        entry_free(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(hu_skill_registry_entry_t));
}

#if defined(HU_GATEWAY_POSIX) && (!defined(HU_IS_TEST) || !HU_IS_TEST)

/* Copy src_file to dst_file. Returns HU_OK on success. */
static hu_error_t copy_file(const char *src_file, const char *dst_file) {
    FILE *src = fopen(src_file, "rb");
    if (!src)
        return HU_ERR_IO;
    FILE *dst = fopen(dst_file, "wb");
    if (!dst) {
        fclose(src);
        return HU_ERR_IO;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            fclose(src);
            fclose(dst);
            unlink(dst_file);
            return HU_ERR_IO;
        }
    }
    fclose(src);
    fclose(dst);
    return HU_OK;
}

/* Find manifest path in dir: .skill.json, manifest.json, or first *.skill.json. */
static bool find_manifest_in_dir(const char *dir, char *out_path, size_t out_len) {
    char path[512];
    int n = snprintf(path, sizeof(path), "%s/.skill.json", dir);
    if (n > 0 && (size_t)n < sizeof(path)) {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(out_path, out_len, "%s", path);
            return true;
        }
    }
    n = snprintf(path, sizeof(path), "%s/manifest.json", dir);
    if (n > 0 && (size_t)n < sizeof(path)) {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(out_path, out_len, "%s", path);
            return true;
        }
    }
#ifndef _WIN32
    {
        DIR *d = opendir(dir);
        if (!d)
            return false;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t len = strlen(e->d_name);
            if (len < 12 || strcmp(e->d_name + len - 11, ".skill.json") != 0)
                continue;
            n = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
            closedir(d);
            if (n > 0 && (size_t)n < sizeof(path)) {
                struct stat st;
                if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                    snprintf(out_path, out_len, "%s", path);
                    return true;
                }
            }
            return false;
        }
        closedir(d);
    }
#endif
    return false;
}

/* Recursively remove directory contents and the directory. */
static hu_error_t remove_dir_recursive(const char *path) {
#ifndef _WIN32
    DIR *d = opendir(path);
    if (!d)
        return (errno == ENOENT) ? HU_OK : HU_ERR_IO;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' && (e->d_name[1] == '\0' ||
                                    (e->d_name[1] == '.' && e->d_name[2] == '\0')))
            continue;
        char sub[512];
        int n = snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
        if (n <= 0 || (size_t)n >= sizeof(sub)) {
            closedir(d);
            return HU_ERR_INVALID_ARGUMENT;
        }
        struct stat st;
        if (stat(sub, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            hu_error_t err = remove_dir_recursive(sub);
            if (err != HU_OK) {
                closedir(d);
                return err;
            }
        } else if (unlink(sub) != 0) {
            closedir(d);
            return HU_ERR_IO;
        }
    }
    closedir(d);
    if (rmdir(path) != 0)
        return HU_ERR_IO;
#endif
    return HU_OK;
}

#endif /* HU_GATEWAY_POSIX && !HU_IS_TEST */

hu_error_t hu_skill_registry_install(hu_allocator_t *alloc, const char *source_path) {
    if (!alloc || !source_path || !source_path[0])
        return HU_ERR_INVALID_ARGUMENT;

#if !defined(HU_IS_TEST) || !HU_IS_TEST
#ifdef HU_GATEWAY_POSIX
    char manifest_path[512];
    if (!find_manifest_in_dir(source_path, manifest_path, sizeof(manifest_path)))
        return HU_ERR_NOT_FOUND;

    char buf[8192];
    FILE *f = fopen(manifest_path, "rb");
    if (!f)
        return HU_ERR_IO;
    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (nr == 0 || nr >= sizeof(buf) - 1)
        return HU_ERR_IO;
    buf[nr] = '\0';

    hu_json_value_t *parsed = NULL;
    hu_error_t err = hu_json_parse(alloc, buf, nr, &parsed);
    if (err != HU_OK || !parsed || parsed->type != HU_JSON_OBJECT) {
        if (parsed)
            hu_json_free(alloc, parsed);
        return err != HU_OK ? err : HU_ERR_PARSE;
    }
    const char *name = hu_json_get_string(parsed, "name");
    if (!name || !name[0]) {
        hu_json_free(alloc, parsed);
        return HU_ERR_PARSE;
    }
    hu_json_free(alloc, parsed);

    const char *home = getenv("HOME");
    if (!home || !home[0])
        return HU_ERR_INVALID_ARGUMENT;

    char base_dir[512];
    int n = snprintf(base_dir, sizeof(base_dir), "%s/.human/skills", home);
    if (n <= 0 || (size_t)n >= sizeof(base_dir))
        return HU_ERR_INVALID_ARGUMENT;
    if (mkdir(base_dir, 0755) != 0 && errno != EEXIST)
        return HU_ERR_IO;

    char dest_dir[512];
    n = snprintf(dest_dir, sizeof(dest_dir), "%s/.human/skills/%.256s", home, name);
    if (n <= 0 || (size_t)n >= sizeof(dest_dir))
        return HU_ERR_INVALID_ARGUMENT;
    if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST)
        return HU_ERR_IO;

    /* Copy manifest as manifest.json (skillforge discovers subdir/manifest.json) */
    char dest_manifest[512];
    n = snprintf(dest_manifest, sizeof(dest_manifest), "%s/manifest.json", dest_dir);
    if (n <= 0 || (size_t)n >= sizeof(dest_manifest))
        return HU_ERR_INVALID_ARGUMENT;
    err = copy_file(manifest_path, dest_manifest);
    if (err != HU_OK)
        return err;

    /* Copy SKILL.md if present */
    char src_md[512];
    n = snprintf(src_md, sizeof(src_md), "%s/SKILL.md", source_path);
    if (n > 0 && (size_t)n < sizeof(src_md)) {
        struct stat st;
        if (stat(src_md, &st) == 0 && S_ISREG(st.st_mode)) {
            char dst_md[512];
            n = snprintf(dst_md, sizeof(dst_md), "%s/SKILL.md", dest_dir);
            if (n > 0 && (size_t)n < sizeof(dst_md))
                copy_file(src_md, dst_md);
        }
    }

    /* Copy any other non-hidden files (e.g. supporting assets) */
#ifndef _WIN32
    {
        DIR *d = opendir(source_path);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.')
                    continue;
                if (strcmp(e->d_name, "SKILL.md") == 0)
                    continue;
                size_t len = strlen(e->d_name);
                if (len >= 12 && strcmp(e->d_name + len - 11, ".skill.json") == 0)
                    continue;
                if (strcmp(e->d_name, "manifest.json") == 0)
                    continue;
                char src_file[512];
                n = snprintf(src_file, sizeof(src_file), "%s/%s", source_path, e->d_name);
                if (n <= 0 || (size_t)n >= sizeof(src_file))
                    continue;
                struct stat st;
                if (stat(src_file, &st) != 0 || !S_ISREG(st.st_mode))
                    continue;
                char dst_file[512];
                n = snprintf(dst_file, sizeof(dst_file), "%s/%s", dest_dir, e->d_name);
                if (n > 0 && (size_t)n < sizeof(dst_file))
                    copy_file(src_file, dst_file);
            }
            closedir(d);
        }
    }
#endif
    return HU_OK;
#else
    (void)alloc;
    (void)source_path;
    return HU_ERR_NOT_SUPPORTED;
#endif
#else
    (void)alloc;
    (void)source_path;
    return HU_OK;
#endif
}

hu_error_t hu_skill_registry_uninstall(const char *name) {
    if (!name || !name[0])
        return HU_ERR_INVALID_ARGUMENT;

#if !defined(HU_IS_TEST) || !HU_IS_TEST
#ifdef HU_GATEWAY_POSIX
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return HU_ERR_INVALID_ARGUMENT;

    char dir_path[512];
    int n = snprintf(dir_path, sizeof(dir_path), "%s/.human/skills/%.256s", home, name);
    if (n <= 0 || (size_t)n >= sizeof(dir_path))
        return HU_ERR_INVALID_ARGUMENT;

    hu_error_t err = remove_dir_recursive(dir_path);
    if (err != HU_OK)
        return err;

    /* Also remove flat .skill.json for backward compatibility */
    char flat_path[512];
    n = snprintf(flat_path, sizeof(flat_path), "%s/.human/skills/%.256s.skill.json", home, name);
    if (n > 0 && (size_t)n < sizeof(flat_path))
        unlink(flat_path);

    return HU_OK;
#else
    (void)name;
    return HU_ERR_NOT_SUPPORTED;
#endif
#else
    (void)name;
    return HU_OK;
#endif
}

hu_error_t hu_skill_registry_update(hu_allocator_t *alloc, const char *source_path) {
    if (!alloc || !source_path || !source_path[0])
        return HU_ERR_INVALID_ARGUMENT;

#if !defined(HU_IS_TEST) || !HU_IS_TEST
    char manifest_path[512];
#ifdef HU_GATEWAY_POSIX
    if (!find_manifest_in_dir(source_path, manifest_path, sizeof(manifest_path)))
        return HU_ERR_NOT_FOUND;

    char buf[8192];
    FILE *f = fopen(manifest_path, "rb");
    if (!f)
        return HU_ERR_IO;
    size_t nr = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (nr == 0 || nr >= sizeof(buf) - 1)
        return HU_ERR_IO;
    buf[nr] = '\0';

    hu_json_value_t *parsed = NULL;
    hu_error_t err = hu_json_parse(alloc, buf, nr, &parsed);
    if (err != HU_OK || !parsed || parsed->type != HU_JSON_OBJECT) {
        if (parsed)
            hu_json_free(alloc, parsed);
        return err != HU_OK ? err : HU_ERR_PARSE;
    }
    const char *name = hu_json_get_string(parsed, "name");
    if (!name || !name[0]) {
        hu_json_free(alloc, parsed);
        return HU_ERR_PARSE;
    }
    hu_json_free(alloc, parsed);

    hu_skill_registry_uninstall(name);
    return hu_skill_registry_install(alloc, source_path);
#else
    (void)alloc;
    (void)source_path;
    return HU_ERR_NOT_SUPPORTED;
#endif
#else
    (void)alloc;
    (void)source_path;
    return HU_OK;
#endif
}

hu_error_t hu_skill_registry_publish(hu_allocator_t *alloc, const char *skill_dir) {
    if (!alloc || !skill_dir)
        return HU_ERR_INVALID_ARGUMENT;

#if !defined(HU_IS_TEST) || !HU_IS_TEST
#ifdef HU_GATEWAY_POSIX
    char manifest_path[512];
    if (!find_manifest_in_dir(skill_dir, manifest_path, sizeof(manifest_path)))
        return HU_ERR_NOT_FOUND;

    char buf[8192];
    FILE *f = fopen(manifest_path, "rb");
    if (!f)
        return HU_ERR_IO;
    size_t read_len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (read_len == 0 || read_len >= sizeof(buf))
        return HU_ERR_IO;
    buf[read_len] = '\0';

    hu_json_value_t *parsed = NULL;
    hu_error_t err = hu_json_parse(alloc, buf, read_len, &parsed);
    if (err != HU_OK || !parsed || parsed->type != HU_JSON_OBJECT) {
        if (parsed)
            hu_json_free(alloc, parsed);
        return err != HU_OK ? err : HU_ERR_PARSE;
    }

    const char *name = hu_json_get_string(parsed, "name");
    const char *desc = hu_json_get_string(parsed, "description");
    if (!name || !name[0] || !desc) {
        hu_json_free(alloc, parsed);
        return HU_ERR_PARSE;
    }
    hu_json_free(alloc, parsed);

    /* Write manifest to .published marker (local-only publish) */
    char published_path[512];
    int n = snprintf(published_path, sizeof(published_path), "%s/.published", skill_dir);
    if (n <= 0 || (size_t)n >= sizeof(published_path))
        return HU_ERR_INVALID_ARGUMENT;
    FILE *pf = fopen(published_path, "wb");
    if (!pf)
        return HU_ERR_IO;
    size_t written = fwrite(buf, 1, read_len, pf);
    fclose(pf);
    if (written != read_len)
        return HU_ERR_IO;

    return HU_OK;
#else
    (void)alloc;
    (void)skill_dir;
    return HU_ERR_NOT_SUPPORTED;
#endif
#else
    (void)alloc;
    (void)skill_dir;
    return HU_OK;
#endif
}

size_t hu_skill_registry_get_installed_dir(char *out, size_t out_len) {
    if (!out || out_len == 0)
        return 0;
    const char *home = getenv("HOME");
    if (!home) {
        out[0] = '\0';
        return 0;
    }
    int n = snprintf(out, out_len, "%s/.human/skills", home);
    return (n > 0 && (size_t)n < out_len) ? (size_t)n : 0;
}

#endif /* !HU_IS_TEST */
