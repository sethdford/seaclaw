#include "human/tools/gui_agent.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/tool.h"
#include <string.h>

#define GUI_AGENT_PARAMS                                                                           \
    "{\"type\":\"object\",\"properties\":{\"action\":{\"type\":\"string\",\"enum\":[\"capture\"," \
    "\"execute\",\"verify\",\"app_allowed\"]},\"type\":{\"type\":\"string\"},\"x\":{\"type\":"     \
    "\"integer\"},\"y\":{\"type\":\"integer\"},\"text\":{\"type\":\"string\"},\"key_combo\":{"   \
    "\"type\":\"string\"},\"app_name\":{\"type\":\"string\"}},\"required\":[\"action\"]}"

#ifdef HU_IS_TEST
static void mock_state_init(hu_gui_state_t *state) {
    memset(state, 0, sizeof(*state));
    state->screen_width = 1920;
    state->screen_height = 1080;
    state->element_count = 3;
    state->verified = true;

    /* button "OK" */
    memcpy(state->elements[0].label, "OK", 2);
    state->elements[0].label_len = 2;
    state->elements[0].x = 100;
    state->elements[0].y = 200;
    state->elements[0].width = 80;
    state->elements[0].height = 32;
    memcpy(state->elements[0].type, "button", 6);

    /* text field "Name" */
    memcpy(state->elements[1].label, "Name", 4);
    state->elements[1].label_len = 4;
    state->elements[1].x = 100;
    state->elements[1].y = 250;
    state->elements[1].width = 200;
    state->elements[1].height = 24;
    memcpy(state->elements[1].type, "text_field", 10);

    /* link "Help" */
    memcpy(state->elements[2].label, "Help", 4);
    state->elements[2].label_len = 4;
    state->elements[2].x = 100;
    state->elements[2].y = 300;
    state->elements[2].width = 60;
    state->elements[2].height = 20;
    memcpy(state->elements[2].type, "link", 4);
}
#endif

hu_error_t hu_gui_capture_state(hu_allocator_t *alloc, hu_gui_state_t *state) {
    if (!alloc || !state)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    mock_state_init(state);
    memcpy(state->app_name, "Calculator", 10);
    return HU_OK;
#else
    (void)state;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_gui_execute_action(hu_allocator_t *alloc, const hu_gui_action_t *action,
                                 hu_gui_state_t *new_state) {
    (void)alloc;
    if (!action || !new_state)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    mock_state_init(new_state);
    switch (action->type) {
    case HU_GUI_ACTION_CLICK:
        memcpy(new_state->app_name, "Calculator", 10);
        break;
    case HU_GUI_ACTION_TYPE:
        memcpy(new_state->app_name, "TextEdit", 8);
        break;
    case HU_GUI_ACTION_SCROLL:
        memcpy(new_state->app_name, "Notes", 5);
        break;
    case HU_GUI_ACTION_KEY:
        memcpy(new_state->app_name, "Calculator", 10);
        break;
    case HU_GUI_ACTION_SCREENSHOT:
        memcpy(new_state->app_name, "Calculator", 10);
        break;
    }
    return HU_OK;
#else
    (void)action;
    (void)new_state;
    return HU_ERR_NOT_SUPPORTED;
#endif
}

hu_error_t hu_gui_verify_state(const hu_gui_state_t *expected, const hu_gui_state_t *actual,
                               bool *matches) {
    if (!expected || !actual || !matches)
        return HU_ERR_INVALID_ARGUMENT;
    *matches = (expected->element_count == actual->element_count &&
                strcmp(expected->app_name, actual->app_name) == 0);
    return HU_OK;
}

bool hu_gui_app_allowed(const char *app_name, size_t name_len) {
    if (!app_name || name_len == 0)
        return false;
    if (name_len >= 10 && strncmp(app_name, "Calculator", 10) == 0)
        return true;
    if (name_len >= 8 && strncmp(app_name, "TextEdit", 8) == 0)
        return true;
    if (name_len >= 5 && strncmp(app_name, "Notes", 5) == 0)
        return true;
    if (name_len >= 8 && strncmp(app_name, "Terminal", 8) == 0)
        return false;
    if (name_len >= 17 && strncmp(app_name, "System Preferences", 17) == 0)
        return false;
    return false;
}

const char *hu_gui_action_type_name(hu_gui_action_type_t type) {
    switch (type) {
    case HU_GUI_ACTION_CLICK:
        return "click";
    case HU_GUI_ACTION_TYPE:
        return "type";
    case HU_GUI_ACTION_SCROLL:
        return "scroll";
    case HU_GUI_ACTION_KEY:
        return "key";
    case HU_GUI_ACTION_SCREENSHOT:
        return "screenshot";
    }
    return "unknown";
}

/* ─────────────────────────────────────────────────────────────────────────
 * Tool vtable
 * ───────────────────────────────────────────────────────────────────────── */

static hu_error_t gui_agent_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                                    hu_tool_result_t *out) {
    (void)ctx;
    if (!alloc || !args || !out)
        return HU_ERR_INVALID_ARGUMENT;

    const char *action = hu_json_get_string(args, "action");
    if (!action || !action[0]) {
        *out = hu_tool_result_fail("missing action", 14);
        return HU_OK;
    }

    if (strcmp(action, "capture") == 0) {
        hu_gui_state_t state;
        hu_error_t err = hu_gui_capture_state(alloc, &state);
        if (err != HU_OK) {
            *out = hu_tool_result_fail("capture failed", 14);
            return HU_OK;
        }
        char *json = hu_strndup(alloc, "{\"elements\":3,\"app\":\"Calculator\"}", 33);
        if (!json) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(json, 33);
        return HU_OK;
    }

    if (strcmp(action, "execute") == 0) {
        hu_gui_action_t act;
        memset(&act, 0, sizeof(act));
        act.type = HU_GUI_ACTION_CLICK;
        act.x = (int)hu_json_get_number(args, "x", 0);
        act.y = (int)hu_json_get_number(args, "y", 0);
        const char *text = hu_json_get_string(args, "text");
        if (text) {
            size_t len = strlen(text);
            if (len >= sizeof(act.text))
                len = sizeof(act.text) - 1;
            memcpy(act.text, text, len);
            act.text_len = len;
        }
        const char *type_str = hu_json_get_string(args, "type");
        if (type_str) {
            if (strcmp(type_str, "type") == 0)
                act.type = HU_GUI_ACTION_TYPE;
            else if (strcmp(type_str, "scroll") == 0)
                act.type = HU_GUI_ACTION_SCROLL;
            else if (strcmp(type_str, "key") == 0)
                act.type = HU_GUI_ACTION_KEY;
            else if (strcmp(type_str, "screenshot") == 0)
                act.type = HU_GUI_ACTION_SCREENSHOT;
        }
        hu_gui_state_t new_state;
        hu_error_t err = hu_gui_execute_action(alloc, &act, &new_state);
        if (err != HU_OK) {
            *out = hu_tool_result_fail("execute failed", 14);
            return HU_OK;
        }
        char *json = hu_strndup(alloc, "{\"success\":true}", 16);
        if (!json) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(json, 16);
        return HU_OK;
    }

    if (strcmp(action, "app_allowed") == 0) {
        const char *app_name = hu_json_get_string(args, "app_name");
        if (!app_name || !app_name[0]) {
            *out = hu_tool_result_fail("missing app_name", 15);
            return HU_OK;
        }
        bool allowed = hu_gui_app_allowed(app_name, strlen(app_name));
        char *msg = hu_strndup(alloc, allowed ? "true" : "false", allowed ? 4 : 5);
        if (!msg) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(msg, allowed ? 4 : 5);
        return HU_OK;
    }

    *out = hu_tool_result_fail("unknown action", 14);
    return HU_OK;
}

static const char *gui_agent_name(void *ctx) {
    (void)ctx;
    return "gui_agent";
}

static const char *gui_agent_description(void *ctx) {
    (void)ctx;
    return "Visual GUI agent: capture state, execute actions (click, type, scroll, key, screenshot)";
}

static const char *gui_agent_parameters_json(void *ctx) {
    (void)ctx;
    return GUI_AGENT_PARAMS;
}

static const hu_tool_vtable_t gui_agent_vtable = {
    .execute = gui_agent_execute,
    .name = gui_agent_name,
    .description = gui_agent_description,
    .parameters_json = gui_agent_parameters_json,
    .deinit = NULL,
};

hu_error_t hu_gui_agent_create(hu_allocator_t *alloc, hu_tool_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    out->ctx = NULL;
    out->vtable = &gui_agent_vtable;
    return HU_OK;
}
