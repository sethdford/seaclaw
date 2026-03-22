#ifdef HU_GATEWAY_POSIX

#include "cp_internal.h"
#include "human/agent/hula_analytics.h"
#include "human/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void default_hula_trace_dir(char *buf, size_t cap) {
    const char *h = getenv("HOME");
    if (!h || !h[0]) {
        buf[0] = '\0';
        return;
    }
    (void)snprintf(buf, cap, "%s/.human/hula_traces", h);
}

static bool safe_trace_basename(const char *name) {
    if (!name || !name[0])
        return false;
    size_t n = strlen(name);
    if (n < 6 || strcmp(name + n - 5, ".json") != 0)
        return false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c <= 32)
            return false;
        if (name[i] == '/' || name[i] == '\\')
            return false;
        if (name[i] == '.' && i + 1 < n && name[i + 1] == '.')
            return false;
    }
    return true;
}

hu_error_t cp_hula_traces_list(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                               const hu_control_protocol_t *proto, const hu_json_value_t *root,
                               char **out, size_t *out_len) {
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
#if !(defined(__unix__) || defined(__APPLE__))
    (void)alloc;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
#else
    char dir[512];
    default_hula_trace_dir(dir, sizeof(dir));
    if (!dir[0])
        return HU_ERR_NOT_FOUND;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *arr = hu_json_array_new(alloc);
    if (!arr) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }

    DIR *d = opendir(dir);
    if (!d) {
        hu_json_object_set(alloc, obj, "traces", arr);
        hu_json_object_set(alloc, obj, "directory", hu_json_string_new(alloc, dir, strlen(dir)));
        hu_error_t er = hu_json_stringify(alloc, obj, out, out_len);
        hu_json_free(alloc, obj);
        return er;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.')
            continue;
        if (!safe_trace_basename(de->d_name))
            continue;
        char path[768];
        int pn = snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        if (pn <= 0 || (size_t)pn >= sizeof(path))
            continue;
        struct stat st;
        if (stat(path, &st) != 0)
            continue;
        hu_json_value_t *row = hu_json_object_new(alloc);
        if (!row)
            continue;
        cp_json_set_str(alloc, row, "id", de->d_name);
        hu_json_object_set(alloc, row, "size", hu_json_number_new(alloc, (double)st.st_size));
        hu_json_object_set(alloc, row, "mtime", hu_json_number_new(alloc, (double)st.st_mtime));
        hu_json_array_push(alloc, arr, row);
    }
    closedir(d);

    hu_json_object_set(alloc, obj, "traces", arr);
    hu_json_object_set(alloc, obj, "directory", hu_json_string_new(alloc, dir, strlen(dir)));
    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
#endif
}

hu_error_t cp_hula_traces_get(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                              const hu_control_protocol_t *proto, const hu_json_value_t *root,
                              char **out, size_t *out_len) {
    (void)app;
    (void)conn;
    (void)proto;
#if !(defined(__unix__) || defined(__APPLE__))
    (void)alloc;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
#else
    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    const char *id = params ? hu_json_get_string(params, "id") : NULL;
    if (!id || !safe_trace_basename(id))
        return HU_ERR_INVALID_ARGUMENT;

    char dir[512];
    default_hula_trace_dir(dir, sizeof(dir));
    if (!dir[0])
        return HU_ERR_NOT_FOUND;

    char path[768];
    int pn = snprintf(path, sizeof(path), "%s/%s", dir, id);
    if (pn <= 0 || (size_t)pn >= sizeof(path))
        return HU_ERR_INVALID_ARGUMENT;

    FILE *f = fopen(path, "rb");
    if (!f)
        return HU_ERR_NOT_FOUND;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 8 * 1024 * 1024) {
        fclose(f);
        return HU_ERR_INVALID_ARGUMENT;
    }
    char *buf = (char *)alloc->alloc(alloc->ctx, (size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return HU_ERR_OUT_OF_MEMORY;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj) {
        alloc->free(alloc->ctx, buf, (size_t)sz + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    cp_json_set_str(alloc, obj, "id", id);
    hu_json_value_t *parsed = NULL;
    if (hu_json_parse(alloc, buf, rd, &parsed) == HU_OK && parsed)
        hu_json_object_set(alloc, obj, "record", parsed);
    else
        hu_json_object_set(alloc, obj, "raw", hu_json_string_new(alloc, buf, rd));
    alloc->free(alloc->ctx, buf, (size_t)sz + 1);

    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
#endif
}

hu_error_t cp_hula_traces_delete(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                 const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                 char **out, size_t *out_len) {
    (void)app;
    (void)conn;
    (void)proto;
#if !(defined(__unix__) || defined(__APPLE__))
    (void)alloc;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
#else
    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    const char *id = params ? hu_json_get_string(params, "id") : NULL;
    if (!id || !safe_trace_basename(id))
        return HU_ERR_INVALID_ARGUMENT;

    char dir[512];
    default_hula_trace_dir(dir, sizeof(dir));
    if (!dir[0])
        return HU_ERR_NOT_FOUND;
    char path[768];
    int pn = snprintf(path, sizeof(path), "%s/%s", dir, id);
    if (pn <= 0 || (size_t)pn >= sizeof(path))
        return HU_ERR_INVALID_ARGUMENT;
    if (unlink(path) != 0)
        return HU_ERR_NOT_FOUND;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, obj, "deleted", hu_json_bool_new(alloc, true));
    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
#endif
}

hu_error_t cp_hula_traces_analytics(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                    const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                    char **out, size_t *out_len) {
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
#if !(defined(__unix__) || defined(__APPLE__))
    (void)alloc;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
#else
    char dir[512];
    default_hula_trace_dir(dir, sizeof(dir));
    if (!dir[0])
        return HU_ERR_NOT_FOUND;
    char *payload = NULL;
    size_t plen = 0;
    hu_error_t err = hu_hula_analytics_summarize(alloc, dir, &payload, &plen);
    if (err != HU_OK)
        return err;
    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj) {
        hu_str_free(alloc, payload);
        return HU_ERR_OUT_OF_MEMORY;
    }
    hu_json_value_t *inner = NULL;
    if (payload && hu_json_parse(alloc, payload, plen, &inner) == HU_OK && inner)
        hu_json_object_set(alloc, obj, "summary", inner);
    else
        cp_json_set_str(alloc, obj, "summary_raw", payload ? payload : "");
    hu_str_free(alloc, payload);
    hu_error_t ser = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return ser;
#endif
}

#endif /* HU_GATEWAY_POSIX */
