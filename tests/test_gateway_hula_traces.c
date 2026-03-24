#include "cp_internal.h"
#include "human/core/json.h"
#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(HU_GATEWAY_POSIX) && (defined(__unix__) || defined(__APPLE__))
#include <unistd.h>
#endif

#if defined(HU_GATEWAY_POSIX) && (defined(__unix__) || defined(__APPLE__))

static bool trace_list_contains_id(hu_allocator_t *alloc, const char *json, size_t json_len,
                                   const char *want_id) {
    hu_json_value_t *root = NULL;
    if (hu_json_parse(alloc, json, json_len, &root) != HU_OK || !root)
        return false;
    hu_json_value_t *tr = hu_json_object_get(root, "traces");
    bool ok = false;
    if (tr && tr->type == HU_JSON_ARRAY) {
        for (size_t i = 0; i < tr->data.array.len && !ok; i++) {
            hu_json_value_t *row = tr->data.array.items[i];
            if (!row || row->type != HU_JSON_OBJECT)
                continue;
            const char *id = hu_json_get_string(row, "id");
            if (id && strcmp(id, want_id) == 0)
                ok = true;
        }
    }
    hu_json_free(alloc, root);
    return ok;
}

static void gateway_hula_traces_env_dir_list_get_analytics_delete(void) {
    hu_allocator_t alloc = hu_system_allocator();
    hu_control_protocol_t proto = {.alloc = &alloc, .ws = NULL, .event_seq = 0, .app_ctx = NULL};

    char tmpl[] = "/tmp/hgtXXXXXX";
    char *tmpdir = mkdtemp(tmpl);
    HU_ASSERT_NOT_NULL(tmpdir);

    char *old_env = NULL;
    const char *cur = getenv("HU_HULA_TRACE_DIR");
    if (cur && cur[0]) {
        old_env = strdup(cur);
        HU_ASSERT_NOT_NULL(old_env);
    }
    HU_ASSERT_EQ(setenv("HU_HULA_TRACE_DIR", tmpdir, 1), 0);

    char path[512];
    int pn = snprintf(path, sizeof(path), "%s/gw_hula_case.json", tmpdir);
    HU_ASSERT_TRUE(pn > 0 && (size_t)pn < sizeof(path));

    FILE *wf = fopen(path, "w");
    HU_ASSERT_NOT_NULL(wf);
    static const char doc[] =
        "{\"version\":1,\"success\":true,\"ts\":1700000000,\"program_name\":\"gwtest\","
        "\"trace\":[{\"id\":\"n1\",\"op\":\"call\",\"status\":\"done\"}]}";
    HU_ASSERT_EQ(fwrite(doc, 1, sizeof(doc) - 1, wf), sizeof(doc) - 1);
    HU_ASSERT_EQ(fclose(wf), 0);

    hu_json_value_t *req_list = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc, "{\"params\":{}}", strlen("{\"params\":{}}"), &req_list),
                 HU_OK);
    HU_ASSERT_NOT_NULL(req_list);

    char *out = NULL;
    size_t out_len = 0;
    HU_ASSERT_EQ(
        cp_hula_traces_list(&alloc, NULL, NULL, &proto, req_list, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    HU_ASSERT_TRUE(trace_list_contains_id(&alloc, out, out_len, "gw_hula_case.json"));
    alloc.free(alloc.ctx, out, out_len + 1);
    out = NULL;

    hu_json_value_t *req_get = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc,
                    "{\"params\":{\"id\":\"gw_hula_case.json\"}}",
                    strlen("{\"params\":{\"id\":\"gw_hula_case.json\"}}"), &req_get),
                 HU_OK);
    HU_ASSERT_NOT_NULL(req_get);
    HU_ASSERT_EQ(cp_hula_traces_get(&alloc, NULL, NULL, &proto, req_get, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    hu_json_value_t *parsed = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc, out, out_len, &parsed), HU_OK);
    HU_ASSERT_NOT_NULL(parsed);
    hu_json_value_t *rec = hu_json_object_get(parsed, "record");
    HU_ASSERT_NOT_NULL(rec);
    const char *pnm = hu_json_get_string(rec, "program_name");
    HU_ASSERT_NOT_NULL(pnm);
    HU_ASSERT_STR_EQ(pnm, "gwtest");
    hu_json_free(&alloc, parsed);
    alloc.free(alloc.ctx, out, out_len + 1);
    out = NULL;

    hu_json_value_t *req_an = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc, "{\"params\":{}}", strlen("{\"params\":{}}"), &req_an),
                 HU_OK);
    HU_ASSERT_NOT_NULL(req_an);
    HU_ASSERT_EQ(
        cp_hula_traces_analytics(&alloc, NULL, NULL, &proto, req_an, &out, &out_len), HU_OK);
    HU_ASSERT_NOT_NULL(out);
    hu_json_value_t *anroot = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc, out, out_len, &anroot), HU_OK);
    HU_ASSERT_NOT_NULL(anroot);
    hu_json_value_t *sum = hu_json_object_get(anroot, "summary");
    HU_ASSERT_NOT_NULL(sum);
    double fc = hu_json_get_number(sum, "file_count", -1);
    HU_ASSERT_TRUE(fc >= 1.0);
    hu_json_free(&alloc, anroot);
    alloc.free(alloc.ctx, out, out_len + 1);
    out = NULL;

    hu_json_value_t *req_del = NULL;
    HU_ASSERT_EQ(hu_json_parse(&alloc,
                    "{\"params\":{\"id\":\"gw_hula_case.json\"}}",
                    strlen("{\"params\":{\"id\":\"gw_hula_case.json\"}}"), &req_del),
                 HU_OK);
    HU_ASSERT_NOT_NULL(req_del);
    HU_ASSERT_EQ(cp_hula_traces_delete(&alloc, NULL, NULL, &proto, req_del, &out, &out_len),
                 HU_OK);
    HU_ASSERT_NOT_NULL(out);
    alloc.free(alloc.ctx, out, out_len + 1);

    hu_json_free(&alloc, req_list);
    hu_json_free(&alloc, req_get);
    hu_json_free(&alloc, req_an);
    hu_json_free(&alloc, req_del);

    if (old_env) {
        (void)setenv("HU_HULA_TRACE_DIR", old_env, 1);
        free(old_env);
    } else {
        (void)unsetenv("HU_HULA_TRACE_DIR");
    }
    (void)rmdir(tmpdir);
}

#endif /* HU_GATEWAY_POSIX && unix */

void run_gateway_hula_traces_tests(void) {
#if defined(HU_GATEWAY_POSIX) && (defined(__unix__) || defined(__APPLE__))
    HU_TEST_SUITE("gateway_hula_traces");
    HU_RUN_TEST(gateway_hula_traces_env_dir_list_get_analytics_delete);
#endif
}
