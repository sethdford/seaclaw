#include "cp_internal.h"
#include "human/core/string.h"
#include "human/tools/canvas.h"
#include "human/tools/canvas_store.h"

#ifdef HU_GATEWAY_POSIX

hu_error_t cp_canvas_list(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                          const hu_control_protocol_t *proto, const hu_json_value_t *root,
                          char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)app;
    static const char mock[] = "{\"canvases\":[]}";
    *out = hu_strndup(alloc, mock, sizeof(mock) - 1);
    *out_len = sizeof(mock) - 1;
    return HU_OK;
#else
    hu_canvas_store_t *store = app ? app->canvas_store : NULL;
    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *arr = hu_json_array_new(alloc);
    if (!arr) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }

    if (store) {
        size_t count = hu_canvas_store_count(store);
        for (size_t i = 0; i < count; i++) {
            hu_canvas_info_t info;
            if (!hu_canvas_store_get(store, i, &info))
                continue;
            hu_json_value_t *item = hu_json_object_new(alloc);
            if (!item)
                continue;
            cp_json_set_str(alloc, item, "canvas_id", info.canvas_id);
            cp_json_set_str(alloc, item, "title", info.title);
            cp_json_set_str(alloc, item, "format", info.format);
            cp_json_set_str(alloc, item, "content", info.content);
            if (info.language)
                cp_json_set_str(alloc, item, "language", info.language);
            if (info.imports)
                cp_json_set_str(alloc, item, "imports", info.imports);
            hu_json_object_set(alloc, item, "version_seq",
                               hu_json_number_new(alloc, (double)info.version_seq));
            hu_json_object_set(alloc, item, "version_count",
                               hu_json_number_new(alloc, (double)info.version_count));
            hu_json_array_push(alloc, arr, item);
        }
    }

    hu_json_object_set(alloc, obj, "canvases", arr);
    return cp_respond_json(alloc, obj, out, out_len);
#endif
}

hu_error_t cp_canvas_get(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                         const hu_control_protocol_t *proto, const hu_json_value_t *root,
                         char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)app;
    (void)root;
    static const char mock[] = "{\"canvas\":null}";
    *out = hu_strndup(alloc, mock, sizeof(mock) - 1);
    *out_len = sizeof(mock) - 1;
    return HU_OK;
#else
    {
    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    if (!params) {
        static const char err[] = "{\"error\":\"params required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }
    const char *canvas_id = hu_json_get_string(params, "canvas_id");
    if (!canvas_id) {
        static const char err[] = "{\"error\":\"canvas_id required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_canvas_store_t *store = app ? app->canvas_store : NULL;
    if (!store) {
        static const char err[] = "{\"canvas\":null}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_canvas_info_t info;
    if (!hu_canvas_store_find(store, canvas_id, &info)) {
        static const char err[] = "{\"error\":\"not found\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *canvas = hu_json_object_new(alloc);
    if (!canvas) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }
    cp_json_set_str(alloc, canvas, "canvas_id", info.canvas_id);
    cp_json_set_str(alloc, canvas, "title", info.title);
    cp_json_set_str(alloc, canvas, "format", info.format);
    cp_json_set_str(alloc, canvas, "content", info.content);
    if (info.language)
        cp_json_set_str(alloc, canvas, "language", info.language);
    if (info.imports)
        cp_json_set_str(alloc, canvas, "imports", info.imports);
    hu_json_object_set(alloc, canvas, "version_seq",
                       hu_json_number_new(alloc, (double)info.version_seq));
    hu_json_object_set(alloc, canvas, "version_count",
                       hu_json_number_new(alloc, (double)info.version_count));
    hu_json_object_set(alloc, canvas, "user_edit_pending",
                       hu_json_bool_new(alloc, info.user_edit_pending));
    hu_json_object_set(alloc, obj, "canvas", canvas);
    return cp_respond_json(alloc, obj, out, out_len);
    }
#endif
}

hu_error_t cp_canvas_edit(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                          const hu_control_protocol_t *proto, const hu_json_value_t *root,
                          char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)app;
    (void)root;
    static const char mock[] = "{\"ok\":true}";
    *out = hu_strndup(alloc, mock, sizeof(mock) - 1);
    *out_len = sizeof(mock) - 1;
    return HU_OK;
#else
    {
    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    if (!params) {
        static const char err[] = "{\"error\":\"params required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }
    const char *canvas_id = hu_json_get_string(params, "canvas_id");
    const char *content = hu_json_get_string(params, "content");
    if (!canvas_id || !content) {
        static const char err[] = "{\"error\":\"canvas_id and content required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_canvas_store_t *store = app ? app->canvas_store : NULL;
    if (!store) {
        static const char err[] = "{\"error\":\"no canvas store\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_error_t err = hu_canvas_store_edit(store, canvas_id, content);
    if (err != HU_OK) {
        static const char msg[] = "{\"error\":\"canvas not found\"}";
        *out = hu_strndup(alloc, msg, sizeof(msg) - 1);
        *out_len = sizeof(msg) - 1;
        return HU_OK;
    }

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, obj, "ok", hu_json_bool_new(alloc, true));
    cp_json_set_str(alloc, obj, "canvas_id", canvas_id);
    return cp_respond_json(alloc, obj, out, out_len);
    }
#endif
}

hu_error_t cp_canvas_undo(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                          const hu_control_protocol_t *proto, const hu_json_value_t *root,
                          char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)app;
    (void)root;
    static const char mock[] = "{\"ok\":true,\"canvas_id\":\"cv_0\",\"version_seq\":0}";
    *out = hu_strndup(alloc, mock, sizeof(mock) - 1);
    *out_len = sizeof(mock) - 1;
    return HU_OK;
#else
    {
    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    if (!params) {
        static const char err[] = "{\"error\":\"params required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }
    const char *canvas_id = hu_json_get_string(params, "canvas_id");
    if (!canvas_id) {
        static const char err[] = "{\"error\":\"canvas_id required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_canvas_store_t *store = app ? app->canvas_store : NULL;
    if (!store) {
        static const char err[] = "{\"error\":\"no canvas store\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_canvas_info_t info;
    hu_error_t err = hu_canvas_store_undo(store, canvas_id, &info);
    if (err != HU_OK) {
        static const char msg[] = "{\"error\":\"nothing to undo\"}";
        *out = hu_strndup(alloc, msg, sizeof(msg) - 1);
        *out_len = sizeof(msg) - 1;
        return HU_OK;
    }

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, obj, "ok", hu_json_bool_new(alloc, true));
    cp_json_set_str(alloc, obj, "canvas_id", canvas_id);
    hu_json_object_set(alloc, obj, "version_seq",
                       hu_json_number_new(alloc, (double)info.version_seq));
    cp_json_set_str(alloc, obj, "content", info.content);
    cp_json_set_str(alloc, obj, "format", info.format);
    return cp_respond_json(alloc, obj, out, out_len);
    }
#endif
}

hu_error_t cp_canvas_redo(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                          const hu_control_protocol_t *proto, const hu_json_value_t *root,
                          char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    if (!alloc || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    (void)app;
    (void)root;
    static const char mock[] = "{\"ok\":true,\"canvas_id\":\"cv_0\",\"version_seq\":0}";
    *out = hu_strndup(alloc, mock, sizeof(mock) - 1);
    *out_len = sizeof(mock) - 1;
    return HU_OK;
#else
    {
    hu_json_value_t *params = root ? hu_json_object_get(root, "params") : NULL;
    if (!params) {
        static const char err[] = "{\"error\":\"params required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }
    const char *canvas_id = hu_json_get_string(params, "canvas_id");
    if (!canvas_id) {
        static const char err[] = "{\"error\":\"canvas_id required\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_canvas_store_t *store = app ? app->canvas_store : NULL;
    if (!store) {
        static const char err[] = "{\"error\":\"no canvas store\"}";
        *out = hu_strndup(alloc, err, sizeof(err) - 1);
        *out_len = sizeof(err) - 1;
        return HU_OK;
    }

    hu_canvas_info_t info;
    hu_error_t err = hu_canvas_store_redo(store, canvas_id, &info);
    if (err != HU_OK) {
        static const char msg[] = "{\"error\":\"nothing to redo\"}";
        *out = hu_strndup(alloc, msg, sizeof(msg) - 1);
        *out_len = sizeof(msg) - 1;
        return HU_OK;
    }

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, obj, "ok", hu_json_bool_new(alloc, true));
    cp_json_set_str(alloc, obj, "canvas_id", canvas_id);
    hu_json_object_set(alloc, obj, "version_seq",
                       hu_json_number_new(alloc, (double)info.version_seq));
    cp_json_set_str(alloc, obj, "content", info.content);
    cp_json_set_str(alloc, obj, "format", info.format);
    return cp_respond_json(alloc, obj, out, out_len);
    }
#endif
}

#endif /* HU_GATEWAY_POSIX */
