#include "human/eval.h"
#include "human/core/string.h"
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EVAL_MAX_TASKS 256

static int tolower_c(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

static bool str_case_contains(const char *haystack, size_t hlen, const char *needle, size_t nlen) {
    if (nlen == 0 || nlen > hlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        size_t j = 0;
        while (j < nlen && tolower_c((unsigned char)haystack[i + j]) == tolower_c((unsigned char)needle[j]))
            j++;
        if (j == nlen) return true;
    }
    return false;
}

static size_t count_expected_words_in_actual(const char *actual, size_t actual_len, const char *expected, size_t expected_len) {
    size_t count = 0;
    size_t total_expected = 0;
    const char *p = expected;
    const char *end = expected + expected_len;
    while (p < end) {
        while (p < end && (isspace((unsigned char)*p) || *p == '\0')) p++;
        if (p >= end) break;
        const char *word_start = p;
        while (p < end && !isspace((unsigned char)*p) && *p != '\0') p++;
        size_t word_len = (size_t)(p - word_start);
        if (word_len > 0) {
            total_expected++;
            if (str_case_contains(actual, actual_len, word_start, word_len))
                count++;
        }
    }
    (void)total_expected;
    return count;
}

static size_t count_words(const char *s, size_t len) {
    size_t n = 0;
    const char *p = s;
    const char *end = s + len;
    while (p < end) {
        while (p < end && (isspace((unsigned char)*p) || *p == '\0')) p++;
        if (p >= end) break;
        n++;
        while (p < end && !isspace((unsigned char)*p) && *p != '\0') p++;
    }
    return n;
}

static char *extract_str(hu_allocator_t *alloc, const char *obj_start, const char *obj_end, const char *key) {
    const char *k = strstr(obj_start, key);
    if (!k || k >= obj_end) return NULL;
    const char *colon = strchr(k + strlen(key), ':');
    if (!colon || colon >= obj_end) return NULL;
    const char *vs = colon + 1;
    while (vs < obj_end && (*vs == ' ' || *vs == '\t')) vs++;
    if (vs >= obj_end || *vs != '"') return NULL;
    vs++;
    const char *ve = vs;
    while (ve < obj_end && *ve != '"') {
        if (*ve == '\\') ve++;
        ve++;
    }
    if (ve >= obj_end) return NULL;
    size_t n = (size_t)(ve - vs);
    char *out = alloc->alloc(alloc->ctx, n + 1);
    if (!out) return NULL;
    memcpy(out, vs, n);
    out[n] = 0;
    return out;
}

static int extract_int(const char *obj_start, const char *obj_end, const char *key) {
    const char *k = strstr(obj_start, key);
    if (!k || k >= obj_end) return 0;
    const char *colon = strchr(k + strlen(key), ':');
    if (!colon || colon >= obj_end) return 0;
    const char *vs = colon + 1;
    while (vs < obj_end && (*vs == ' ' || *vs == '\t')) vs++;
    int v = 0;
    (void)sscanf(vs, "%d", &v);
    return v;
}

static int64_t extract_int64(const char *obj_start, const char *obj_end, const char *key) {
    const char *k = strstr(obj_start, key);
    if (!k || k >= obj_end) return 0;
    const char *colon = strchr(k + strlen(key), ':');
    if (!colon || colon >= obj_end) return 0;
    const char *vs = colon + 1;
    while (vs < obj_end && (*vs == ' ' || *vs == '\t')) vs++;
    int64_t v = 0;
    (void)sscanf(vs, "%" SCNd64, &v);
    return v;
}

hu_error_t hu_eval_suite_load_json(hu_allocator_t *alloc, const char *json, size_t json_len, hu_eval_suite_t *out) {
    if (!alloc || !json || !json_len || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    (void)json_len;
    const char *name_key = "\"name\"";
    const char *found = strstr(json, name_key);
    if (found) {
        const char *vs = strchr(found + strlen(name_key), '"');
        if (vs) { vs++;
            const char *ve = strchr(vs, '"');
            if (ve) {
                size_t nlen = (size_t)(ve - vs);
                out->name = alloc->alloc(alloc->ctx, nlen + 1);
                if (out->name) { memcpy(out->name, vs, nlen); out->name[nlen] = 0; }
            }
        }
    }
    if (!out->name) { out->name = alloc->alloc(alloc->ctx, 5); if (out->name) memcpy(out->name, "eval", 5); }

    const char *tasks_key = "\"tasks\"";
    const char *tasks_start = strstr(json, tasks_key);
    if (tasks_start) {
        const char *arr_start = strchr(tasks_start + strlen(tasks_key), '[');
        if (arr_start) {
            const char *p = arr_start + 1;
            hu_eval_task_t *tasks = alloc->alloc(alloc->ctx, EVAL_MAX_TASKS * sizeof(hu_eval_task_t));
            if (!tasks) return HU_ERR_OUT_OF_MEMORY;
            memset(tasks, 0, EVAL_MAX_TASKS * sizeof(hu_eval_task_t));
            size_t count = 0;

            while (count < EVAL_MAX_TASKS) {
                while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',') p++;
                if (*p == ']') break;
                if (*p != '{') break;

                const char *obj_start = p;
                int brace = 1;
                p++;
                while (*p && brace > 0) {
                    if (*p == '"') {
                        p++;
                        while (*p && *p != '"') {
                            if (*p == '\\') p++;
                            p++;
                        }
                        if (*p) p++;
                    } else if (*p == '{') { brace++; p++; }
                    else if (*p == '}') { brace--; p++; }
                    else p++;
                }
                const char *obj_end = p;
                if (brace != 0) break;

                hu_eval_task_t *t = &tasks[count];
                t->id = extract_str(alloc, obj_start, obj_end, "\"id\"");
                t->prompt = extract_str(alloc, obj_start, obj_end, "\"prompt\"");
                if (t->prompt) t->prompt_len = strlen(t->prompt);
                t->expected = extract_str(alloc, obj_start, obj_end, "\"expected\"");
                if (t->expected) t->expected_len = strlen(t->expected);
                t->category = extract_str(alloc, obj_start, obj_end, "\"category\"");
                t->difficulty = extract_int(obj_start, obj_end, "\"difficulty\"");
                t->timeout_ms = extract_int64(obj_start, obj_end, "\"timeout_ms\"");
                if (t->timeout_ms == 0) t->timeout_ms = 5000;

                count++;
            }

            out->tasks = tasks;
            out->tasks_count = count;
        }
    }
    return HU_OK;
}

hu_error_t hu_eval_run_suite(hu_allocator_t *alloc, hu_provider_t *provider, const char *model, size_t model_len, hu_eval_suite_t *suite, hu_eval_match_mode_t mode, hu_eval_run_t *out) {
    if (!alloc || !suite || !out)
        return HU_ERR_INVALID_ARGUMENT;
#if defined(HU_IS_TEST) && HU_IS_TEST
    (void)provider;
#else
    if (!provider || !provider->vtable || !provider->vtable->chat_with_system || !provider->vtable->get_name)
        return HU_ERR_INVALID_ARGUMENT;
#endif
    memset(out, 0, sizeof(*out));
    if (suite->tasks_count == 0) {
        out->suite_name = suite->name ? hu_strdup(alloc, suite->name) : hu_strndup(alloc, "eval", 4);
        out->provider = hu_strndup(alloc, "test", 4);
        out->model = model && model_len > 0 ? hu_strndup(alloc, model, model_len) : NULL;
        out->results = NULL;
        out->results_count = 0;
        out->passed = 0;
        out->failed = 0;
        out->pass_rate = 1.0;
        return HU_OK;
    }
    out->results = alloc->alloc(alloc->ctx, suite->tasks_count * sizeof(hu_eval_result_t));
    if (!out->results)
        return HU_ERR_OUT_OF_MEMORY;
    memset(out->results, 0, suite->tasks_count * sizeof(hu_eval_result_t));
    out->results_count = suite->tasks_count;

    for (size_t i = 0; i < suite->tasks_count; i++) {
        hu_eval_task_t *task = &suite->tasks[i];
        hu_eval_result_t *res = &out->results[i];
        char *response = NULL;
        size_t response_len = 0;

#if defined(HU_IS_TEST) && HU_IS_TEST
        {
            const char *prefix = "Mock response for: ";
            size_t plen = strlen(prefix);
            size_t prompt_len = task->prompt ? strlen(task->prompt) : 0;
            size_t total = plen + prompt_len + 1;
            response = alloc->alloc(alloc->ctx, total);
            if (response) {
                memcpy(response, prefix, plen);
                if (task->prompt)
                    memcpy(response + plen, task->prompt, prompt_len + 1);
                else
                    response[plen] = '\0';
                response_len = plen + prompt_len;
            }
        }
#else
        {
            hu_error_t err = provider->vtable->chat_with_system(provider->ctx, alloc, NULL, 0,
                task->prompt ? task->prompt : "", task->prompt ? task->prompt_len : 0,
                model ? model : "", model_len, 0.0, &response, &response_len);
            if (err != HU_OK) {
                res->task_id = task->id ? hu_strdup(alloc, task->id) : NULL;
                res->passed = false;
                res->actual_output = NULL;
                res->actual_output_len = 0;
                res->elapsed_ms = 0;
                res->tool_calls_made = 0;
                res->tokens_used = 0;
                res->error_msg = hu_sprintf(alloc, "provider error: %d", (int)err);
                if (response)
                    alloc->free(alloc->ctx, response, response_len + 1);
                out->failed++;
                continue;
            }
        }
#endif

        bool passed = false;
        hu_eval_check(alloc, response ? response : "", response_len, task->expected ? task->expected : "", task->expected ? task->expected_len : 0, mode, &passed);

        res->task_id = task->id ? hu_strdup(alloc, task->id) : NULL;
        res->passed = passed;
        res->actual_output = response;
        res->actual_output_len = response_len;
        res->elapsed_ms = 0;
        res->tool_calls_made = 0;
        res->tokens_used = 0;
        res->error_msg = NULL;

        if (passed)
            out->passed++;
        else
            out->failed++;
    }

    out->pass_rate = (out->results_count > 0) ? (double)out->passed / (double)out->results_count : 1.0;

    out->suite_name = suite->name ? hu_strdup(alloc, suite->name) : hu_strndup(alloc, "eval", 4);
#if defined(HU_IS_TEST) && HU_IS_TEST
    out->provider = hu_strndup(alloc, "test", 4);
#else
    {
        const char *pname = provider->vtable->get_name(provider->ctx);
        out->provider = pname ? hu_strdup(alloc, pname) : hu_strndup(alloc, "unknown", 7);
    }
#endif
    out->model = (model && model_len > 0) ? hu_strndup(alloc, model, model_len) : NULL;

    return HU_OK;
}

hu_error_t hu_eval_run_load_json(hu_allocator_t *alloc, const char *json, size_t json_len,
                                 hu_eval_run_t *out) {
    if (!alloc || !json || !json_len || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    /* Parse report format: {"suite":"...","passed":N,"failed":N,"pass_rate":X.XX,"elapsed_ms":N} */
    const char *p = json;
    const char *end = json + json_len;
    while (p < end) {
        if (strncmp(p, "\"suite\"", 7) == 0) {
            p = strchr(p + 7, '"');
            if (p) {
                p++;
                const char *ve = strchr(p, '"');
                if (ve && ve - p > 0) {
                    size_t n = (size_t)(ve - p);
                    out->suite_name = alloc->alloc(alloc->ctx, n + 1);
                    if (out->suite_name) {
                        memcpy(out->suite_name, p, n);
                        out->suite_name[n] = '\0';
                    }
                }
            }
            break;
        }
        p++;
    }
    p = json;
    if (strstr(json, "\"passed\"")) {
        const char *k = strstr(json, "\"passed\"");
        if (k) {
            const char *v = strchr(k + 8, ':');
            if (v)
                (void)sscanf(v + 1, "%zu", &out->passed);
        }
    }
    if (strstr(json, "\"failed\"")) {
        const char *k = strstr(json, "\"failed\"");
        if (k) {
            const char *v = strchr(k + 8, ':');
            if (v)
                (void)sscanf(v + 1, "%zu", &out->failed);
        }
    }
    if (strstr(json, "\"pass_rate\"")) {
        const char *k = strstr(json, "\"pass_rate\"");
        if (k) {
            const char *v = strchr(k + 11, ':');
            if (v)
                (void)sscanf(v + 1, "%lf", &out->pass_rate);
        }
    }
    if (strstr(json, "\"elapsed_ms\"")) {
        const char *k = strstr(json, "\"elapsed_ms\"");
        if (k) {
            const char *v = strchr(k + 12, ':');
            if (v)
                (void)sscanf(v + 1, "%" SCNd64, &out->total_elapsed_ms);
        }
    }
    return HU_OK;
}

hu_error_t hu_eval_check(hu_allocator_t *alloc, const char *actual, size_t actual_len, const char *expected, size_t expected_len, hu_eval_match_mode_t mode, bool *passed) {
    if (!alloc || !actual || !expected || !passed) return HU_ERR_INVALID_ARGUMENT;
    *passed = false;
    switch (mode) {
        case HU_EVAL_EXACT: *passed = (actual_len == expected_len && memcmp(actual, expected, actual_len) == 0); break;
        case HU_EVAL_CONTAINS:
            if (expected_len <= actual_len) { for (size_t i = 0; i <= actual_len - expected_len; i++) { if (memcmp(actual+i, expected, expected_len) == 0) { *passed = true; break; } } } break;
        case HU_EVAL_NUMERIC_CLOSE: { double a = atof(actual); double e = atof(expected); *passed = fabs(a - e) < 0.01; break; }
        case HU_EVAL_LLM_JUDGE: {
            /* Heuristic: case-insensitive contains OR word overlap >= 50% of expected words. */
            if (str_case_contains(actual, actual_len, expected, expected_len)) {
                *passed = true;
                break;
            }
            size_t exp_words = count_words(expected, expected_len);
            if (exp_words > 0) {
                size_t matched = count_expected_words_in_actual(actual, actual_len, expected, expected_len);
                *passed = (matched >= (exp_words + 1) / 2);
            }
            break;
        }
    }
    return HU_OK;
}

hu_error_t hu_eval_report_json(hu_allocator_t *alloc, const hu_eval_run_t *run, char **out, size_t *out_len) {
    if (!alloc || !run || !out || !out_len) return HU_ERR_INVALID_ARGUMENT;
    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "{\"suite\":\"%s\",\"passed\":%zu,\"failed\":%zu,\"pass_rate\":%.2f,\"elapsed_ms\":%" PRId64 "}",
        run->suite_name ? run->suite_name : "", run->passed, run->failed, run->pass_rate, run->total_elapsed_ms);
    if (n < 0) return HU_ERR_INVALID_ARGUMENT;
    *out = alloc->alloc(alloc->ctx, (size_t)n + 1);
    if (!*out) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*out, buf, (size_t)n + 1);
    *out_len = (size_t)n;
    return HU_OK;
}

hu_error_t hu_eval_compare(hu_allocator_t *alloc, const hu_eval_run_t *baseline, const hu_eval_run_t *current, char **report, size_t *report_len) {
    if (!alloc || !baseline || !current || !report || !report_len) return HU_ERR_INVALID_ARGUMENT;
    char buf[512];
    double delta = current->pass_rate - baseline->pass_rate;
    int n = snprintf(buf, sizeof(buf), "{\"baseline\":%.2f,\"current\":%.2f,\"delta\":%.2f}", baseline->pass_rate, current->pass_rate, delta);
    if (n < 0) return HU_ERR_INVALID_ARGUMENT;
    *report = alloc->alloc(alloc->ctx, (size_t)n + 1);
    if (!*report) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*report, buf, (size_t)n + 1);
    *report_len = (size_t)n;
    return HU_OK;
}

void hu_eval_suite_free(hu_allocator_t *alloc, hu_eval_suite_t *suite) {
    if (!alloc || !suite) return;
    if (suite->name) {
        alloc->free(alloc->ctx, suite->name, strlen(suite->name) + 1);
        suite->name = NULL;
    }
    if (suite->tasks) {
        for (size_t i = 0; i < suite->tasks_count; i++) {
            hu_eval_task_t *t = &suite->tasks[i];
            if (t->id) { alloc->free(alloc->ctx, t->id, strlen(t->id) + 1); t->id = NULL; }
            if (t->prompt) { alloc->free(alloc->ctx, t->prompt, strlen(t->prompt) + 1); t->prompt = NULL; }
            if (t->expected) { alloc->free(alloc->ctx, t->expected, strlen(t->expected) + 1); t->expected = NULL; }
            if (t->category) { alloc->free(alloc->ctx, t->category, strlen(t->category) + 1); t->category = NULL; }
        }
        alloc->free(alloc->ctx, suite->tasks, EVAL_MAX_TASKS * sizeof(hu_eval_task_t));
        suite->tasks = NULL;
        suite->tasks_count = 0;
    }
}

void hu_eval_result_free(hu_allocator_t *alloc, hu_eval_result_t *result) {
    if (!alloc || !result) return;
    if (result->task_id) {
        alloc->free(alloc->ctx, result->task_id, strlen(result->task_id) + 1);
        result->task_id = NULL;
    }
    if (result->actual_output) {
        alloc->free(alloc->ctx, result->actual_output, result->actual_output_len + 1);
        result->actual_output = NULL;
        result->actual_output_len = 0;
    }
    if (result->error_msg) {
        alloc->free(alloc->ctx, result->error_msg, strlen(result->error_msg) + 1);
        result->error_msg = NULL;
    }
}

void hu_eval_run_free(hu_allocator_t *alloc, hu_eval_run_t *run) {
    if (!alloc || !run) return;
    if (run->suite_name) {
        alloc->free(alloc->ctx, run->suite_name, strlen(run->suite_name) + 1);
        run->suite_name = NULL;
    }
    if (run->provider) {
        alloc->free(alloc->ctx, run->provider, strlen(run->provider) + 1);
        run->provider = NULL;
    }
    if (run->model) {
        alloc->free(alloc->ctx, run->model, strlen(run->model) + 1);
        run->model = NULL;
    }
    if (run->results) {
        for (size_t i = 0; i < run->results_count; i++)
            hu_eval_result_free(alloc, &run->results[i]);
        alloc->free(alloc->ctx, run->results, run->results_count * sizeof(hu_eval_result_t));
        run->results = NULL;
        run->results_count = 0;
    }
}
