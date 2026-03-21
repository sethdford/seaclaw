#include "human/tools/visual_grounding.h"
#include "human/context/vision.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/multimodal.h"
#include "human/provider.h"
#include <stdio.h>
#include <string.h>

#if defined(HU_IS_TEST) && HU_IS_TEST
static const char k_hu_vg_mock_selector[] = "#mock-grounded-button";
#endif

hu_error_t hu_visual_ground_action(hu_allocator_t *alloc, hu_provider_t *provider,
                                 const char *model, size_t model_len,
                                 const char *screenshot_path, size_t path_len,
                                 const char *action_description, size_t action_len,
                                 double *out_x, double *out_y, char **out_selector,
                                 size_t *out_selector_len) {
#if defined(HU_IS_TEST) && HU_IS_TEST
    if (!alloc || !screenshot_path || path_len == 0 || !action_description || action_len == 0 ||
        !out_x || !out_y)
        return HU_ERR_INVALID_ARGUMENT;
    (void)provider;
    (void)model;
    (void)model_len;
    if (out_selector) {
        size_t sl = sizeof(k_hu_vg_mock_selector) - 1;
        char *dup = hu_strndup(alloc, k_hu_vg_mock_selector, sl);
        if (!dup)
            return HU_ERR_OUT_OF_MEMORY;
        *out_selector = dup;
        if (out_selector_len)
            *out_selector_len = sl;
    } else if (out_selector_len)
        *out_selector_len = 0;
    *out_x = 100.0;
    *out_y = 200.0;
    return HU_OK;
#else
    if (!alloc || !provider || !screenshot_path || path_len == 0 || !action_description ||
        action_len == 0 || !out_x || !out_y)
        return HU_ERR_INVALID_ARGUMENT;

    *out_x = -1.0;
    *out_y = -1.0;
    if (out_selector)
        *out_selector = NULL;
    if (out_selector_len)
        *out_selector_len = 0;

    if (!provider->vtable || !provider->vtable->chat)
        return HU_ERR_NOT_SUPPORTED;

    const char *use_model = model ? model : "";
    size_t use_model_len = model ? model_len : 0;

    bool can_direct_vision =
        provider->vtable->supports_vision && provider->vtable->supports_vision(provider->ctx);
    if (can_direct_vision && provider->vtable->supports_vision_for_model &&
        !provider->vtable->supports_vision_for_model(provider->ctx, use_model, use_model_len))
        can_direct_vision = false;

    static const char sys[] = "You are a UI element locator. Output only valid JSON.";
    hu_json_value_t *json = NULL;
    hu_error_t err = HU_OK;

    if (can_direct_vision) {
        char *base64 = NULL;
        size_t base64_len = 0;
        char *media_type = NULL;
        size_t media_type_len = 0;
        err = hu_vision_read_image(alloc, screenshot_path, path_len, &base64, &base64_len,
                                   &media_type, &media_type_len);
        if (err != HU_OK)
            return err;

        char user_text[2048];
        int un = snprintf(
            user_text, sizeof(user_text),
            "You are given a screenshot of a user interface.\n\n"
            "I want to: %.*s\n\n"
            "Return ONLY valid JSON with the approximate screen coordinates to click:\n"
            "{\"x\": <number>, \"y\": <number>, \"selector\": \"<CSS selector if identifiable>\", "
            "\"confidence\": 0.0-1.0}\n"
            "If you can't determine coordinates, set x and y to -1.",
            (int)(action_len < 500 ? action_len : 500), action_description);
        size_t user_text_len = 0;
        if (un > 0) {
            if ((size_t)un < sizeof(user_text))
                user_text_len = (size_t)un;
            else
                user_text_len = sizeof(user_text) - 1;
        }

        hu_content_part_t parts[2];
        parts[0].tag = HU_CONTENT_PART_TEXT;
        parts[0].data.text.ptr = user_text;
        parts[0].data.text.len = user_text_len;

        parts[1].tag = HU_CONTENT_PART_IMAGE_BASE64;
        parts[1].data.image_base64.data = base64;
        parts[1].data.image_base64.data_len = base64_len;
        parts[1].data.image_base64.media_type = media_type;
        parts[1].data.image_base64.media_type_len = media_type_len;

        hu_chat_message_t sys_msg = {0};
        sys_msg.role = HU_ROLE_SYSTEM;
        sys_msg.content = sys;
        sys_msg.content_len = sizeof(sys) - 1;

        hu_chat_message_t user_msg = {0};
        user_msg.role = HU_ROLE_USER;
        user_msg.content_parts = parts;
        user_msg.content_parts_count = 2;

        hu_chat_message_t messages[] = {sys_msg, user_msg};
        hu_chat_request_t req = {
            .messages = messages,
            .messages_count = 2,
            .model = use_model,
            .model_len = use_model_len,
            .temperature = 0.0,
            .max_tokens = 256,
            .timeout_secs = 30,
        };

        hu_chat_response_t resp = {0};
        err = provider->vtable->chat(provider->ctx, alloc, &req, use_model, use_model_len, 0.0,
                                     &resp);

        alloc->free(alloc->ctx, base64, base64_len + 1);
        alloc->free(alloc->ctx, media_type, media_type_len + 1);

        if (err != HU_OK) {
            hu_chat_response_free(alloc, &resp);
            return err;
        }
        if (!resp.content || resp.content_len == 0) {
            hu_chat_response_free(alloc, &resp);
            return HU_ERR_IO;
        }
        err = hu_json_parse(alloc, resp.content, resp.content_len, &json);
        hu_chat_response_free(alloc, &resp);
    } else {
        if (!provider->vtable->chat_with_system)
            return HU_ERR_NOT_SUPPORTED;

        char *desc = NULL;
        size_t desc_len = 0;
        err = hu_vision_describe_image(alloc, provider, screenshot_path, path_len, use_model,
                                       use_model_len, &desc, &desc_len);
        if (err != HU_OK || !desc)
            return err != HU_OK ? err : HU_ERR_IO;

        char prompt[2048];
        int pn = snprintf(
            prompt, sizeof(prompt),
            "Given this screenshot description:\n%.*s\n\n"
            "I want to: %.*s\n\n"
            "Return ONLY valid JSON with the approximate screen coordinates to click:\n"
            "{\"x\": <number>, \"y\": <number>, \"selector\": \"<CSS selector if identifiable>\", "
            "\"confidence\": 0.0-1.0}\n"
            "If you can't determine coordinates, set x and y to -1.",
            (int)(desc_len < 1000 ? desc_len : 1000), desc,
            (int)(action_len < 500 ? action_len : 500), action_description);
        alloc->free(alloc->ctx, desc, desc_len + 1);

        char *llm_out = NULL;
        size_t llm_out_len = 0;
        err = provider->vtable->chat_with_system(
            provider->ctx, alloc, sys, sizeof(sys) - 1, prompt, pn > 0 ? (size_t)pn : 0, use_model,
            use_model_len, 0.0, &llm_out, &llm_out_len);

        if (err != HU_OK || !llm_out) {
            if (llm_out)
                alloc->free(alloc->ctx, llm_out, llm_out_len + 1);
            return HU_ERR_IO;
        }

        err = hu_json_parse(alloc, llm_out, llm_out_len, &json);
        alloc->free(alloc->ctx, llm_out, llm_out_len + 1);
    }

    if (err == HU_OK && json) {
        *out_x = hu_json_get_number(json, "x", -1.0);
        *out_y = hu_json_get_number(json, "y", -1.0);

        if (out_selector) {
            const char *sel = hu_json_get_string(json, "selector");
            if (sel && sel[0]) {
                size_t sl = strlen(sel);
                char *dup = hu_strndup(alloc, sel, sl);
                *out_selector = dup;
                if (out_selector_len)
                    *out_selector_len = dup ? sl : 0;
            }
        }
        hu_json_free(alloc, json);
    }

    return HU_OK;
#endif
}
