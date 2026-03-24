#include "human/agent/agent_comm.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static void gen_msg_id(char *buf, size_t cap) {
    static uint32_t seq = 0;
    seq++;
    snprintf(buf, cap, "acp-%08x-%04x", (uint32_t)time(NULL), (uint16_t)(seq & 0xFFFF));
}

hu_error_t hu_acp_inbox_init(hu_acp_inbox_t *inbox, hu_allocator_t *alloc, size_t initial_cap) {
    if (!inbox || !alloc)
        return HU_ERR_INVALID_ARGUMENT;
    memset(inbox, 0, sizeof(*inbox));
    inbox->alloc = alloc;
    if (initial_cap == 0)
        initial_cap = 16;
    inbox->messages =
        (hu_acp_message_t *)alloc->alloc(alloc->ctx, initial_cap * sizeof(hu_acp_message_t));
    if (!inbox->messages)
        return HU_ERR_OUT_OF_MEMORY;
    memset(inbox->messages, 0, initial_cap * sizeof(hu_acp_message_t));
    inbox->capacity = initial_cap;
    return HU_OK;
}

void hu_acp_inbox_deinit(hu_acp_inbox_t *inbox) {
    if (!inbox)
        return;
    for (size_t i = 0; i < inbox->count; i++)
        hu_acp_message_free(inbox->alloc, &inbox->messages[i]);
    if (inbox->messages)
        inbox->alloc->free(inbox->alloc->ctx, inbox->messages,
                           inbox->capacity * sizeof(hu_acp_message_t));
    memset(inbox, 0, sizeof(*inbox));
}

hu_error_t hu_acp_message_create(hu_allocator_t *alloc, hu_acp_msg_type_t type,
                                 const char *sender_id, size_t sender_len, const char *receiver_id,
                                 size_t receiver_len, const char *payload, size_t payload_len,
                                 hu_acp_message_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    char id_buf[32];
    gen_msg_id(id_buf, sizeof(id_buf));
    out->id = hu_strndup(alloc, id_buf, strlen(id_buf));
    out->id_len = strlen(id_buf);
    out->type = type;
    out->priority = HU_ACP_PRIORITY_NORMAL;
    out->timestamp = (int64_t)time(NULL);

    if (sender_id && sender_len > 0) {
        out->sender_id = hu_strndup(alloc, sender_id, sender_len);
        out->sender_id_len = sender_len;
    }
    if (receiver_id && receiver_len > 0) {
        out->receiver_id = hu_strndup(alloc, receiver_id, receiver_len);
        out->receiver_id_len = receiver_len;
    }
    if (payload && payload_len > 0) {
        out->payload = hu_strndup(alloc, payload, payload_len);
        out->payload_len = payload_len;
    }
    return HU_OK;
}

hu_error_t hu_acp_message_reply(hu_allocator_t *alloc, const hu_acp_message_t *request,
                                const char *payload, size_t payload_len, hu_acp_message_t *out) {
    if (!alloc || !request || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_error_t err = hu_acp_message_create(alloc, HU_ACP_RESPONSE, request->receiver_id,
                                           request->receiver_id_len, request->sender_id,
                                           request->sender_id_len, payload, payload_len, out);
    if (err != HU_OK)
        return err;

    if (request->id && request->id_len > 0) {
        out->correlation_id = hu_strndup(alloc, request->id, request->id_len);
        out->correlation_id_len = request->id_len;
    }
    return HU_OK;
}

hu_error_t hu_acp_inbox_push(hu_acp_inbox_t *inbox, const hu_acp_message_t *msg) {
    if (!inbox || !msg)
        return HU_ERR_INVALID_ARGUMENT;

    if (inbox->count >= inbox->capacity) {
        size_t new_cap = inbox->capacity * 2;
        hu_acp_message_t *nm = (hu_acp_message_t *)inbox->alloc->alloc(
            inbox->alloc->ctx, new_cap * sizeof(hu_acp_message_t));
        if (!nm)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(nm, inbox->messages, inbox->count * sizeof(hu_acp_message_t));
        memset(nm + inbox->count, 0, (new_cap - inbox->count) * sizeof(hu_acp_message_t));
        inbox->alloc->free(inbox->alloc->ctx, inbox->messages,
                           inbox->capacity * sizeof(hu_acp_message_t));
        inbox->messages = nm;
        inbox->capacity = new_cap;
    }

    inbox->messages[inbox->count] = *msg;
    inbox->count++;
    return HU_OK;
}

hu_error_t hu_acp_inbox_pop(hu_acp_inbox_t *inbox, hu_acp_message_t *out) {
    if (!inbox || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (inbox->count == 0)
        return HU_ERR_NOT_FOUND;

    /* Find highest priority message */
    size_t best = 0;
    for (size_t i = 1; i < inbox->count; i++) {
        if (inbox->messages[i].priority > inbox->messages[best].priority)
            best = i;
        else if (inbox->messages[i].priority == inbox->messages[best].priority &&
                 inbox->messages[i].timestamp < inbox->messages[best].timestamp)
            best = i;
    }

    *out = inbox->messages[best];
    /* Shift remaining */
    for (size_t i = best; i + 1 < inbox->count; i++)
        inbox->messages[i] = inbox->messages[i + 1];
    inbox->count--;
    memset(&inbox->messages[inbox->count], 0, sizeof(hu_acp_message_t));
    return HU_OK;
}

size_t hu_acp_inbox_count(const hu_acp_inbox_t *inbox, int type_filter) {
    if (!inbox)
        return 0;
    if (type_filter < 0)
        return inbox->count;
    size_t n = 0;
    for (size_t i = 0; i < inbox->count; i++) {
        if ((int)inbox->messages[i].type == type_filter)
            n++;
    }
    return n;
}

void hu_acp_message_free(hu_allocator_t *alloc, hu_acp_message_t *msg) {
    if (!alloc || !msg)
        return;
    if (msg->id)
        hu_str_free(alloc, msg->id);
    if (msg->correlation_id)
        hu_str_free(alloc, msg->correlation_id);
    if (msg->sender_id)
        hu_str_free(alloc, msg->sender_id);
    if (msg->receiver_id)
        hu_str_free(alloc, msg->receiver_id);
    if (msg->payload)
        hu_str_free(alloc, msg->payload);
    memset(msg, 0, sizeof(*msg));
}

static const char *const MSG_TYPE_NAMES[] = {
    "request", "response", "delegate", "broadcast", "status_update", "cancel",
};

const char *hu_acp_msg_type_name(hu_acp_msg_type_t type) {
    if (type < 0 || type > HU_ACP_CANCEL)
        return "unknown";
    return MSG_TYPE_NAMES[type];
}
