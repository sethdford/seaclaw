#include "human/a2a.h"
#include <string.h>
#include <stdio.h>

#ifndef HU_IS_TEST
#define HU_IS_TEST 0
#endif

static char *a2a_dup(hu_allocator_t *alloc, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *d = alloc->alloc(alloc->ctx, len + 1);
    if (d) { memcpy(d, s, len); d[len] = 0; }
    return d;
}

hu_error_t hu_a2a_discover(hu_allocator_t *alloc, const char *agent_url, hu_a2a_agent_card_t *out) {
    if (!alloc || !agent_url || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (HU_IS_TEST) {
        out->name = a2a_dup(alloc, "test-agent");
        out->description = a2a_dup(alloc, "A test agent");
        out->url = a2a_dup(alloc, agent_url);
        out->version = a2a_dup(alloc, "1.0.0");
        return HU_OK;
    }
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_a2a_send_task(hu_allocator_t *alloc, const char *agent_url, const hu_a2a_message_t *message, hu_a2a_task_t *out) {
    if (!alloc || !agent_url || !message || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (HU_IS_TEST) {
        out->id = a2a_dup(alloc, "task-001");
        out->state = HU_A2A_TASK_SUBMITTED;
        return HU_OK;
    }
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_a2a_get_task(hu_allocator_t *alloc, const char *agent_url, const char *task_id, hu_a2a_task_t *out) {
    if (!alloc || !agent_url || !task_id || !out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (HU_IS_TEST) {
        out->id = a2a_dup(alloc, task_id);
        out->state = HU_A2A_TASK_COMPLETED;
        return HU_OK;
    }
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_a2a_cancel_task(hu_allocator_t *alloc, const char *agent_url, const char *task_id) {
    if (!alloc || !agent_url || !task_id) return HU_ERR_INVALID_ARGUMENT;
    if (HU_IS_TEST) return HU_OK;
    return HU_ERR_NOT_SUPPORTED;
}

hu_error_t hu_a2a_server_init(hu_allocator_t *alloc, const hu_a2a_agent_card_t *card) {
    if (!alloc || !card) return HU_ERR_INVALID_ARGUMENT;
    return HU_OK;
}

hu_error_t hu_a2a_server_handle_request(hu_allocator_t *alloc, const char *method, const char *body, size_t body_len, char **response, size_t *response_len) {
    if (!alloc || !method || !response || !response_len) return HU_ERR_INVALID_ARGUMENT;
    const char *resp;
    if (strcmp(method, "GET") == 0) {
        resp = "{\"name\":\"human\",\"version\":\"1.0\"}";
    } else if (strcmp(method, "POST") == 0 && body && body_len > 0) {
        resp = "{\"jsonrpc\":\"2.0\",\"result\":{\"id\":\"task-001\",\"state\":\"completed\"}}";
    } else {
        return HU_ERR_INVALID_ARGUMENT;
    }
    size_t len = strlen(resp);
    *response = alloc->alloc(alloc->ctx, len + 1);
    if (!*response) return HU_ERR_OUT_OF_MEMORY;
    memcpy(*response, resp, len + 1);
    *response_len = len;
    return HU_OK;
}

void hu_a2a_server_deinit(hu_allocator_t *alloc) { (void)alloc; }

void hu_a2a_agent_card_free(hu_allocator_t *alloc, hu_a2a_agent_card_t *card) {
    if (!alloc || !card) return;
    if (card->name) { alloc->free(alloc->ctx, card->name, strlen(card->name)+1); card->name = NULL; }
    if (card->description) { alloc->free(alloc->ctx, card->description, strlen(card->description)+1); card->description = NULL; }
    if (card->url) { alloc->free(alloc->ctx, card->url, strlen(card->url)+1); card->url = NULL; }
    if (card->version) { alloc->free(alloc->ctx, card->version, strlen(card->version)+1); card->version = NULL; }
}

void hu_a2a_task_free(hu_allocator_t *alloc, hu_a2a_task_t *task) {
    if (!alloc || !task) return;
    if (task->id) { alloc->free(alloc->ctx, task->id, strlen(task->id)+1); task->id = NULL; }
}

void hu_a2a_message_free(hu_allocator_t *alloc, hu_a2a_message_t *msg) { (void)alloc; (void)msg; }
