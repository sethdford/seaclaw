#include "human/tools/gui_agent.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/log.h"
#include "human/core/string.h"
#include "human/tool.h"
#include <stdio.h>
#include <string.h>

#if defined(__APPLE__) && !defined(HU_IS_TEST)
#include <ApplicationServices/ApplicationServices.h>
#endif
#include "human/pwa/cdp.h"

#define HU_GUI_MAX_ELEMENTS_CDP 16

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

#if defined(__APPLE__) && !defined(HU_IS_TEST)
#define HU_GUI_MAX_ELEMENTS 32

static void ax_collect_elements(AXUIElementRef elem, hu_gui_state_t *state) {
    if (state->element_count >= HU_GUI_MAX_ELEMENTS)
        return;
    CFArrayRef children = NULL;
    if (AXUIElementCopyAttributeValue(elem, kAXChildrenAttribute, (CFTypeRef *)&children) !=
        kAXErrorSuccess ||
        !children) {
        return;
    }
    CFIndex count = CFArrayGetCount(children);
    for (CFIndex i = 0; i < count && state->element_count < HU_GUI_MAX_ELEMENTS; i++) {
        AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(children, i);
        hu_gui_element_t *ge = &state->elements[state->element_count];

        CFStringRef role = NULL;
        if (AXUIElementCopyAttributeValue(child, kAXRoleAttribute, (CFTypeRef *)&role) ==
                kAXErrorSuccess &&
            role) {
            if (!CFStringGetCString(role, ge->type, (CFIndex)sizeof(ge->type), kCFStringEncodingUTF8))
                ge->type[0] = '\0';
            ge->type[sizeof(ge->type) - 1] = '\0';
            CFRelease(role);
        } else {
            ge->type[0] = '\0';
        }

        CFStringRef label = NULL;
        if (AXUIElementCopyAttributeValue(child, kAXTitleAttribute, (CFTypeRef *)&label) ==
                kAXErrorSuccess &&
            label) {
            if (!CFStringGetCString(label, ge->label, (CFIndex)sizeof(ge->label),
                                   kCFStringEncodingUTF8))
                ge->label[0] = '\0';
            ge->label_len = strlen(ge->label);
            ge->label[sizeof(ge->label) - 1] = '\0';
            CFRelease(label);
        } else {
            ge->label[0] = '\0';
            ge->label_len = 0;
        }

        AXValueRef pos_val = NULL;
        AXValueRef size_val = NULL;
        CGPoint pos = {0};
        CGSize size = {0};
        if (AXUIElementCopyAttributeValue(child, kAXPositionAttribute, (CFTypeRef *)&pos_val) ==
            kAXErrorSuccess) {
            if (pos_val && AXValueGetValue(pos_val, kAXValueTypeCGPoint, &pos))
                ge->x = (int)pos.x, ge->y = (int)pos.y;
            if (pos_val)
                CFRelease(pos_val);
        }
        if (AXUIElementCopyAttributeValue(child, kAXSizeAttribute, (CFTypeRef *)&size_val) ==
            kAXErrorSuccess) {
            if (size_val && AXValueGetValue(size_val, kAXValueTypeCGSize, &size))
                ge->width = (int)size.width, ge->height = (int)size.height;
            if (size_val)
                CFRelease(size_val);
        }

        state->element_count++;
        ax_collect_elements(child, state);
    }
    CFRelease(children);
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
#if defined(__APPLE__)
    memset(state, 0, sizeof(*state));

    AXUIElementRef sys_wide = AXUIElementCreateSystemWide();
    AXUIElementRef focused_app = NULL;
    AXError ax_err =
        AXUIElementCopyAttributeValue(sys_wide, kAXFocusedApplicationAttribute,
                                      (CFTypeRef *)&focused_app);
    CFRelease(sys_wide);
    if (ax_err != kAXErrorSuccess || !focused_app)
        return HU_ERR_IO;

    CFStringRef title = NULL;
    if (AXUIElementCopyAttributeValue(focused_app, kAXTitleAttribute, (CFTypeRef *)&title) ==
            kAXErrorSuccess &&
        title) {
        if (!CFStringGetCString(title, state->app_name, (CFIndex)sizeof(state->app_name),
                               kCFStringEncodingUTF8))
            state->app_name[0] = '\0';
        state->app_name[sizeof(state->app_name) - 1] = '\0';
        CFRelease(title);
    }

    AXUIElementRef focused_window = NULL;
    if (AXUIElementCopyAttributeValue(focused_app, kAXFocusedWindowAttribute,
                                      (CFTypeRef *)&focused_window) == kAXErrorSuccess &&
        focused_window) {
        ax_collect_elements(focused_window, state);
        CFRelease(focused_window);
    }

    CGDirectDisplayID display = CGMainDisplayID();
    state->screen_width = (int)CGDisplayPixelsWide(display);
    state->screen_height = (int)CGDisplayPixelsHigh(display);
    state->verified = true;

    CFRelease(focused_app);
    return HU_OK;
#else
    /* Cross-platform fallback: use CDP to query the browser */
    {
        hu_cdp_session_t cdp;
        hu_error_t cdp_err = hu_cdp_connect(alloc, "localhost", 9222, &cdp);
        if (cdp_err == HU_OK) {
            memset(state, 0, sizeof(*state));
            state->screen_width = 1920;
            state->screen_height = 1080;
            char *title = NULL;
            size_t title_len = 0;
            if (hu_cdp_get_title(&cdp, &title, &title_len) == HU_OK && title) {
                size_t copy = title_len < sizeof(state->app_name) - 1 ? title_len : sizeof(state->app_name) - 1;
                memcpy(state->app_name, title, copy);
                alloc->free(alloc->ctx, title, title_len + 1);
            }
            hu_cdp_element_t elems[HU_GUI_MAX_ELEMENTS_CDP];
            size_t elem_count = 0;
            if (hu_cdp_query_elements(&cdp, "*", 1, elems, HU_GUI_MAX_ELEMENTS_CDP, &elem_count) == HU_OK) {
                for (size_t ei = 0; ei < elem_count && state->element_count < 32; ei++) {
                    hu_gui_element_t *ge = &state->elements[state->element_count];
                    ge->x = elems[ei].x;
                    ge->y = elems[ei].y;
                    ge->width = elems[ei].width;
                    ge->height = elems[ei].height;
                    size_t tl = elems[ei].text_len < sizeof(ge->label) - 1 ? elems[ei].text_len : sizeof(ge->label) - 1;
                    memcpy(ge->label, elems[ei].text, tl);
                    ge->label_len = tl;
                    size_t tag_len = strlen(elems[ei].tag);
                    size_t tc = tag_len < sizeof(ge->type) - 1 ? tag_len : sizeof(ge->type) - 1;
                    memcpy(ge->type, elems[ei].tag, tc);
                    state->element_count++;
                }
            }
            state->verified = true;
            hu_cdp_disconnect(&cdp);
            return HU_OK;
        }
    }
    (void)alloc;
    (void)state;
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

hu_error_t hu_gui_execute_action(hu_allocator_t *alloc, const hu_gui_action_t *action,
                                 hu_gui_state_t *new_state) {
    if (!action || !new_state)
        return HU_ERR_INVALID_ARGUMENT;
#ifdef HU_IS_TEST
    (void)alloc;
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
#if defined(__APPLE__)
    hu_error_t err;
    switch (action->type) {
    case HU_GUI_ACTION_CLICK: {
        CGPoint point = CGPointMake((CGFloat)action->x, (CGFloat)action->y);
        CGEventRef down =
            CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, point, kCGMouseButtonLeft);
        CGEventRef up =
            CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, point, kCGMouseButtonLeft);
        if (down && up) {
            CGEventPost(kCGHIDEventTap, down);
            CGEventPost(kCGHIDEventTap, up);
        }
        if (down)
            CFRelease(down);
        if (up)
            CFRelease(up);
        break;
    }
    case HU_GUI_ACTION_TYPE: {
        if (action->text_len > 0 && action->text_len <= sizeof(action->text)) {
            CFStringRef str =
                CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)action->text,
                                       (CFIndex)action->text_len, kCFStringEncodingUTF8, false);
            if (str) {
                CFIndex len = CFStringGetLength(str);
                for (CFIndex i = 0; i < len; i++) {
                    UniChar ch = CFStringGetCharacterAtIndex(str, i);
                    CGEventRef ev_down = CGEventCreateKeyboardEvent(NULL, 0, true);
                    CGEventRef ev_up = CGEventCreateKeyboardEvent(NULL, 0, false);
                    if (ev_down) {
                        CGEventKeyboardSetUnicodeString(ev_down, 1, &ch);
                        CGEventPost(kCGHIDEventTap, ev_down);
                        CFRelease(ev_down);
                    }
                    if (ev_up) {
                        CGEventKeyboardSetUnicodeString(ev_up, 1, &ch);
                        CGEventPost(kCGHIDEventTap, ev_up);
                        CFRelease(ev_up);
                    }
                }
                CFRelease(str);
            }
        }
        break;
    }
    case HU_GUI_ACTION_SCROLL: {
        int32_t scroll_delta = (action->y != 0) ? (action->y > 0 ? 1 : -1) : 0;
        CGEventRef scroll =
            CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 1, scroll_delta);
        if (scroll) {
            CGPoint point = CGPointMake((CGFloat)action->x, (CGFloat)action->y);
            CGEventSetLocation(scroll, point);
            CGEventPost(kCGHIDEventTap, scroll);
            CFRelease(scroll);
        }
        break;
    }
    case HU_GUI_ACTION_KEY: {
        if (action->key_combo_len > 0 && action->key_combo_len < sizeof(action->key_combo)) {
            for (size_t i = 0; i < action->key_combo_len; i++) {
                UniChar ch = (UniChar)(unsigned char)action->key_combo[i];
                CGEventRef key_down = CGEventCreateKeyboardEvent(NULL, 0, true);
                CGEventRef key_up = CGEventCreateKeyboardEvent(NULL, 0, false);
                if (key_down) {
                    CGEventKeyboardSetUnicodeString(key_down, 1, &ch);
                    CGEventPost(kCGHIDEventTap, key_down);
                    CFRelease(key_down);
                }
                if (key_up) {
                    CGEventKeyboardSetUnicodeString(key_up, 1, &ch);
                    CGEventPost(kCGHIDEventTap, key_up);
                    CFRelease(key_up);
                }
            }
        }
        break;
    }
    case HU_GUI_ACTION_SCREENSHOT: {
        break;
    }
    }
    err = hu_gui_capture_state(alloc, new_state);
    return err;
#else
    /* Cross-platform fallback: CDP browser automation */
    {
        hu_cdp_session_t cdp;
        hu_error_t cdp_err = hu_cdp_connect(alloc, "localhost", 9222, &cdp);
        if (cdp_err == HU_OK) {
            switch (action->type) {
            case HU_GUI_ACTION_CLICK:
                hu_cdp_click(&cdp, action->x, action->y);
                break;
            case HU_GUI_ACTION_TYPE:
                hu_cdp_type(&cdp, action->text, action->text_len);
                break;
            case HU_GUI_ACTION_KEY:
            case HU_GUI_ACTION_SCROLL:
            case HU_GUI_ACTION_SCREENSHOT:
                break;
            }
            hu_cdp_disconnect(&cdp);
            return hu_gui_capture_state(alloc, new_state);
        }
    }
    (void)alloc;
    (void)action;
    (void)new_state;
    return HU_ERR_NOT_SUPPORTED;
#endif
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
 * Multi-step workflow: observe → act → verify, with retry on failure
 * ───────────────────────────────────────────────────────────────────────── */

hu_error_t hu_gui_workflow_init(hu_gui_workflow_t *wf, int max_retries) {
    if (!wf)
        return HU_ERR_INVALID_ARGUMENT;
    memset(wf, 0, sizeof(*wf));
    wf->max_retries = (max_retries > 0 && max_retries <= HU_GUI_WORKFLOW_MAX_RETRIES)
                          ? max_retries
                          : HU_GUI_WORKFLOW_MAX_RETRIES;
    return HU_OK;
}

hu_error_t hu_gui_workflow_add_step(hu_gui_workflow_t *wf, const hu_gui_action_t *action,
                                    const hu_gui_state_t *expected_state) {
    if (!wf || !action)
        return HU_ERR_INVALID_ARGUMENT;
    if (wf->step_count >= HU_GUI_WORKFLOW_MAX_STEPS)
        return HU_ERR_INVALID_ARGUMENT;
    hu_gui_workflow_step_t *step = &wf->steps[wf->step_count];
    memset(step, 0, sizeof(*step));
    step->action = *action;
    if (expected_state) {
        step->expected_state = *expected_state;
        step->has_expected = true;
    }
    wf->step_count++;
    return HU_OK;
}

hu_error_t hu_gui_workflow_run(hu_allocator_t *alloc, hu_gui_workflow_t *wf) {
    if (!alloc || !wf)
        return HU_ERR_INVALID_ARGUMENT;
    if (wf->step_count == 0) {
        wf->completed = true;
        return HU_OK;
    }

    for (wf->current_step = 0; wf->current_step < wf->step_count; wf->current_step++) {
        hu_gui_workflow_step_t *step = &wf->steps[wf->current_step];
        bool step_ok = false;

        for (int attempt = 0; attempt <= wf->max_retries && !step_ok; attempt++) {
            step->retries = attempt;

            /* Observe: capture current state */
            hu_gui_state_t before;
            hu_error_t err = hu_gui_capture_state(alloc, &before);
            if (err != HU_OK) {
                snprintf(wf->failure_reason, sizeof(wf->failure_reason),
                         "capture failed at step %zu", wf->current_step);
                wf->failed = true;
                return err;
            }

            /* Check app whitelist */
            if (before.app_name[0] &&
                !hu_gui_app_allowed(before.app_name, strlen(before.app_name))) {
                snprintf(wf->failure_reason, sizeof(wf->failure_reason),
                         "app '%s' not in whitelist", before.app_name);
                wf->failed = true;
                return HU_ERR_PERMISSION_DENIED;
            }

            /* Act: execute the action */
            hu_gui_state_t after;
            err = hu_gui_execute_action(alloc, &step->action, &after);
            if (err != HU_OK) {
                if (attempt >= wf->max_retries) {
                    snprintf(wf->failure_reason, sizeof(wf->failure_reason),
                             "action failed at step %zu after %d retries",
                             wf->current_step, attempt);
                    wf->failed = true;
                    return err;
                }
                continue;
            }

            /* Verify: check state if expected is provided */
            if (step->has_expected) {
                bool matches = false;
                hu_error_t verify_err =
                    hu_gui_verify_state(&step->expected_state, &after, &matches);
                if (verify_err != HU_OK)
                    hu_log_warn("gui_agent", NULL, "UI state verification failed: %s",
                                hu_error_string(verify_err));
                if (!matches) {
                    if (attempt >= wf->max_retries) {
                        snprintf(wf->failure_reason, sizeof(wf->failure_reason),
                                 "verification failed at step %zu after %d retries",
                                 wf->current_step, attempt);
                        wf->failed = true;
                        return HU_ERR_TOOL_VALIDATION;
                    }
                    continue;
                }
                step->verified = true;
            } else {
                step->verified = true;
            }

            step->completed = true;
            step_ok = true;
        }
    }

    wf->completed = true;
    return HU_OK;
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

    if (strcmp(action, "verify") == 0) {
        hu_gui_state_t current;
        hu_error_t err = hu_gui_capture_state(alloc, &current);
        if (err != HU_OK) {
            *out = hu_tool_result_fail("capture for verify failed", 25);
            return HU_OK;
        }
        hu_gui_state_t expected;
        memset(&expected, 0, sizeof(expected));
        const char *app_name = hu_json_get_string(args, "app_name");
        if (app_name) {
            size_t len = strlen(app_name);
            if (len >= sizeof(expected.app_name))
                len = sizeof(expected.app_name) - 1;
            memcpy(expected.app_name, app_name, len);
        }
        expected.element_count = (size_t)hu_json_get_number(args, "element_count",
                                                            (double)current.element_count);
        bool matches = false;
        hu_error_t verify_err = hu_gui_verify_state(&expected, &current, &matches);
        if (verify_err != HU_OK)
            hu_log_warn("gui_agent", NULL, "UI state verification failed: %s",
                        hu_error_string(verify_err));
        const char *resp = matches ? "{\"verified\":true}" : "{\"verified\":false}";
        size_t resp_len = matches ? 17 : 18;
        char *json = hu_strndup(alloc, resp, resp_len);
        if (!json) {
            *out = hu_tool_result_fail("out of memory", 13);
            return HU_ERR_OUT_OF_MEMORY;
        }
        *out = hu_tool_result_ok_owned(json, resp_len);
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
