#include "human/channel.h"
#include "human/channels/mqtt.h"
#include "human/channel_loop.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HU_GATEWAY_POSIX) && (!defined(HU_IS_TEST) || !HU_IS_TEST)
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define HU_MQTT_MAX_MSG        65536
#define HU_MQTT_LAST_MSG_LEN  4096
#define HU_MQTT_MOCK_MAX      8
#define HU_MQTT_DEFAULT_PORT  1883
#define HU_MQTT_POLL_TIMEOUT  5

typedef struct hu_mqtt_mock_msg {
    char session_key[128];
    char content[4096];
} hu_mqtt_mock_msg_t;

typedef struct hu_mqtt_ctx {
    hu_allocator_t *alloc;
    char *broker_url;
    char *inbound_topic;
    char *outbound_topic;
    char *username;
    char *password;
    int qos;
    bool running;
#if HU_IS_TEST
    char last_message[HU_MQTT_LAST_MSG_LEN];
    size_t last_message_len;
    size_t mock_count;
    hu_mqtt_mock_msg_t mock_msgs[HU_MQTT_MOCK_MAX];
#endif
} hu_mqtt_ctx_t;

#if defined(HU_GATEWAY_POSIX) && (!defined(HU_IS_TEST) || !HU_IS_TEST)
/* Parse broker_url (e.g. "mqtt://host:1883" or "host" or "host:1883") into host and port. */
static bool mqtt_parse_broker(const char *broker_url, char *host, size_t host_size,
                              uint16_t *port_out) {
    if (!broker_url || !host || host_size < 2 || !port_out)
        return false;
    *port_out = HU_MQTT_DEFAULT_PORT;
    const char *p = broker_url;
    if (strncmp(p, "mqtt://", 7) == 0)
        p += 7;
    const char *colon = strchr(p, ':');
    if (colon && colon - p < (ptrdiff_t)host_size) {
        size_t host_len = (size_t)(colon - p);
        memcpy(host, p, host_len);
        host[host_len] = '\0';
        unsigned long port_val = strtoul(colon + 1, NULL, 10);
        if (port_val >= 1 && port_val <= 65535)
            *port_out = (uint16_t)port_val;
    } else {
        size_t len = strlen(p);
        if (len >= host_size)
            return false;
        memcpy(host, p, len + 1);
    }
    return host[0] != '\0';
}

/* Encode remaining length into buf (1–4 bytes). Returns number of bytes written. */
static size_t mqtt_encode_remaining_length(uint8_t *buf, size_t len) {
    size_t n = 0;
    do {
        uint8_t b = (uint8_t)(len % 128);
        len /= 128;
        if (len > 0)
            b |= 0x80;
        buf[n++] = b;
    } while (len > 0);
    return n;
}

/* Build MQTT v3.1.1 CONNECT packet. Returns length written, or 0 on overflow. */
static size_t mqtt_build_connect(uint8_t *buf, size_t buf_size, const char *client_id,
                                 const char *username, const char *password) {
    const char *cid = client_id && client_id[0] ? client_id : "human";
    size_t cid_len = strlen(cid);
    size_t user_len = username ? strlen(username) : 0;
    size_t pass_len = password ? strlen(password) : 0;

    /* Variable header: "MQTT" (6) + version 4 + flags + keepalive 60 */
    uint8_t flags = 0x02; /* clean session */
    if (user_len > 0)
        flags |= 0x80;
    if (pass_len > 0)
        flags |= 0x40;

    size_t payload_len = 2 + cid_len;
    if (user_len > 0)
        payload_len += 2 + user_len;
    if (pass_len > 0)
        payload_len += 2 + pass_len;

    size_t var_len = 10 + payload_len; /* 6 + 1 + 1 + 2 + payload */
    uint8_t rl_buf[4];
    size_t rl_n = mqtt_encode_remaining_length(rl_buf, var_len);
    size_t total = 1 + rl_n + var_len;
    if (total > buf_size)
        return 0;

    size_t off = 0;
    buf[off++] = 0x10; /* CONNECT */
    memcpy(buf + off, rl_buf, rl_n);
    off += rl_n;
    buf[off++] = 0x00;
    buf[off++] = 0x04;
    buf[off++] = 'M';
    buf[off++] = 'Q';
    buf[off++] = 'T';
    buf[off++] = 'T';
    buf[off++] = 0x04; /* version 3.1.1 */
    buf[off++] = flags;
    buf[off++] = 0x00;
    buf[off++] = 0x3C; /* keepalive 60 */
    buf[off++] = (uint8_t)(cid_len >> 8);
    buf[off++] = (uint8_t)(cid_len & 0xFF);
    memcpy(buf + off, cid, cid_len);
    off += cid_len;
    if (user_len > 0) {
        buf[off++] = (uint8_t)(user_len >> 8);
        buf[off++] = (uint8_t)(user_len & 0xFF);
        memcpy(buf + off, username, user_len);
        off += user_len;
    }
    if (pass_len > 0) {
        buf[off++] = (uint8_t)(pass_len >> 8);
        buf[off++] = (uint8_t)(pass_len & 0xFF);
        memcpy(buf + off, password, pass_len);
        off += pass_len;
    }
    return off;
}

/* Build PUBLISH packet (QoS 0). Returns length or 0. */
static size_t mqtt_build_publish(uint8_t *buf, size_t buf_size, const char *topic,
                                 const void *payload, size_t payload_len) {
    size_t topic_len = strlen(topic);
    size_t var_len = 2 + topic_len + payload_len;
    uint8_t rl_buf[4];
    size_t rl_n = mqtt_encode_remaining_length(rl_buf, var_len);
    size_t total = 1 + rl_n + var_len;
    if (total > buf_size)
        return 0;

    size_t off = 0;
    buf[off++] = 0x30; /* PUBLISH QoS 0 */
    memcpy(buf + off, rl_buf, rl_n);
    off += rl_n;
    buf[off++] = (uint8_t)(topic_len >> 8);
    buf[off++] = (uint8_t)(topic_len & 0xFF);
    memcpy(buf + off, topic, topic_len);
    off += topic_len;
    if (payload && payload_len > 0) {
        memcpy(buf + off, payload, payload_len);
        off += payload_len;
    }
    return off;
}

/* Build SUBSCRIBE packet. Returns length or 0. */
static size_t mqtt_build_subscribe(uint8_t *buf, size_t buf_size, uint16_t packet_id,
                                    const char *topic, uint8_t qos) {
    size_t topic_len = strlen(topic);
    size_t payload_len = 2 + topic_len + 1;
    size_t var_len = 2 + payload_len;
    uint8_t rl_buf[4];
    size_t rl_n = mqtt_encode_remaining_length(rl_buf, var_len);
    size_t total = 1 + rl_n + var_len;
    if (total > buf_size)
        return 0;

    size_t off = 0;
    buf[off++] = 0x82; /* SUBSCRIBE */
    memcpy(buf + off, rl_buf, rl_n);
    off += rl_n;
    buf[off++] = (uint8_t)(packet_id >> 8);
    buf[off++] = (uint8_t)(packet_id & 0xFF);
    buf[off++] = (uint8_t)(topic_len >> 8);
    buf[off++] = (uint8_t)(topic_len & 0xFF);
    memcpy(buf + off, topic, topic_len);
    off += topic_len;
    buf[off++] = qos > 2 ? 0 : (uint8_t)qos;
    return off;
}

/* Build DISCONNECT packet. */
static size_t mqtt_build_disconnect(uint8_t *buf, size_t buf_size) {
    if (buf_size < 2)
        return 0;
    buf[0] = 0xE0;
    buf[1] = 0x00;
    return 2;
}

static int mqtt_tcp_connect(const char *host, uint16_t port) {
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port_str, &hints, &res) != 0)
        return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return -1;
    }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return fd;
}

static hu_error_t mqtt_send_all(int fd, const uint8_t *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0)
            return HU_ERR_IO;
        sent += (size_t)n;
    }
    return HU_OK;
}

static hu_error_t mqtt_recv_exact(int fd, uint8_t *buf, size_t len, int timeout_sec) {
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    size_t got = 0;
    while (got < len) {
        int r = poll(&pfd, 1, timeout_sec * 1000);
        if (r <= 0)
            return r == 0 ? HU_ERR_TIMEOUT : HU_ERR_IO;
        ssize_t n = recv(fd, buf + got, len - got, 0);
        if (n <= 0)
            return HU_ERR_IO;
        got += (size_t)n;
    }
    return HU_OK;
}

/* Decode remaining length, returns bytes consumed or 0 on error. */
static size_t mqtt_decode_remaining_length(const uint8_t *buf, size_t buf_len, size_t *out_len) {
    size_t val = 0;
    size_t shift = 0;
    for (size_t i = 0; i < 4 && i < buf_len; i++) {
        val += (size_t)(buf[i] & 0x7F) << shift;
        if ((buf[i] & 0x80) == 0) {
            *out_len = val;
            return i + 1;
        }
        shift += 7;
    }
    return 0;
}
#endif /* HU_GATEWAY_POSIX && !HU_IS_TEST */

static hu_error_t mqtt_start(void *ctx) {
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
#if HU_IS_TEST
    c->running = true;
    return HU_OK;
#else
    if (!c->broker_url || c->broker_url[0] == '\0')
        return HU_ERR_NOT_SUPPORTED;
    c->running = true;
    return HU_OK;
#endif
}

static void mqtt_stop(void *ctx) {
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ctx;
    if (c)
        c->running = false;
}

static hu_error_t mqtt_send(void *ctx, const char *target, size_t target_len, const char *message,
                            size_t message_len, const char *const *media, size_t media_count) {
    (void)target;
    (void)target_len;
    (void)media;
    (void)media_count;
#if HU_IS_TEST
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    if (!c->running)
        return HU_ERR_INVALID_ARGUMENT;
    if (message && message_len > 0) {
        size_t copy = message_len;
        if (copy >= HU_MQTT_LAST_MSG_LEN)
            copy = HU_MQTT_LAST_MSG_LEN - 1;
        memcpy(c->last_message, message, copy);
        c->last_message[copy] = '\0';
        c->last_message_len = copy;
    }
    return HU_OK;
#else
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ctx;
    if (!c || !c->alloc)
        return HU_ERR_INVALID_ARGUMENT;
    if (!c->broker_url || c->broker_url[0] == '\0')
        return HU_ERR_NOT_SUPPORTED;
    if (!message || message_len > HU_MQTT_MAX_MSG)
        return HU_ERR_INVALID_ARGUMENT;
    if (!c->running)
        return HU_ERR_INVALID_ARGUMENT;

#if defined(HU_GATEWAY_POSIX)
    const char *topic = c->outbound_topic && c->outbound_topic[0] ? c->outbound_topic : "human/out";
    char host[256];
    uint16_t port;
    if (!mqtt_parse_broker(c->broker_url, host, sizeof(host), &port))
        return HU_ERR_INVALID_ARGUMENT;

    int fd = mqtt_tcp_connect(host, port);
    if (fd < 0)
        return HU_ERR_IO;

    uint8_t pkt[4096];
    size_t pkt_len = mqtt_build_connect(pkt, sizeof(pkt), "human", c->username, c->password);
    if (pkt_len == 0) {
        close(fd);
        return HU_ERR_INVALID_ARGUMENT;
    }
    hu_error_t err = mqtt_send_all(fd, pkt, pkt_len);
    if (err != HU_OK) {
        close(fd);
        return err;
    }
    uint8_t connack[4];
    err = mqtt_recv_exact(fd, connack, sizeof(connack), 5);
    if (err != HU_OK || connack[0] != 0x20 || connack[3] != 0x00) {
        close(fd);
        return err != HU_OK ? err : HU_ERR_IO;
    }

    pkt_len = mqtt_build_publish(pkt, sizeof(pkt), topic, message, message_len);
    if (pkt_len == 0) {
        close(fd);
        return HU_ERR_INVALID_ARGUMENT;
    }
    err = mqtt_send_all(fd, pkt, pkt_len);
    if (err != HU_OK) {
        close(fd);
        return err;
    }

    pkt_len = mqtt_build_disconnect(pkt, sizeof(pkt));
    (void)mqtt_send_all(fd, pkt, pkt_len);
    close(fd);
    return HU_OK;
#else
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

static const char *mqtt_name(void *ctx) {
    (void)ctx;
    return "mqtt";
}

static bool mqtt_health_check(void *ctx) {
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ctx;
#if HU_IS_TEST
    return c != NULL;
#else
    return c && c->running;
#endif
}

/* MQTT is a pub/sub protocol without message retention. History would require
 * an external persistence layer. */
static hu_error_t mqtt_load_conversation_history(void *ctx, hu_allocator_t *alloc,
                                                 const char *contact_id, size_t contact_id_len,
                                                 size_t limit, hu_channel_history_entry_t **out,
                                                 size_t *out_count) {
    (void)ctx;
    (void)alloc;
    (void)contact_id;
    (void)contact_id_len;
    (void)limit;
    (void)out;
    (void)out_count;
    return HU_ERR_NOT_SUPPORTED;
}

static const hu_channel_vtable_t mqtt_vtable = {
    .start = mqtt_start,
    .stop = mqtt_stop,
    .send = mqtt_send,
    .name = mqtt_name,
    .health_check = mqtt_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
    .load_conversation_history = mqtt_load_conversation_history,
};

hu_error_t hu_mqtt_poll(void *channel_ctx, hu_allocator_t *alloc, hu_channel_loop_msg_t *msgs,
                        size_t max_msgs, size_t *out_count) {
    hu_mqtt_ctx_t *ctx = (hu_mqtt_ctx_t *)channel_ctx;
    if (!ctx || !msgs || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;
#if HU_IS_TEST
    (void)alloc;
    for (size_t i = 0; i < ctx->mock_count && i < max_msgs; i++) {
        size_t sk = strlen(ctx->mock_msgs[i].session_key);
        if (sk > 127)
            sk = 127;
        memcpy(msgs[i].session_key, ctx->mock_msgs[i].session_key, sk);
        msgs[i].session_key[sk] = '\0';
        size_t ct = strlen(ctx->mock_msgs[i].content);
        if (ct > 4095)
            ct = 4095;
        memcpy(msgs[i].content, ctx->mock_msgs[i].content, ct);
        msgs[i].content[ct] = '\0';
    }
    *out_count = ctx->mock_count > max_msgs ? max_msgs : ctx->mock_count;
    return HU_OK;
#else
#if defined(HU_GATEWAY_POSIX)
    (void)alloc;
    const char *topic =
        ctx->inbound_topic && ctx->inbound_topic[0] ? ctx->inbound_topic : "human/in";
    char host[256];
    uint16_t port;
    if (!mqtt_parse_broker(ctx->broker_url, host, sizeof(host), &port))
        return HU_ERR_INVALID_ARGUMENT;

    int fd = mqtt_tcp_connect(host, port);
    if (fd < 0)
        return HU_OK; /* No broker: return empty, not error */

    uint8_t pkt[4096];
    size_t pkt_len = mqtt_build_connect(pkt, sizeof(pkt), "human", ctx->username, ctx->password);
    if (pkt_len == 0) {
        close(fd);
        return HU_OK;
    }
    hu_error_t err = mqtt_send_all(fd, pkt, pkt_len);
    if (err != HU_OK) {
        close(fd);
        return HU_OK;
    }
    uint8_t connack[4];
    err = mqtt_recv_exact(fd, connack, sizeof(connack), 5);
    if (err != HU_OK || connack[0] != 0x20 || connack[3] != 0x00) {
        close(fd);
        return HU_OK;
    }

    uint8_t qos_byte = (ctx->qos > 2 || ctx->qos < 0) ? 0 : (uint8_t)ctx->qos;
    pkt_len = mqtt_build_subscribe(pkt, sizeof(pkt), 1, topic, qos_byte);
    if (pkt_len == 0) {
        close(fd);
        return HU_OK;
    }
    err = mqtt_send_all(fd, pkt, pkt_len);
    if (err != HU_OK) {
        close(fd);
        return HU_OK;
    }
    uint8_t suback[5];
    err = mqtt_recv_exact(fd, suback, sizeof(suback), 5);
    if (err != HU_OK || suback[0] != 0x90) {
        close(fd);
        return HU_OK;
    }

    size_t count = 0;
    uint8_t recv_buf[8192];
    size_t recv_len = 0;
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    int deadline_sec = HU_MQTT_POLL_TIMEOUT;

    while (count < max_msgs && deadline_sec > 0) {
        int r = poll(&pfd, 1, 1000);
        if (r < 0)
            break;
        if (r == 0) {
            deadline_sec--;
            continue;
        }
        ssize_t n = recv(fd, recv_buf + recv_len, sizeof(recv_buf) - recv_len, 0);
        if (n <= 0)
            break;
        recv_len += (size_t)n;

        while (recv_len >= 2 && count < max_msgs) {
            uint8_t ptype = recv_buf[0];
            size_t rl_val;
            size_t rl_n = mqtt_decode_remaining_length(recv_buf + 1, recv_len - 1, &rl_val);
            if (rl_n == 0)
                break;
            size_t pkt_total = 1 + rl_n + rl_val;
            if (recv_len < pkt_total)
                break;

            if ((ptype & 0xF0) == 0x30) {
                /* PUBLISH */
                size_t off = 1 + rl_n;
                if (rl_val < 2)
                    goto next_pkt;
                uint16_t topic_len = (uint16_t)((recv_buf[off] << 8) | recv_buf[off + 1]);
                off += 2;
                if (rl_val < 2 + (size_t)topic_len)
                    goto next_pkt;
                uint8_t has_packet_id = (ptype & 0x06) != 0;
                size_t var_hdr_len = 2 + (size_t)topic_len + (has_packet_id ? 2 : 0);
                if (rl_val < var_hdr_len)
                    goto next_pkt;
                size_t payload_off = off + (size_t)topic_len + (has_packet_id ? 2 : 0);
                size_t payload_len = rl_val - var_hdr_len;
                size_t sk_len = topic_len > 127 ? 127 : topic_len;
                memcpy(msgs[count].session_key, recv_buf + off, sk_len);
                msgs[count].session_key[sk_len] = '\0';
                size_t ct_len = payload_len > 4095 ? 4095 : payload_len;
                memcpy(msgs[count].content, recv_buf + payload_off, ct_len);
                msgs[count].content[ct_len] = '\0';
                msgs[count].is_group = false;
                msgs[count].message_id = -1;
                msgs[count].has_attachment = false;
                msgs[count].has_video = false;
                msgs[count].guid[0] = '\0';
                count++;
            }
        next_pkt:
            memmove(recv_buf, recv_buf + pkt_total, recv_len - pkt_total);
            recv_len -= pkt_total;
        }
    }

    pkt_len = mqtt_build_disconnect(pkt, sizeof(pkt));
    (void)mqtt_send_all(fd, pkt, pkt_len);
    close(fd);
    *out_count = count;
    return HU_OK;
#else
    (void)alloc;
    (void)max_msgs;
    return HU_ERR_NOT_SUPPORTED;
#endif
#endif
}

#if HU_IS_TEST
hu_error_t hu_mqtt_test_inject_mock(hu_channel_t *ch, const char *session_key,
                                    size_t session_key_len, const char *content,
                                    size_t content_len) {
    if (!ch || !ch->ctx)
        return HU_ERR_INVALID_ARGUMENT;
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ch->ctx;
    if (c->mock_count >= HU_MQTT_MOCK_MAX)
        return HU_ERR_OUT_OF_MEMORY;
    size_t i = c->mock_count++;
    size_t sk = session_key_len > 127 ? 127 : session_key_len;
    size_t ct = content_len > 4095 ? 4095 : content_len;
    if (session_key && sk > 0)
        memcpy(c->mock_msgs[i].session_key, session_key, sk);
    c->mock_msgs[i].session_key[sk] = '\0';
    if (content && ct > 0)
        memcpy(c->mock_msgs[i].content, content, ct);
    c->mock_msgs[i].content[ct] = '\0';
    return HU_OK;
}

const char *hu_mqtt_test_get_last_message(hu_channel_t *ch, size_t *out_len) {
    if (!ch || !ch->ctx)
        return NULL;
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ch->ctx;
    if (out_len)
        *out_len = c->last_message_len;
    return c->last_message;
}
#endif

hu_error_t hu_mqtt_create(hu_allocator_t *alloc, const char *broker_url, size_t broker_url_len,
                          const char *inbound_topic, size_t inbound_topic_len,
                          const char *outbound_topic, size_t outbound_topic_len,
                          const char *username, size_t username_len, const char *password,
                          size_t password_len, int qos, hu_channel_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;
    if (!broker_url || broker_url_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->qos = qos;

    c->broker_url = hu_strndup(alloc, broker_url, broker_url_len);
    if (!c->broker_url)
        goto oom;
    if (inbound_topic && inbound_topic_len > 0) {
        c->inbound_topic = hu_strndup(alloc, inbound_topic, inbound_topic_len);
        if (!c->inbound_topic)
            goto oom;
    }
    if (outbound_topic && outbound_topic_len > 0) {
        c->outbound_topic = hu_strndup(alloc, outbound_topic, outbound_topic_len);
        if (!c->outbound_topic)
            goto oom;
    }
    if (username && username_len > 0) {
        c->username = hu_strndup(alloc, username, username_len);
        if (!c->username)
            goto oom;
    }
    if (password && password_len > 0) {
        c->password = hu_strndup(alloc, password, password_len);
        if (!c->password)
            goto oom;
    }

    out->ctx = c;
    out->vtable = &mqtt_vtable;
    return HU_OK;
oom:
    if (c->broker_url) {
        size_t n = strlen(c->broker_url) + 1;
        alloc->free(alloc->ctx, c->broker_url, n);
    }
    if (c->inbound_topic) {
        size_t n = strlen(c->inbound_topic) + 1;
        alloc->free(alloc->ctx, c->inbound_topic, n);
    }
    if (c->outbound_topic) {
        size_t n = strlen(c->outbound_topic) + 1;
        alloc->free(alloc->ctx, c->outbound_topic, n);
    }
    if (c->username) {
        size_t n = strlen(c->username) + 1;
        alloc->free(alloc->ctx, c->username, n);
    }
    if (c->password) {
        size_t n = strlen(c->password) + 1;
        alloc->free(alloc->ctx, c->password, n);
    }
    alloc->free(alloc->ctx, c, sizeof(*c));
    return HU_ERR_OUT_OF_MEMORY;
}

void hu_mqtt_destroy(hu_channel_t *ch, hu_allocator_t *alloc) {
    if (!ch || !ch->ctx)
        return;
    hu_mqtt_ctx_t *c = (hu_mqtt_ctx_t *)ch->ctx;
    hu_allocator_t *a = alloc ? alloc : c->alloc;
    if (a) {
        if (c->broker_url) {
            size_t n = strlen(c->broker_url) + 1;
            a->free(a->ctx, c->broker_url, n);
        }
        if (c->inbound_topic) {
            size_t n = strlen(c->inbound_topic) + 1;
            a->free(a->ctx, c->inbound_topic, n);
        }
        if (c->outbound_topic) {
            size_t n = strlen(c->outbound_topic) + 1;
            a->free(a->ctx, c->outbound_topic, n);
        }
        if (c->username) {
            size_t n = strlen(c->username) + 1;
            a->free(a->ctx, c->username, n);
        }
        if (c->password) {
            size_t n = strlen(c->password) + 1;
            a->free(a->ctx, c->password, n);
        }
    }
    a->free(a->ctx, c, sizeof(*c));
    ch->ctx = NULL;
    ch->vtable = NULL;
}
