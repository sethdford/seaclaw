#include "human/tools/visual_grounding.h"
#include "human/context/vision.h"
#include "human/core/error.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>

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
    *out_x = 512.0;
    *out_y = 384.0;
    if (out_selector)
        *out_selector = NULL;
    if (out_selector_len)
        *out_selector_len = 0;
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

    if (!provider->vtable || !provider->vtable->supports_vision ||
        !provider->vtable->supports_vision(provider->ctx) ||
        !provider->vtable->chat_with_system)
        return HU_ERR_NOT_SUPPORTED;

    char *desc = NULL;
    size_t desc_len = 0;
    const char *use_model = model ? model : "";
    hu_error_t err =
        hu_vision_describe_image(alloc, provider, screenshot_path, path_len, use_model, model_len,
                                 &desc, &desc_len);
    if (err != HU_OK || !desc)
        return err != HU_OK ? err : HU_ERR_IO;

    char prompt[2048];
    int pn =
        snprintf(prompt, sizeof(prompt),
                 "Given this screenshot description:\n%.*s\n\n"
                 "I want to: %.*s\n\n"
                 "Return ONLY valid JSON with the approximate screen coordinates to click:\n"
                 "{\"x\": <number>, \"y\": <number>, \"selector\": \"<CSS selector if identifiable>\", "
                 "\"confidence\": 0.0-1.0}\n"
                 "If you can't determine coordinates, set x and y to -1.",
                 (int)(desc_len < 1000 ? desc_len : 1000), desc,
                 (int)(action_len < 500 ? action_len : 500), action_description);
    alloc->free(alloc->ctx, desc, desc_len + 1);

    static const char sys[] = "You are a UI element locator. Output only valid JSON.";
    char *llm_out = NULL;
    size_t llm_out_len = 0;
    err = provider->vtable->chat_with_system(provider->ctx, alloc, sys, sizeof(sys) - 1, prompt,
                                             pn > 0 ? (size_t)pn : 0, use_model, model_len, 0.0,
                                             &llm_out, &llm_out_len);

    if (err != HU_OK || !llm_out) {
        if (llm_out)
            alloc->free(alloc->ctx, llm_out, llm_out_len + 1);
        return HU_ERR_IO;
    }

    hu_json_value_t *json = NULL;
    err = hu_json_parse(alloc, llm_out, llm_out_len, &json);
    alloc->free(alloc->ctx, llm_out, llm_out_len + 1);

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
