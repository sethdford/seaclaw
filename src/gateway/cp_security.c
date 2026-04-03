/* Gateway handler for security.cot.summary — returns recent CoT audit verdicts */
#include "cp_internal.h"
#include "human/core/string.h"
#include <stdio.h>
#include <time.h>

hu_error_t cp_security_cot_summary(hu_allocator_t *alloc, hu_app_context_t *app, hu_ws_conn_t *conn,
                                   const hu_control_protocol_t *proto, const hu_json_value_t *root,
                                   char **out, size_t *out_len) {
    (void)app;
    (void)conn;
    (void)proto;
    (void)root;

    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;

    hu_json_value_t *arr = hu_json_array_new(alloc);
    if (!arr) {
        hu_json_free(alloc, obj);
        return HU_ERR_OUT_OF_MEMORY;
    }

    /* In production, this would query the audit logger for recent CoT verdicts.
     * For now, return structure the UI can consume. */
    hu_json_object_set(alloc, obj, "entries", arr);
    hu_json_object_set(alloc, obj, "total_audited", hu_json_number_new(alloc, 0));
    hu_json_object_set(alloc, obj, "total_blocked", hu_json_number_new(alloc, 0));
    hu_json_object_set(alloc, obj, "total_suspicious", hu_json_number_new(alloc, 0));

    return cp_respond_json(alloc, obj, out, out_len);
}
