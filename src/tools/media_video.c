/* media_video — Generate videos via Veo 3.1 on Vertex AI.
 * Submits a predictLongRunning request, polls until done, downloads the
 * resulting MP4 from the GCS URI, and writes it to a temp file. */

#include "human/tools/media_video.h"
#include "human/agent.h"
#include "human/agent/tool_context.h"
#include "human/config.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/http.h"
#include "human/core/json.h"
#include "human/core/string.h"
#include "human/core/vertex_auth.h"
#include "human/tool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define mv_sleep_secs(s) Sleep((s) * 1000)
#else
#include <unistd.h>
#define mv_sleep_secs(s) sleep((unsigned)(s))
#endif

#define MV_PROMPT_MAX        4000
#define MV_POLL_INTERVAL_SEC 15
#define MV_POLL_MAX_ATTEMPTS 12 /* 12 * 15s = 3 min max */

static const char *mv_name(void *ctx) { (void)ctx; return "media_video"; }

static const char *mv_desc(void *ctx) {
    (void)ctx;
    return "Generate a short video from a text prompt using Veo 3.1 on Vertex AI. "
           "Returns a local file path to the generated MP4.";
}

static const char *mv_params(void *ctx) {
    (void)ctx;
    return "{\"type\":\"object\",\"properties\":{"
           "\"prompt\":{\"type\":\"string\",\"description\":\"Video description\"},"
           "\"duration\":{\"type\":\"integer\","
           "\"enum\":[4,6,8],\"description\":\"Duration in seconds (default 8)\"},"
           "\"aspect_ratio\":{\"type\":\"string\","
           "\"enum\":[\"16:9\",\"9:16\"],\"description\":\"Aspect ratio (default 16:9)\"},"
           "\"model\":{\"type\":\"string\","
           "\"enum\":[\"veo_3.1\",\"veo_3.1_fast\",\"veo_3.1_lite\"],"
           "\"description\":\"Model variant (default veo_3.1)\"}},"
           "\"required\":[\"prompt\"]}";
}

static const char *mv_model_id(const char *model) {
    if (strcmp(model, "veo_3.1_fast") == 0) return "veo-3.1-fast-generate-001";
    if (strcmp(model, "veo_3.1_lite") == 0) return "veo-3.1-lite-generate-001";
    return "veo-3.1-generate-001";
}

static bool mv_model_ok(const char *s) {
    if (!s) return false;
    return strcmp(s, "veo_3.1") == 0 || strcmp(s, "veo_3.1_fast") == 0 ||
           strcmp(s, "veo_3.1_lite") == 0;
}

static hu_error_t mv_execute(void *ctx, hu_allocator_t *alloc, const hu_json_value_t *args,
                             hu_tool_result_t *out) {
    (void)ctx;
    if (!alloc || !args || !out)
        return HU_ERR_INVALID_ARGUMENT;

    const char *prompt = hu_json_get_string(args, "prompt");
    if (!prompt || !prompt[0]) {
        *out = hu_tool_result_fail("prompt is required", 18);
        return HU_OK;
    }
    if (strlen(prompt) > MV_PROMPT_MAX) {
        *out = hu_tool_result_fail("prompt too long", 15);
        return HU_OK;
    }

    const char *model = hu_json_get_string(args, "model");
    if (!model) {
        hu_agent_t *cfg_agent = hu_agent_get_current_for_tools();
        if (cfg_agent && cfg_agent->config && cfg_agent->config->media_gen.default_video_model)
            model = cfg_agent->config->media_gen.default_video_model;
        else
            model = "veo_3.1";
    } else if (!mv_model_ok(model)) {
        *out = hu_tool_result_fail("invalid model", 13);
        return HU_OK;
    }

    double duration = hu_json_get_number(args, "duration", 8.0);
    if (duration != 4.0 && duration != 6.0 && duration != 8.0)
        duration = 8.0;

    const char *aspect = hu_json_get_string(args, "aspect_ratio");
    if (!aspect) aspect = "16:9";
    else if (strcmp(aspect, "16:9") != 0 && strcmp(aspect, "9:16") != 0) {
        *out = hu_tool_result_fail("invalid aspect_ratio", 20);
        return HU_OK;
    }

#if defined(HU_IS_TEST) && HU_IS_TEST
    char mock_path[256];
    size_t plen = strlen(prompt);
    if (plen > 30) plen = 30;
    int n = snprintf(mock_path, sizeof(mock_path), "/tmp/human_vid_mock_%.*s.mp4",
                     (int)plen, prompt);
    if (n <= 0 || (size_t)n >= sizeof(mock_path)) {
        *out = hu_tool_result_fail("mock path overflow", 18);
        return HU_OK;
    }
    char *path_copy = hu_strndup(alloc, mock_path, (size_t)n);
    char *desc = hu_strndup(alloc, mock_path, (size_t)n);
    if (!path_copy || !desc) {
        if (path_copy) alloc->free(alloc->ctx, path_copy, (size_t)n + 1);
        if (desc) alloc->free(alloc->ctx, desc, (size_t)n + 1);
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = hu_tool_result_ok_with_media(desc, (size_t)n, path_copy, (size_t)n);
    return HU_OK;
#else
    hu_vertex_auth_t vauth = {0};
    hu_error_t err = hu_vertex_auth_load_adc(&vauth, alloc);
    if (err != HU_OK) {
        *out = hu_tool_result_fail("Vertex AI credentials not configured", 36);
        return HU_OK;
    }
    err = hu_vertex_auth_ensure_token(&vauth, alloc);
    if (err != HU_OK) {
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("failed to obtain Vertex AI token", 32);
        return HU_OK;
    }

    const char *project = NULL;
    const char *region = NULL;
    hu_agent_t *mv_agent = hu_agent_get_current_for_tools();
    if (mv_agent && mv_agent->config) {
        project = mv_agent->config->media_gen.vertex_project;
        region = mv_agent->config->media_gen.vertex_region;
    }
    if (!project) project = getenv("GOOGLE_CLOUD_PROJECT");
    if (!project) project = getenv("VERTEX_PROJECT");
    if (!region) region = getenv("GOOGLE_CLOUD_LOCATION");
    if (!region) region = "us-central1";
    if (!project || !project[0]) {
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("GOOGLE_CLOUD_PROJECT not set", 28);
        return HU_OK;
    }

    const char *mid = mv_model_id(model);

    /* Step 1: Submit predictLongRunning */
    char url[512];
    int ulen = snprintf(url, sizeof(url),
                        "https://%s-aiplatform.googleapis.com/v1/projects/%s/locations/%s/"
                        "publishers/google/models/%s:predictLongRunning",
                        region, project, region, mid);
    if (ulen <= 0 || (size_t)ulen >= sizeof(url)) {
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("URL too long", 12);
        return HU_OK;
    }

    /* Build request: { instances: [{prompt}], parameters: {storageUri, aspectRatio, ...} } */
    hu_json_value_t *root = hu_json_object_new(alloc);
    hu_json_value_t *instances = hu_json_array_new(alloc);
    hu_json_value_t *inst = hu_json_object_new(alloc);
    hu_json_value_t *pv = hu_json_string_new(alloc, prompt, strlen(prompt));
    if (!root || !instances || !inst || !pv) {
        hu_json_free(alloc, root); hu_json_free(alloc, instances);
        hu_json_free(alloc, inst); hu_json_free(alloc, pv);
        hu_vertex_auth_free(&vauth);
        return HU_ERR_OUT_OF_MEMORY;
    }
    hu_json_object_set(alloc, inst, "prompt", pv);
    hu_json_array_push(alloc, instances, inst);
    hu_json_object_set(alloc, root, "instances", instances);

    /* storageUri is required — Veo writes output video to this GCS bucket.
     * The bucket must exist; Veo does not create it.
     * Fallback chain: config -> env -> gs://{project}-human-media/veo/ */
    const char *storage_uri = NULL;
    if (mv_agent && mv_agent->config)
        storage_uri = mv_agent->config->media_gen.veo_storage_uri;
    if (!storage_uri || !storage_uri[0])
        storage_uri = getenv("HU_VEO_STORAGE_URI");
    char storage_buf[256] = {0};
    if (!storage_uri || !storage_uri[0]) {
        int sn = snprintf(storage_buf, sizeof(storage_buf),
                          "gs://%s-human-media/veo/", project);
        if (sn > 0 && (size_t)sn < sizeof(storage_buf))
            storage_uri = storage_buf;
    }
    if (!storage_uri || !storage_uri[0]) {
        hu_json_free(alloc, root);
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("veo_storage_uri not configured (set in config or HU_VEO_STORAGE_URI)", 69);
        return HU_OK;
    }

    hu_json_value_t *params = hu_json_object_new(alloc);
    if (params) {
        hu_json_value_t *su = hu_json_string_new(alloc, storage_uri, strlen(storage_uri));
        if (su) hu_json_object_set(alloc, params, "storageUri", su);
        hu_json_value_t *ar = hu_json_string_new(alloc, aspect, strlen(aspect));
        if (ar) hu_json_object_set(alloc, params, "aspectRatio", ar);
        hu_json_value_t *sc = hu_json_number_new(alloc, 1.0);
        if (sc) hu_json_object_set(alloc, params, "sampleCount", sc);
        hu_json_value_t *dur = hu_json_number_new(alloc, duration);
        if (dur) hu_json_object_set(alloc, params, "durationSeconds", dur);
        hu_json_object_set(alloc, root, "parameters", params);
    }

    char *body = NULL;
    size_t body_len = 0;
    err = hu_json_stringify(alloc, root, &body, &body_len);
    hu_json_free(alloc, root);
    if (err != HU_OK || !body) {
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("failed to build request", 23);
        return HU_OK;
    }

    char auth_buf[1024];
    hu_vertex_auth_get_bearer(&vauth, auth_buf, sizeof(auth_buf));

    hu_http_response_t resp = {0};
    err = hu_http_post_json(alloc, url, auth_buf, body, body_len, &resp);
    alloc->free(alloc->ctx, body, body_len + 1);
    if (err != HU_OK) {
        hu_http_response_free(alloc, &resp);
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("Veo API submit failed", 21);
        return HU_OK;
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        hu_http_response_free(alloc, &resp);
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("Veo API submit error", 20);
        return HU_OK;
    }

    /* Extract operation name from response */
    hu_json_value_t *submit_json = NULL;
    err = hu_json_parse(alloc, resp.body, resp.body_len, &submit_json);
    hu_http_response_free(alloc, &resp);
    if (err != HU_OK || !submit_json) {
        hu_json_free(alloc, submit_json);
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("failed to parse submit response", 31);
        return HU_OK;
    }

    const char *op_name = hu_json_get_string(submit_json, "name");
    if (!op_name || !op_name[0]) {
        hu_json_free(alloc, submit_json);
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("no operation name in response", 29);
        return HU_OK;
    }
    char op_name_buf[512];
    snprintf(op_name_buf, sizeof(op_name_buf), "%s", op_name);
    hu_json_free(alloc, submit_json);

    /* Step 2: Poll fetchPredictOperation until done */
    char poll_url[512];
    int pul = snprintf(poll_url, sizeof(poll_url),
                       "https://%s-aiplatform.googleapis.com/v1/projects/%s/locations/%s/"
                       "publishers/google/models/%s:fetchPredictOperation",
                       region, project, region, mid);
    if (pul <= 0 || (size_t)pul >= sizeof(poll_url)) {
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("poll URL too long", 17);
        return HU_OK;
    }

    const char *gcs_uri = NULL;
    char gcs_uri_buf[1024] = {0};

    for (int attempt = 0; attempt < MV_POLL_MAX_ATTEMPTS; attempt++) {
        mv_sleep_secs(MV_POLL_INTERVAL_SEC);

        err = hu_vertex_auth_ensure_token(&vauth, alloc);
        if (err != HU_OK) break;
        hu_vertex_auth_get_bearer(&vauth, auth_buf, sizeof(auth_buf));

        /* Build poll request */
        hu_json_value_t *poll_root = hu_json_object_new(alloc);
        hu_json_value_t *on_val = hu_json_string_new(alloc, op_name_buf, strlen(op_name_buf));
        if (poll_root && on_val) {
            hu_json_object_set(alloc, poll_root, "operationName", on_val);
        }
        char *poll_body = NULL;
        size_t poll_body_len = 0;
        hu_json_stringify(alloc, poll_root, &poll_body, &poll_body_len);
        hu_json_free(alloc, poll_root);
        if (!poll_body) continue;

        hu_http_response_t poll_resp = {0};
        err = hu_http_post_json(alloc, poll_url, auth_buf, poll_body, poll_body_len, &poll_resp);
        alloc->free(alloc->ctx, poll_body, poll_body_len + 1);
        if (err != HU_OK) {
            hu_http_response_free(alloc, &poll_resp);
            continue;
        }

        hu_json_value_t *poll_json = NULL;
        err = hu_json_parse(alloc, poll_resp.body, poll_resp.body_len, &poll_json);
        hu_http_response_free(alloc, &poll_resp);
        if (err != HU_OK || !poll_json) {
            hu_json_free(alloc, poll_json);
            continue;
        }

        bool done = hu_json_get_bool(poll_json, "done", false);
        if (done) {
            /* Extract GCS URI: response.videos[0].gcsUri */
            const hu_json_value_t *response = hu_json_object_get(poll_json, "response");
            if (response && response->type == HU_JSON_OBJECT) {
                const hu_json_value_t *videos = hu_json_object_get(response, "videos");
                if (videos && videos->type == HU_JSON_ARRAY && videos->data.array.len > 0) {
                    const hu_json_value_t *v0 = videos->data.array.items[0];
                    if (v0 && v0->type == HU_JSON_OBJECT) {
                        gcs_uri = hu_json_get_string(v0, "gcsUri");
                        if (gcs_uri && gcs_uri[0])
                            snprintf(gcs_uri_buf, sizeof(gcs_uri_buf), "%s", gcs_uri);
                    }
                }
            }
            hu_json_free(alloc, poll_json);
            break;
        }
        hu_json_free(alloc, poll_json);
    }

    if (!gcs_uri_buf[0]) {
        hu_vertex_auth_free(&vauth);
        *out = hu_tool_result_fail("video generation timed out or failed", 36);
        return HU_OK;
    }

    /* Step 3: Download the video from GCS */
    /* Convert gs://bucket/path to https://storage.googleapis.com/bucket/path */
    char dl_url[1024];
    if (strncmp(gcs_uri_buf, "gs://", 5) == 0) {
        int dl = snprintf(dl_url, sizeof(dl_url), "https://storage.googleapis.com/%s",
                          gcs_uri_buf + 5);
        if (dl <= 0 || (size_t)dl >= sizeof(dl_url)) {
            hu_vertex_auth_free(&vauth);
            *out = hu_tool_result_fail("download URL too long", 21);
            return HU_OK;
        }
    } else {
        snprintf(dl_url, sizeof(dl_url), "%s", gcs_uri_buf);
    }

    hu_vertex_auth_get_bearer(&vauth, auth_buf, sizeof(auth_buf));
    hu_vertex_auth_free(&vauth);

    hu_http_response_t dl_resp = {0};
    err = hu_http_get(alloc, dl_url, auth_buf, &dl_resp);
    if (err != HU_OK || dl_resp.status_code < 200 || dl_resp.status_code >= 300) {
        hu_http_response_free(alloc, &dl_resp);
        *out = hu_tool_result_fail("failed to download video", 24);
        return HU_OK;
    }

    /* Write to temp file */
    char path_buf[256];
    int pn = snprintf(path_buf, sizeof(path_buf), "/tmp/human_vid_%lx.mp4",
                      (unsigned long)time(NULL));
    if (pn <= 0 || (size_t)pn >= sizeof(path_buf)) {
        hu_http_response_free(alloc, &dl_resp);
        *out = hu_tool_result_fail("path overflow", 13);
        return HU_OK;
    }
    FILE *f = fopen(path_buf, "wb");
    if (!f) {
        hu_http_response_free(alloc, &dl_resp);
        *out = hu_tool_result_fail("failed to write video file", 26);
        return HU_OK;
    }
    fwrite(dl_resp.body, 1, dl_resp.body_len, f);
    fclose(f);
    hu_http_response_free(alloc, &dl_resp);

    size_t pl = strlen(path_buf);
    char *path_copy = hu_strndup(alloc, path_buf, pl);
    char *desc = hu_sprintf(alloc, "Generated video saved to %s", path_buf);
    if (!path_copy || !desc) {
        if (path_copy) alloc->free(alloc->ctx, path_copy, pl + 1);
        if (desc) alloc->free(alloc->ctx, desc, strlen(desc) + 1);
        *out = hu_tool_result_fail("out of memory", 13);
        return HU_ERR_OUT_OF_MEMORY;
    }
    *out = hu_tool_result_ok_with_media(desc, strlen(desc), path_copy, pl);
    return HU_OK;
#endif
}

static const hu_tool_vtable_t media_video_vtable = {
    .execute = mv_execute,
    .name = mv_name,
    .description = mv_desc,
    .parameters_json = mv_params,
    .deinit = NULL,
};

hu_error_t hu_media_video_create(hu_allocator_t *alloc, hu_tool_t *out) {
    (void)alloc;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    out->vtable = &media_video_vtable;
    out->ctx = NULL;
    return HU_OK;
}
