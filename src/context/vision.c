/* Vision utilities — image reading, base64 encoding, and context building for
 * multimodal (image) understanding in conversations. */

#include "seaclaw/context/vision.h"
#include "seaclaw/core/string.h"
#include "seaclaw/multimodal.h"
#include <stdio.h>
#include <string.h>

#ifndef SC_IS_TEST
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__CYGWIN__)
#include <sys/stat.h>
#endif
#endif

sc_error_t sc_vision_read_image(sc_allocator_t *alloc, const char *path, size_t path_len,
                                char **base64_out, size_t *base64_len, char **media_type_out,
                                size_t *media_type_len) {
    if (!alloc || !base64_out || !base64_len || !media_type_out || !media_type_len)
        return SC_ERR_INVALID_ARGUMENT;
    if (!path)
        return SC_ERR_INVALID_ARGUMENT;
    *base64_out = NULL;
    *base64_len = 0;
    *media_type_out = NULL;
    *media_type_len = 0;

#if defined(SC_IS_TEST) && SC_IS_TEST
    (void)path_len;
    /* Mock: return a minimal valid base64 string */
    const char *mock_b64 = "iVBORw0KGgo=";
    const char *mock_mime = "image/png";
    size_t b64_len = strlen(mock_b64);
    size_t mime_len = strlen(mock_mime);
    char *b64 = sc_strndup(alloc, mock_b64, b64_len);
    char *mime = sc_strndup(alloc, mock_mime, mime_len);
    if (!b64 || !mime) {
        if (b64)
            alloc->free(alloc->ctx, b64, b64_len + 1);
        if (mime)
            alloc->free(alloc->ctx, mime, mime_len + 1);
        return SC_ERR_OUT_OF_MEMORY;
    }
    *base64_out = b64;
    *base64_len = b64_len;
    *media_type_out = mime;
    *media_type_len = mime_len;
    return SC_OK;
#else
#ifndef _WIN32
#ifndef _WIN64
#ifndef __CYGWIN__
    if (path_len == 0)
        return SC_ERR_INVALID_ARGUMENT;

    char path_buf[4096];
    if (path_len >= sizeof(path_buf))
        return SC_ERR_INVALID_ARGUMENT;
    memcpy(path_buf, path, path_len);
    path_buf[path_len] = '\0';

    struct stat st;
    if (stat(path_buf, &st) != 0)
        return SC_ERR_NOT_FOUND;
    if (!S_ISREG(st.st_mode))
        return SC_ERR_INVALID_ARGUMENT;
    if ((size_t)st.st_size > SC_MULTIMODAL_MAX_IMAGE_SIZE)
        return SC_ERR_INVALID_ARGUMENT;

    FILE *f = fopen(path_buf, "rb");
    if (!f)
        return SC_ERR_IO;

    size_t file_size = (size_t)st.st_size;
    unsigned char *buf = (unsigned char *)alloc->alloc(alloc->ctx, file_size);
    if (!buf) {
        fclose(f);
        return SC_ERR_IO;
    }
    size_t nr = fread(buf, 1, file_size, f);
    fclose(f);
    if (nr != file_size) {
        alloc->free(alloc->ctx, buf, file_size);
        return SC_ERR_IO;
    }

    const char *mime = sc_multimodal_detect_mime(buf, file_size);
    if (!mime)
        mime = "application/octet-stream";

    char *b64 = NULL;
    size_t b64_len = 0;
    sc_error_t err = sc_multimodal_encode_base64(alloc, buf, file_size, &b64, &b64_len);
    alloc->free(alloc->ctx, buf, file_size);
    if (err != SC_OK)
        return err;

    size_t mime_len = strlen(mime);
    char *mime_dup = sc_strndup(alloc, mime, mime_len);
    if (!mime_dup) {
        alloc->free(alloc->ctx, b64, b64_len + 1);
        return SC_ERR_OUT_OF_MEMORY;
    }

    *base64_out = b64;
    *base64_len = b64_len;
    *media_type_out = mime_dup;
    *media_type_len = mime_len;
    return SC_OK;
#endif
#endif
#endif
    (void)path_len;
    return SC_ERR_NOT_SUPPORTED;
#endif
}

sc_error_t sc_vision_describe_image(sc_allocator_t *alloc, sc_provider_t *provider,
                                    const char *image_path, size_t image_path_len,
                                    const char *model, size_t model_len, char **description_out,
                                    size_t *description_len) {
    if (!alloc || !provider || !description_out || !description_len)
        return SC_ERR_INVALID_ARGUMENT;
    *description_out = NULL;
    *description_len = 0;

    if (!provider->vtable || !provider->vtable->supports_vision ||
        !provider->vtable->supports_vision(provider->ctx))
        return SC_ERR_NOT_SUPPORTED;
    if (provider->vtable->supports_vision_for_model &&
        !provider->vtable->supports_vision_for_model(provider->ctx, model, model_len))
        return SC_ERR_NOT_SUPPORTED;

    char *base64 = NULL;
    size_t base64_len = 0;
    char *media_type = NULL;
    size_t media_type_len = 0;
    sc_error_t err = sc_vision_read_image(alloc, image_path, image_path_len, &base64, &base64_len,
                                          &media_type, &media_type_len);
    if (err != SC_OK)
        return err;

    /* Build content parts: text + image */
    sc_content_part_t parts[2];
    parts[0].tag = SC_CONTENT_PART_TEXT;
    parts[0].data.text.ptr = "What is in this image?";
    parts[0].data.text.len = 22;

    parts[1].tag = SC_CONTENT_PART_IMAGE_BASE64;
    parts[1].data.image_base64.data = base64;
    parts[1].data.image_base64.data_len = base64_len;
    parts[1].data.image_base64.media_type = media_type;
    parts[1].data.image_base64.media_type_len = media_type_len;

    sc_chat_message_t sys_msg = {
        .role = SC_ROLE_SYSTEM,
        .content =
            "Describe this image briefly in 1-2 sentences. Focus on what's visually present.",
        .content_len = 79,
        .name = NULL,
        .name_len = 0,
        .tool_call_id = NULL,
        .tool_call_id_len = 0,
        .content_parts = NULL,
        .content_parts_count = 0,
        .tool_calls = NULL,
        .tool_calls_count = 0,
    };

    sc_chat_message_t user_msg = {
        .role = SC_ROLE_USER,
        .content = NULL,
        .content_len = 0,
        .name = NULL,
        .name_len = 0,
        .tool_call_id = NULL,
        .tool_call_id_len = 0,
        .content_parts = parts,
        .content_parts_count = 2,
        .tool_calls = NULL,
        .tool_calls_count = 0,
    };

    sc_chat_message_t messages[] = {sys_msg, user_msg};
    sc_chat_request_t req = {
        .messages = messages,
        .messages_count = 2,
        .model = model,
        .model_len = model_len,
        .temperature = 0.3,
        .max_tokens = 128,
        .tools = NULL,
        .tools_count = 0,
        .timeout_secs = 30,
        .reasoning_effort = NULL,
        .reasoning_effort_len = 0,
        .response_format = NULL,
        .response_format_len = 0,
    };

    sc_chat_response_t resp = {0};
    err = provider->vtable->chat(provider->ctx, alloc, &req, model, model_len, 0.3, &resp);

    alloc->free(alloc->ctx, base64, base64_len + 1);
    alloc->free(alloc->ctx, media_type, media_type_len + 1);

    if (err != SC_OK) {
        sc_chat_response_free(alloc, &resp);
        return err;
    }
    if (!resp.content || resp.content_len == 0) {
        sc_chat_response_free(alloc, &resp);
        return SC_ERR_PROVIDER_RESPONSE;
    }

    size_t desc_len = resp.content_len;
    *description_out = sc_strndup(alloc, resp.content, desc_len);
    sc_chat_response_free(alloc, &resp);
    if (!*description_out)
        return SC_ERR_OUT_OF_MEMORY;
    *description_len = desc_len;
    return SC_OK;
}

char *sc_vision_build_context(sc_allocator_t *alloc, const char *description,
                              size_t description_len, size_t *out_len) {
    if (!alloc || !description || description_len == 0) {
        if (out_len)
            *out_len = 0;
        return NULL;
    }

    const char *prefix = "\n### Image Context\nThe user shared an image: ";
    const char *suffix = "\nRespond naturally to what you see — comment on it, react to it, or "
                         "reference it in conversation.\n";

    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t total = prefix_len + description_len + suffix_len;

    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!buf)
        return NULL;

    memcpy(buf, prefix, prefix_len);
    memcpy(buf + prefix_len, description, description_len);
    memcpy(buf + prefix_len + description_len, suffix, suffix_len);
    buf[total] = '\0';

    if (out_len)
        *out_len = total;
    return buf;
}
