#include "human/agent/acp_bridge.h"
#include "human/core/string.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

hu_msg_priority_t hu_acp_bridge_map_priority(hu_acp_priority_t acp_prio) {
    switch (acp_prio) {
    case HU_ACP_PRIORITY_LOW:
        return HU_MSG_PRIO_LOW;
    case HU_ACP_PRIORITY_NORMAL:
        return HU_MSG_PRIO_NORMAL;
    case HU_ACP_PRIORITY_HIGH:
    case HU_ACP_PRIORITY_URGENT:
        return HU_MSG_PRIO_HIGH;
    default:
        return HU_MSG_PRIO_NORMAL;
    }
}

hu_msg_type_t hu_acp_bridge_map_type(hu_acp_msg_type_t acp_type) {
    switch (acp_type) {
    case HU_ACP_REQUEST:
        return HU_MSG_TASK;
    case HU_ACP_RESPONSE:
        return HU_MSG_RESULT;
    case HU_ACP_DELEGATE:
        return HU_MSG_TASK;
    case HU_ACP_BROADCAST:
        return HU_MSG_BROADCAST;
    case HU_ACP_STATUS_UPDATE:
        return HU_MSG_PROGRESS;
    case HU_ACP_CANCEL:
        return HU_MSG_CANCEL;
    default:
        return HU_MSG_TASK;
    }
}

hu_error_t hu_acp_bridge_send(hu_allocator_t *alloc, hu_mailbox_t *mbox,
                              const hu_acp_message_t *msg, uint64_t from_agent, uint64_t to_agent) {
    if (!alloc || !mbox || !msg)
        return HU_ERR_INVALID_ARGUMENT;

    /* Envelope: "ACP|<type>|<correlation_id>|<payload>" */
    const char *type_name = hu_acp_msg_type_name(msg->type);
    const char *corr = msg->correlation_id ? msg->correlation_id : "";
    const char *payload = msg->payload ? msg->payload : "";

    size_t env_cap = 8 + strlen(type_name) + strlen(corr) + msg->payload_len + 4;
    char *envelope = (char *)alloc->alloc(alloc->ctx, env_cap);
    if (!envelope)
        return HU_ERR_OUT_OF_MEMORY;

    int n = snprintf(envelope, env_cap, "ACP|%s|%s|%s", type_name, corr, payload);
    if (n < 0 || (size_t)n >= env_cap) {
        alloc->free(alloc->ctx, envelope, env_cap);
        return HU_ERR_OUT_OF_MEMORY;
    }

    hu_msg_priority_t prio = hu_acp_bridge_map_priority(msg->priority);
    hu_msg_type_t mtype = hu_acp_bridge_map_type(msg->type);

    uint32_t ttl = 0;
    if (msg->deadline > 0) {
        int64_t now = (int64_t)time(NULL);
        if (msg->deadline > now)
            ttl = (uint32_t)(msg->deadline - now);
    }

    hu_error_t err =
        hu_mailbox_send_ex(mbox, from_agent, to_agent, mtype, envelope, (size_t)n, 0, prio, ttl);
    alloc->free(alloc->ctx, envelope, env_cap);
    return err;
}

hu_error_t hu_acp_bridge_recv(hu_allocator_t *alloc, const hu_message_t *mailbox_msg,
                              hu_acp_message_t *out) {
    if (!alloc || !mailbox_msg || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));

    if (!mailbox_msg->payload || mailbox_msg->payload_len < 4)
        return HU_ERR_PARSE;

    /* Check ACP envelope prefix */
    if (memcmp(mailbox_msg->payload, "ACP|", 4) != 0)
        return HU_ERR_PARSE;

    const char *p = mailbox_msg->payload + 4;
    const char *end = mailbox_msg->payload + mailbox_msg->payload_len;

    /* Parse type */
    const char *sep1 = memchr(p, '|', (size_t)(end - p));
    if (!sep1)
        return HU_ERR_PARSE;

    char type_buf[32];
    size_t type_len = (size_t)(sep1 - p);
    if (type_len >= sizeof(type_buf))
        type_len = sizeof(type_buf) - 1;
    memcpy(type_buf, p, type_len);
    type_buf[type_len] = '\0';

    if (strcmp(type_buf, "request") == 0)
        out->type = HU_ACP_REQUEST;
    else if (strcmp(type_buf, "response") == 0)
        out->type = HU_ACP_RESPONSE;
    else if (strcmp(type_buf, "delegate") == 0)
        out->type = HU_ACP_DELEGATE;
    else if (strcmp(type_buf, "broadcast") == 0)
        out->type = HU_ACP_BROADCAST;
    else if (strcmp(type_buf, "status_update") == 0)
        out->type = HU_ACP_STATUS_UPDATE;
    else if (strcmp(type_buf, "cancel") == 0)
        out->type = HU_ACP_CANCEL;
    else
        out->type = HU_ACP_REQUEST;

    /* Parse correlation_id */
    p = sep1 + 1;
    const char *sep2 = memchr(p, '|', (size_t)(end - p));
    if (!sep2)
        return HU_ERR_PARSE;
    size_t corr_len = (size_t)(sep2 - p);
    if (corr_len > 0) {
        out->correlation_id = hu_strndup(alloc, p, corr_len);
        out->correlation_id_len = corr_len;
    }

    /* Remaining is payload */
    p = sep2 + 1;
    size_t payload_len = (size_t)(end - p);
    if (payload_len > 0) {
        out->payload = hu_strndup(alloc, p, payload_len);
        out->payload_len = payload_len;
    }

    /* Map sender from mailbox message */
    char sender_buf[24];
    int sn = snprintf(sender_buf, sizeof(sender_buf), "%llu",
                      (unsigned long long)mailbox_msg->from_agent);
    if (sn > 0) {
        out->sender_id = hu_strndup(alloc, sender_buf, (size_t)sn);
        out->sender_id_len = (size_t)sn;
    }

    out->timestamp = (int64_t)mailbox_msg->timestamp;
    return HU_OK;
}
