#ifdef HU_GATEWAY_POSIX

#include "cp_internal.h"
#include "human/agent.h"
#include "human/eval/turing_score.h"
#include "human/memory.h"
#include <stdio.h>
#include <string.h>

#ifdef HU_ENABLE_SQLITE

hu_error_t cp_turing_scores(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                            const hu_control_protocol_t *proto, const hu_json_value_t *root,
                            char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!app || !app->agent || !app->agent->memory)
        return HU_ERR_NOT_SUPPORTED;
    sqlite3 *db = hu_sqlite_memory_get_db(app->agent->memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;
    if (hu_turing_init_tables(db) != HU_OK)
        return HU_ERR_IO;

    hu_turing_score_t scores[50];
    int64_t timestamps[50];
    char contact_ids[50][HU_TURING_CONTACT_ID_MAX];
    size_t count = 0;
    if (hu_turing_get_trend(alloc, db, NULL, 0, 50, scores, timestamps, contact_ids, &count) !=
        HU_OK)
        return HU_ERR_IO;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *arr = hu_json_array_new(alloc);
    if (!arr) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        hu_json_value_t *entry = hu_json_object_new(alloc);
        if (!entry)
            break;
        hu_json_object_set(alloc, entry, "contact_id",
                           hu_json_string_new(alloc, contact_ids[i], strlen(contact_ids[i])));
        hu_json_object_set(alloc, entry, "timestamp",
                           hu_json_number_new(alloc, (double)timestamps[i]));
        hu_json_object_set(alloc, entry, "overall",
                           hu_json_number_new(alloc, (double)scores[i].overall));
        hu_json_object_set(alloc, entry, "verdict",
                           hu_json_string_new(alloc, hu_turing_verdict_name(scores[i].verdict),
                                              strlen(hu_turing_verdict_name(scores[i].verdict))));
        hu_json_value_t *dims = hu_json_object_new(alloc);
        if (dims) {
            for (int d = 0; d < HU_TURING_DIM_COUNT; d++) {
                const char *dn = hu_turing_dimension_name((hu_turing_dimension_t)d);
                hu_json_object_set(alloc, dims, dn,
                                   hu_json_number_new(alloc, (double)scores[i].dimensions[d]));
            }
            hu_json_object_set(alloc, entry, "dimensions", dims);
        }
        hu_json_array_push(alloc, arr, entry);
    }

    hu_json_object_set(alloc, obj, "scores", arr);
    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
}

hu_error_t cp_turing_trend(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                           const hu_control_protocol_t *proto, const hu_json_value_t *root,
                           char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!app || !app->agent || !app->agent->memory)
        return HU_ERR_NOT_SUPPORTED;
    sqlite3 *db = hu_sqlite_memory_get_db(app->agent->memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;
    if (hu_turing_init_tables(db) != HU_OK)
        return HU_ERR_IO;

    hu_turing_score_t scores[50];
    int64_t timestamps[50];
    char contact_ids[50][HU_TURING_CONTACT_ID_MAX];
    size_t count = 0;
    if (hu_turing_get_trend(alloc, db, NULL, 0, 50, scores, timestamps, contact_ids, &count) !=
        HU_OK)
        return HU_ERR_IO;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *arr = hu_json_array_new(alloc);
    if (!arr) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        hu_json_value_t *entry = hu_json_object_new(alloc);
        if (!entry)
            break;
        hu_json_object_set(alloc, entry, "contact_id",
                           hu_json_string_new(alloc, contact_ids[i], strlen(contact_ids[i])));
        hu_json_object_set(alloc, entry, "timestamp",
                           hu_json_number_new(alloc, (double)timestamps[i]));
        hu_json_object_set(alloc, entry, "overall",
                           hu_json_number_new(alloc, (double)scores[i].overall));
        hu_json_array_push(alloc, arr, entry);
    }

    hu_json_object_set(alloc, obj, "trend", arr);
    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
}

hu_error_t cp_turing_dimensions(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!app || !app->agent || !app->agent->memory)
        return HU_ERR_NOT_SUPPORTED;
    sqlite3 *db = hu_sqlite_memory_get_db(app->agent->memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;
    if (hu_turing_init_tables(db) != HU_OK)
        return HU_ERR_IO;

    int dim_avgs[HU_TURING_DIM_COUNT];
    hu_turing_get_weakest_dimensions(db, dim_avgs);

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *dims = hu_json_object_new(alloc);
    if (!dims) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }

    for (int d = 0; d < HU_TURING_DIM_COUNT; d++) {
        const char *dn = hu_turing_dimension_name((hu_turing_dimension_t)d);
        hu_json_object_set(alloc, dims, dn, hu_json_number_new(alloc, (double)dim_avgs[d]));
    }

    hu_json_object_set(alloc, obj, "dimensions", dims);
    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
}

hu_error_t cp_turing_contact(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                             const hu_control_protocol_t *proto, const hu_json_value_t *root,
                             char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    if (!app || !app->agent || !app->agent->memory)
        return HU_ERR_NOT_SUPPORTED;
    sqlite3 *db = hu_sqlite_memory_get_db(app->agent->memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;
    if (hu_turing_init_tables(db) != HU_OK)
        return HU_ERR_IO;

    const char *contact_id = NULL;
    size_t contact_id_len = 0;
    if (root) {
        hu_json_value_t *params = hu_json_object_get(root, "params");
        if (params) {
            contact_id = hu_json_get_string(params, "contact_id");
            if (contact_id)
                contact_id_len = strlen(contact_id);
        }
    }
    if (!contact_id || contact_id_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    int contact_dims[HU_TURING_DIM_COUNT];
    if (hu_turing_get_contact_dimensions(db, contact_id, contact_id_len, contact_dims) != HU_OK)
        return HU_ERR_IO;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *dims = hu_json_object_new(alloc);
    if (!dims) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }
    for (int d = 0; d < HU_TURING_DIM_COUNT; d++) {
        const char *dn = hu_turing_dimension_name((hu_turing_dimension_t)d);
        hu_json_object_set(alloc, dims, dn, hu_json_number_new(alloc, (double)contact_dims[d]));
    }
    hu_json_object_set(alloc, obj, "contact_id",
                       hu_json_string_new(alloc, contact_id, contact_id_len));
    hu_json_object_set(alloc, obj, "dimensions", dims);

    size_t hint_len = 0;
    char *hint = hu_turing_build_contact_hint(alloc, contact_dims, &hint_len);
    if (hint) {
        hu_json_object_set(alloc, obj, "hint", hu_json_string_new(alloc, hint, hint_len));
        alloc->free(alloc->ctx, hint, hint_len + 1);
    }

    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
}

hu_error_t cp_turing_trajectory(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!app || !app->agent || !app->agent->memory)
        return HU_ERR_NOT_SUPPORTED;
    sqlite3 *db = hu_sqlite_memory_get_db(app->agent->memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;
    if (hu_turing_init_tables(db) != HU_OK)
        return HU_ERR_IO;

    hu_turing_score_t scores[20];
    int64_t timestamps[20];
    char contact_ids[20][HU_TURING_CONTACT_ID_MAX];
    size_t count = 0;
    if (hu_turing_get_trend(alloc, db, NULL, 0, 20, scores, timestamps, contact_ids, &count) !=
        HU_OK)
        return HU_ERR_IO;
    if (count < 2)
        return HU_ERR_NOT_SUPPORTED;

    hu_turing_trajectory_t traj;
    if (hu_turing_score_trajectory(scores, count, &traj) != HU_OK)
        return HU_ERR_IO;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, obj, "directional_alignment",
                       hu_json_number_new(alloc, (double)traj.directional_alignment));
    hu_json_object_set(alloc, obj, "cumulative_impact",
                       hu_json_number_new(alloc, (double)traj.cumulative_impact));
    hu_json_object_set(alloc, obj, "stability", hu_json_number_new(alloc, (double)traj.stability));
    hu_json_object_set(alloc, obj, "overall", hu_json_number_new(alloc, (double)traj.overall));

    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
}

hu_error_t cp_turing_ab_tests(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                              const hu_control_protocol_t *proto, const hu_json_value_t *root,
                              char **out, size_t *out_len) {
    (void)conn;
    (void)proto;
    (void)root;
    if (!app || !app->agent || !app->agent->memory)
        return HU_ERR_NOT_SUPPORTED;
    sqlite3 *db = hu_sqlite_memory_get_db(app->agent->memory);
    if (!db)
        return HU_ERR_NOT_SUPPORTED;
    if (hu_ab_test_init_table(db) != HU_OK)
        return HU_ERR_IO;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_value_t *arr = hu_json_array_new(alloc);
    if (!arr) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }

    static const char *tests[] = {"disfluency_freq", "backchannel_prob", "double_text_prob"};
    for (size_t i = 0; i < 3; i++) {
        hu_ab_test_t test;
        if (hu_ab_test_get_results(db, tests[i], &test) == HU_OK) {
            hu_json_value_t *entry = hu_json_object_new(alloc);
            if (!entry)
                break;
            hu_json_object_set(alloc, entry, "name",
                               hu_json_string_new(alloc, test.name, strlen(test.name)));
            hu_json_object_set(alloc, entry, "variant_a",
                               hu_json_number_new(alloc, (double)test.variant_a));
            hu_json_object_set(alloc, entry, "variant_b",
                               hu_json_number_new(alloc, (double)test.variant_b));
            float avg_a =
                test.score_count_a > 0 ? (float)test.score_sum_a / (float)test.score_count_a : 0.0f;
            float avg_b =
                test.score_count_b > 0 ? (float)test.score_sum_b / (float)test.score_count_b : 0.0f;
            hu_json_object_set(alloc, entry, "avg_a", hu_json_number_new(alloc, (double)avg_a));
            hu_json_object_set(alloc, entry, "avg_b", hu_json_number_new(alloc, (double)avg_b));
            hu_json_object_set(alloc, entry, "count_a",
                               hu_json_number_new(alloc, (double)test.score_count_a));
            hu_json_object_set(alloc, entry, "count_b",
                               hu_json_number_new(alloc, (double)test.score_count_b));
            hu_json_object_set(alloc, entry, "active", hu_json_bool_new(alloc, test.active));
            hu_json_array_push(alloc, arr, entry);
        }
    }

    hu_json_object_set(alloc, obj, "tests", arr);
    hu_error_t err = hu_json_stringify(alloc, obj, out, out_len);
    hu_json_free(alloc, obj);
    return err;
}

#else /* !HU_ENABLE_SQLITE */

hu_error_t cp_turing_scores(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                            const hu_control_protocol_t *proto, const hu_json_value_t *root,
                            char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_turing_trend(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                           const hu_control_protocol_t *proto, const hu_json_value_t *root,
                           char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_turing_dimensions(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_turing_contact(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                             const hu_control_protocol_t *proto, const hu_json_value_t *root,
                             char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_turing_trajectory(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t cp_turing_ab_tests(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                              const hu_control_protocol_t *proto, const hu_json_value_t *root,
                              char **out, size_t *out_len) {
    (void)alloc;
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;
    (void)out;
    (void)out_len;
    return HU_ERR_NOT_SUPPORTED;
}

#endif /* HU_ENABLE_SQLITE */

#endif /* HU_GATEWAY_POSIX */
