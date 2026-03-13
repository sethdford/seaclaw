#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/peripheral.h"
#include "human/tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HU_PERIPHERAL_CTRL_NAME "peripheral_ctrl"
#define HU_PERIPHERAL_CTRL_DESC "Control hardware peripherals: read/write GPIO pins, check status, flash firmware"
#define HU_PERIPHERAL_CTRL_PARAMS                                                                    \
    "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\","                         \
    "\"enum\":[\"read\",\"write\",\"status\",\"flash\",\"capabilities\"]},\"pin\":{\"type\":"        \
    "\"integer\"},\"value\":{\"type\":\"integer\"},\"firmware_path\":{\"type\":\"string\"}},"        \
    "\"required\":[\"action\"]}"

typedef struct hu_peripheral_ctrl_ctx {
    hu_peripheral_t *peripheral;
} hu_peripheral_ctrl_ctx_t;

#if !HU_IS_TEST
static const char *peripheral_err_to_msg(hu_peripheral_error_t err) {
    switch (err) {
    case HU_PERIPHERAL_ERR_NONE:
        return "ok";
    case HU_PERIPHERAL_ERR_NOT_CONNECTED:
        return "peripheral not connected";
    case HU_PERIPHERAL_ERR_IO:
        return "I/O error";
    case HU_PERIPHERAL_ERR_FLASH_FAILED:
        return "flash failed";
    case HU_PERIPHERAL_ERR_TIMEOUT:
        return "timeout";
    case HU_PERIPHERAL_ERR_INVALID_ADDRESS:
        return "invalid address";
    case HU_PERIPHERAL_ERR_PERMISSION_DENIED:
        return "permission denied";
    case HU_PERIPHERAL_ERR_DEVICE_NOT_FOUND:
        return "device not found";
    case HU_PERIPHERAL_ERR_UNSUPPORTED_OPERATION:
        return "unsupported operation";
    default:
        return "unknown peripheral error";
    }
}

static hu_error_t format_capabilities(hu_allocator_t *alloc,
                                      const hu_peripheral_capabilities_t *cap,
                                      char **out_str, size_t *out_len) {
    hu_json_value_t *obj = hu_json_object_new(alloc);
    if (!obj)
        return HU_ERR_OUT_OF_MEMORY;

    if (cap->board_name && cap->board_name_len > 0)
        hu_json_object_set(alloc, obj, "board_name",
                           hu_json_string_new(alloc, cap->board_name, cap->board_name_len));
    if (cap->board_type && cap->board_type_len > 0)
        hu_json_object_set(alloc, obj, "board_type",
                           hu_json_string_new(alloc, cap->board_type, cap->board_type_len));
    if (cap->gpio_pins && cap->gpio_pins_len > 0)
        hu_json_object_set(alloc, obj, "gpio_pins",
                           hu_json_string_new(alloc, cap->gpio_pins, cap->gpio_pins_len));
    hu_json_object_set(alloc, obj, "flash_size_kb",
                       hu_json_number_new(alloc, (double)cap->flash_size_kb));
    hu_json_object_set(alloc, obj, "has_serial", hu_json_bool_new(alloc, cap->has_serial));
    hu_json_object_set(alloc, obj, "has_gpio", hu_json_bool_new(alloc, cap->has_gpio));
    hu_json_object_set(alloc, obj, "has_flash", hu_json_bool_new(alloc, cap->has_flash));
    hu_json_object_set(alloc, obj, "has_adc", hu_json_bool_new(alloc, cap->has_adc));

    hu_error_t err = hu_json_stringify(alloc, obj, out_str, out_len);
    hu_json_free(alloc, obj);
    return err;
}
#endif /* !HU_IS_TEST */

static hu_error_t peripheral_ctrl_execute(void *ctx, hu_allocator_t *alloc,
                                         const hu_json_value_t *args, hu_tool_result_t *out) {
    hu_peripheral_ctrl_ctx_t *c = (hu_peripheral_ctrl_ctx_t *)ctx;
    if (!c || !c->peripheral || !args || !out) {
        *out = hu_tool_result_fail("invalid args", 12);
        return HU_ERR_INVALID_ARGUMENT;
    }

    const char *action = hu_json_get_string(args, "action");
    if (!action || strlen(action) == 0) {
        *out = hu_tool_result_fail("missing action", 14);
        return HU_OK;
    }

#if HU_IS_TEST
    (void)c;
    if (strcmp(action, "read") == 0) {
        char *msg = hu_strndup(alloc, "0", 1);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 12);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, 1);
        return HU_OK;
    }
    if (strcmp(action, "write") == 0) {
        *out = hu_tool_result_ok("ok", 2);
        return HU_OK;
    }
    if (strcmp(action, "status") == 0) {
        *out = hu_tool_result_ok("healthy", 7);
        return HU_OK;
    }
    if (strcmp(action, "capabilities") == 0) {
        char *msg = hu_strndup(alloc, "{\"board_name\":\"mock\",\"has_gpio\":true}", 36);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 12);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, 36);
        return HU_OK;
    }
    if (strcmp(action, "flash") == 0) {
        *out = hu_tool_result_ok("flashed", 7);
        return HU_OK;
    }
    *out = hu_tool_result_fail("unknown action", 14);
    return HU_OK;
#else
    hu_peripheral_t *p = c->peripheral;
    const hu_peripheral_vtable_t *vt = p->vtable;
    if (!vt) {
        *out = hu_tool_result_fail("peripheral has no vtable", 24);
        return HU_OK;
    }

    if (strcmp(action, "read") == 0) {
        uint32_t pin = (uint32_t)hu_json_get_number(args, "pin", 0);
        uint8_t value = 0;
        hu_peripheral_error_t perr = vt->read(p->ctx, pin, &value);
        if (perr != HU_PERIPHERAL_ERR_NONE) {
            const char *msg = peripheral_err_to_msg(perr);
            *out = hu_tool_result_fail(msg, strlen(msg));
            return HU_OK;
        }
        char buf[16];
        int n = snprintf(buf, sizeof(buf), "%u", (unsigned)value);
        char *str = hu_strndup(alloc, buf, (size_t)n);
        if (!str) {
            *out = hu_tool_result_fail("out of memory", 12);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(str, (size_t)n);
        return HU_OK;
    }

    if (strcmp(action, "write") == 0) {
        uint32_t pin = (uint32_t)hu_json_get_number(args, "pin", 0);
        double val = hu_json_get_number(args, "value", 0);
        uint8_t data = (uint8_t)(val < 0 ? 0 : (val > 255 ? 255 : (unsigned)val));
        hu_peripheral_error_t perr = vt->write(p->ctx, pin, data);
        if (perr != HU_PERIPHERAL_ERR_NONE) {
            const char *msg = peripheral_err_to_msg(perr);
            *out = hu_tool_result_fail(msg, strlen(msg));
            return HU_OK;
        }
        *out = hu_tool_result_ok("ok", 2);
        return HU_OK;
    }

    if (strcmp(action, "status") == 0) {
        bool healthy = vt->health_check(p->ctx);
        *out = hu_tool_result_ok(healthy ? "healthy" : "unhealthy", healthy ? 7 : 10);
        return HU_OK;
    }

    if (strcmp(action, "capabilities") == 0) {
        hu_peripheral_capabilities_t cap = vt->capabilities(p->ctx);
        char *json_str = NULL;
        size_t json_len = 0;
        hu_error_t err = format_capabilities(alloc, &cap, &json_str, &json_len);
        if (err != HU_OK) {
            *out = hu_tool_result_fail("out of memory", 12);
            return err;
        }
        *out = hu_tool_result_ok_owned(json_str, json_len);
        return HU_OK;
    }

    if (strcmp(action, "flash") == 0) {
        const char *path = hu_json_get_string(args, "firmware_path");
        if (!path || strlen(path) == 0) {
            *out = hu_tool_result_fail("missing firmware_path", 20);
            return HU_OK;
        }
        size_t path_len = strlen(path);
        hu_peripheral_error_t perr = vt->flash(p->ctx, path, path_len);
        if (perr != HU_PERIPHERAL_ERR_NONE) {
            const char *msg = peripheral_err_to_msg(perr);
            *out = hu_tool_result_fail(msg, strlen(msg));
            return HU_OK;
        }
        *out = hu_tool_result_ok("flashed", 7);
        return HU_OK;
    }

    *out = hu_tool_result_fail("unknown action", 14);
    return HU_OK;
#endif
}

static const char *peripheral_ctrl_name(void *ctx) {
    (void)ctx;
    return HU_PERIPHERAL_CTRL_NAME;
}

static const char *peripheral_ctrl_description(void *ctx) {
    (void)ctx;
    return HU_PERIPHERAL_CTRL_DESC;
}

static const char *peripheral_ctrl_parameters_json(void *ctx) {
    (void)ctx;
    return HU_PERIPHERAL_CTRL_PARAMS;
}

static void peripheral_ctrl_deinit(void *ctx, hu_allocator_t *alloc) {
    if (ctx && alloc)
        alloc->free(alloc->ctx, ctx, sizeof(hu_peripheral_ctrl_ctx_t));
}

static const hu_tool_vtable_t peripheral_ctrl_vtable = {
    .execute = peripheral_ctrl_execute,
    .name = peripheral_ctrl_name,
    .description = peripheral_ctrl_description,
    .parameters_json = peripheral_ctrl_parameters_json,
    .deinit = peripheral_ctrl_deinit,
};

hu_error_t hu_peripheral_ctrl_tool_create(hu_allocator_t *alloc, hu_peripheral_t *peripheral,
                                           hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_peripheral_ctrl_ctx_t *ctx =
        (hu_peripheral_ctrl_ctx_t *)alloc->alloc(alloc->ctx, sizeof(hu_peripheral_ctrl_ctx_t));
    if (!ctx)
        return HU_ERR_OUT_OF_MEMORY;
    ctx->peripheral = peripheral;

    out->ctx = ctx;
    out->vtable = &peripheral_ctrl_vtable;
    return HU_OK;
}
