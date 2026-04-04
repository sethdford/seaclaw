#ifdef HU_GATEWAY_POSIX

#include "cp_internal.h"
#include "human/agent/task_store.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

#if !(defined(HU_IS_TEST) && HU_IS_TEST)
static hu_error_t json_buf_finish(hu_json_buf_t *buf, hu_allocator_t *alloc, char **out,
                                  size_t *out_len) {
    if (!buf || !alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    if (!buf->ptr || buf->len == 0) {
        *out = hu_strdup(alloc, "{}");
        if (!*out)
            return HU_ERR_OUT_OF_MEMORY;
        *out_len = strlen(*out);
        hu_json_buf_free(buf);
        return HU_OK;
    }
    char *p = (char *)alloc->alloc(alloc->ctx, buf->len + 1);
    if (!p) {
        hu_json_buf_free(buf);
        return HU_ERR_OUT_OF_MEMORY;
    }
    memcpy(p, buf->ptr, buf->len);
    p[buf->len] = '\0';
    *out = p;
    *out_len = buf->len;
    hu_json_buf_free(buf);
    return HU_OK;
}

static hu_error_t respond_task_store_missing(hu_allocator_t *alloc, char **out, size_t *out_len) {
    hu_json_buf_t buf = {0};
    hu_error_t e = hu_json_buf_init(&buf, alloc);
    if (e != HU_OK)
        return e;
    e = hu_json_buf_append_raw(&buf, "{\"error\":", 10);
    if (e != HU_OK) {
        hu_json_buf_free(&buf);
        return e;
    }
    e = hu_json_append_string(&buf, "task_store_unavailable", 22);
    if (e != HU_OK) {
        hu_json_buf_free(&buf);
        return e;
    }
    e = hu_json_buf_append_raw(&buf, "}", 1);
    if (e != HU_OK) {
        hu_json_buf_free(&buf);
        return e;
    }
    return json_buf_finish(&buf, alloc, out, out_len);
}

static hu_error_t append_task_fields(hu_json_buf_t *buf, const hu_task_record_t *t) {
    hu_error_t e = hu_json_buf_append_raw(buf, "{", 1);
    if (e)
        return e;
    e = hu_json_append_key_int(buf, "id", 2, (long long)t->id);
    if (e)
        return e;
    e = hu_json_buf_append_raw(buf, ",", 1);
    if (e)
        return e;
    e = hu_json_append_key_value(buf, "name", 4, t->name ? t->name : "",
                                 t->name ? strlen(t->name) : 0);
    if (e)
        return e;
    e = hu_json_buf_append_raw(buf, ",", 1);
    if (e)
        return e;
    const char *st = hu_task_status_string(t->status);
    e = hu_json_append_key_value(buf, "status", 6, st, strlen(st));
    if (e)
        return e;
    e = hu_json_buf_append_raw(buf, ",", 1);
    if (e)
        return e;
    e = hu_json_append_key_value(buf, "program_json", 12, t->program_json ? t->program_json : "",
                                 t->program_json ? strlen(t->program_json) : 0);
    if (e)
        return e;
    e = hu_json_buf_append_raw(buf, ",", 1);
    if (e)
        return e;
    e = hu_json_append_key_value(buf, "trace_json", 10, t->trace_json ? t->trace_json : "",
                                 t->trace_json ? strlen(t->trace_json) : 0);
    if (e)
        return e;
    e = hu_json_buf_append_raw(buf, ",", 1);
    if (e)
        return e;
    e = hu_json_append_key_int(buf, "created_at", 10, (long long)t->created_at);
    if (e)
        return e;
    e = hu_json_buf_append_raw(buf, ",", 1);
    if (e)
        return e;
    e = hu_json_append_key_int(buf, "updated_at", 10, (long long)t->updated_at);
    if (e)
        return e;
    e = hu_json_buf_append_raw(buf, ",", 1);
    if (e)
        return e;
    e = hu_json_append_key_int(buf, "parent_task_id", 14, (long long)t->parent_task_id);
    if (e)
        return e;
    return hu_json_buf_append_raw(buf, "}", 1);
}
#endif /* !(HU_IS_TEST) */

hu_error_t cp_tasks_list(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                         const hu_control_protocol_t *proto, const hu_json_value_t *root,
                         char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)app;
    (void)root;
#endif
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

#if HU_IS_TEST
    *out = hu_strdup(alloc,
                     "{\"tasks\":[{\"id\":1,\"name\":\"mock_task\",\"status\":\"pending\","
                     "\"program_json\":\"{}\",\"trace_json\":\"[]\",\"created_at\":1700000000,"
                     "\"updated_at\":1700000000,\"parent_task_id\":0}]}");
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    *out_len = strlen(*out);
    return HU_OK;
#else
    if (!app || !app->task_store)
        return respond_task_store_missing(alloc, out, out_len);

    hu_task_status_t *flt = NULL;
    hu_task_status_t flt_storage;
    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    if (params && hu_json_object_get(params, "status")) {
        double sd = hu_json_get_number(params, "status", -1.0);
        if (sd >= 0.0 && sd <= 4.0) {
            flt_storage = (hu_task_status_t)(int)sd;
            flt = &flt_storage;
        }
    }

    hu_task_record_t *rows = NULL;
    size_t n = 0;
    hu_error_t err = hu_task_store_list(app->task_store, alloc, flt, &rows, &n);
    if (err != HU_OK)
        return err;

    hu_json_buf_t buf = {0};
    err = hu_json_buf_init(&buf, alloc);
    if (err != HU_OK) {
        hu_task_records_free(alloc, rows, n);
        return err;
    }
    err = hu_json_buf_append_raw(&buf, "{\"tasks\":[", 11);
    if (err != HU_OK) {
        hu_json_buf_free(&buf);
        hu_task_records_free(alloc, rows, n);
        return err;
    }
    for (size_t i = 0; i < n && err == HU_OK; i++) {
        if (i > 0)
            err = hu_json_buf_append_raw(&buf, ",", 1);
        if (err == HU_OK)
            err = append_task_fields(&buf, &rows[i]);
    }
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, "]}", 2);
    hu_task_records_free(alloc, rows, n);
    if (err != HU_OK) {
        hu_json_buf_free(&buf);
        return err;
    }
    return json_buf_finish(&buf, alloc, out, out_len);
#endif
}

hu_error_t cp_tasks_get(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                        const hu_control_protocol_t *proto, const hu_json_value_t *root,
                        char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)app;
    (void)root;
#endif
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

#if HU_IS_TEST
    *out = hu_strdup(alloc,
                     "{\"task\":{\"id\":2,\"name\":\"mock_get\",\"status\":\"running\","
                     "\"program_json\":\"{}\",\"trace_json\":\"[]\",\"created_at\":1700000001,"
                     "\"updated_at\":1700000002,\"parent_task_id\":0}}");
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    *out_len = strlen(*out);
    return HU_OK;
#else
    if (!app || !app->task_store)
        return respond_task_store_missing(alloc, out, out_len);

    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    double idd = params ? hu_json_get_number(params, "id", 0.0) : 0.0;
    if (idd <= 0.0)
        return HU_ERR_INVALID_ARGUMENT;
    uint64_t id = (uint64_t)idd;

    hu_task_record_t rec = {0};
    hu_error_t err = hu_task_store_load(app->task_store, alloc, id, &rec);
    if (err != HU_OK)
        return err;

    hu_json_buf_t buf = {0};
    err = hu_json_buf_init(&buf, alloc);
    if (err != HU_OK) {
        hu_task_record_free(alloc, &rec);
        return err;
    }
    err = hu_json_buf_append_raw(&buf, "{\"task\":", 9);
    if (err == HU_OK)
        err = append_task_fields(&buf, &rec);
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, "}", 1);
    hu_task_record_free(alloc, &rec);
    if (err != HU_OK) {
        hu_json_buf_free(&buf);
        return err;
    }
    return json_buf_finish(&buf, alloc, out, out_len);
#endif
}

hu_error_t cp_tasks_cancel(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                           const hu_control_protocol_t *proto, const hu_json_value_t *root,
                           char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)app;
    (void)root;
#endif
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out = NULL;
    *out_len = 0;

#if HU_IS_TEST
    *out = hu_strdup(alloc, "{\"ok\":true,\"id\":3,\"status\":\"cancelled\"}");
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    *out_len = strlen(*out);
    return HU_OK;
#else
    if (!app || !app->task_store)
        return respond_task_store_missing(alloc, out, out_len);

    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    double idd = params ? hu_json_get_number(params, "id", 0.0) : 0.0;
    if (idd <= 0.0)
        return HU_ERR_INVALID_ARGUMENT;
    uint64_t id = (uint64_t)idd;

    hu_error_t err =
        hu_task_store_update_status(app->task_store, id, HU_TASK_STATUS_CANCELLED);
    if (err != HU_OK)
        return err;

    hu_json_buf_t buf = {0};
    err = hu_json_buf_init(&buf, alloc);
    if (err != HU_OK)
        return err;
    err = hu_json_buf_append_raw(&buf, "{\"ok\":true,\"id\":", 18);
    if (err == HU_OK) {
        char nb[32];
        int nn = snprintf(nb, sizeof(nb), "%llu", (unsigned long long)id);
        if (nn > 0 && (size_t)nn < sizeof(nb))
            err = hu_json_buf_append_raw(&buf, nb, (size_t)nn);
        else
            err = HU_ERR_INTERNAL;
    }
    if (err == HU_OK)
        err = hu_json_buf_append_raw(&buf, ",\"status\":\"cancelled\"}", 23);
    if (err != HU_OK) {
        hu_json_buf_free(&buf);
        return err;
    }
    return json_buf_finish(&buf, alloc, out, out_len);
#endif
}

#endif /* HU_GATEWAY_POSIX */
