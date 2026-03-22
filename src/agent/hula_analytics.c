#include "human/agent/hula_analytics.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

hu_error_t hu_hula_analytics_summarize(hu_allocator_t *alloc, const char *trace_dir, char **out_json,
                                       size_t *out_len) {
    if (!alloc || !trace_dir || !trace_dir[0] || !out_json || !out_len)
        return HU_ERR_INVALID_ARGUMENT;
    *out_json = NULL;
    *out_len = 0;

#if !(defined(__unix__) || defined(__APPLE__))
    (void)trace_dir;
    return HU_ERR_NOT_SUPPORTED;
#else
    size_t file_count = 0;
    size_t success_count = 0;
    size_t fail_count = 0;
    size_t total_steps = 0;
    time_t newest_ts = 0;

    DIR *d = opendir(trace_dir);
    if (!d) {
        hu_json_value_t *obj = hu_json_object_new(alloc);
        if (!obj)
            return HU_ERR_OUT_OF_MEMORY;
        hu_json_object_set(alloc, obj, "file_count", hu_json_number_new(alloc, 0));
        hu_json_object_set(alloc, obj, "success_count", hu_json_number_new(alloc, 0));
        hu_json_object_set(alloc, obj, "fail_count", hu_json_number_new(alloc, 0));
        hu_json_object_set(alloc, obj, "total_trace_steps", hu_json_number_new(alloc, 0));
        hu_json_object_set(alloc, obj, "newest_ts", hu_json_number_new(alloc, 0));
        hu_error_t er = hu_json_stringify(alloc, obj, out_json, out_len);
        hu_json_free(alloc, obj);
        return er;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.')
            continue;
        size_t nl = strlen(de->d_name);
        if (nl < 6 || strcmp(de->d_name + nl - 5, ".json") != 0)
            continue;

        char path[768];
        int pn = snprintf(path, sizeof(path), "%s/%s", trace_dir, de->d_name);
        if (pn <= 0 || (size_t)pn >= sizeof(path))
            continue;

        FILE *f = fopen(path, "rb");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > 4 * 1024 * 1024) {
            fclose(f);
            continue;
        }
        char *buf = (char *)alloc->alloc(alloc->ctx, (size_t)sz + 1);
        if (!buf) {
            fclose(f);
            closedir(d);
            return HU_ERR_OUT_OF_MEMORY;
        }
        size_t rd = fread(buf, 1, (size_t)sz, f);
        fclose(f);
        buf[rd] = '\0';

        hu_json_value_t *root = NULL;
        if (hu_json_parse(alloc, buf, rd, &root) != HU_OK || !root) {
            alloc->free(alloc->ctx, buf, (size_t)sz + 1);
            continue;
        }
        alloc->free(alloc->ctx, buf, (size_t)sz + 1);

        file_count++;
        bool ok_field = false;
        hu_json_value_t *sv = hu_json_object_get(root, "success");
        if (sv && sv->type == HU_JSON_BOOL && sv->data.boolean)
            ok_field = true;
        if (ok_field)
            success_count++;
        else
            fail_count++;

        hu_json_value_t *tsv = hu_json_object_get(root, "ts");
        if (tsv && tsv->type == HU_JSON_NUMBER) {
            double tv = tsv->data.number;
            time_t t = (time_t)tv;
            if (tv == tv && tv >= 0 && (double)t == tv && t > newest_ts)
                newest_ts = t;
        }

        hu_json_value_t *tr = hu_json_object_get(root, "trace");
        if (tr && tr->type == HU_JSON_ARRAY)
            total_steps += tr->data.array.len;

        hu_json_free(alloc, root);
    }
    closedir(d);

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;
    hu_json_object_set(alloc, obj, "file_count", hu_json_number_new(alloc, (double)file_count));
    hu_json_object_set(alloc, obj, "success_count", hu_json_number_new(alloc, (double)success_count));
    hu_json_object_set(alloc, obj, "fail_count", hu_json_number_new(alloc, (double)fail_count));
    hu_json_object_set(alloc, obj, "total_trace_steps",
                       hu_json_number_new(alloc, (double)total_steps));
    hu_json_object_set(alloc, obj, "newest_ts", hu_json_number_new(alloc, (double)newest_ts));

    hu_error_t err = hu_json_stringify(alloc, obj, out_json, out_len);
    hu_json_free(alloc, obj);
    return err;
#endif
}
