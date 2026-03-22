/*
 * IMAP channel — poll mailbox via libcurl IMAP, send via libcurl SMTP when configured.
 * HU_IS_TEST: mock poll queue + in-memory outbox only.
 */
#include "human/channels/imap.h"
#include "human/channel.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include "human/core/string.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HU_IMAP_OUTBOX_MAX      32
#define HU_IMAP_MOCK_QUEUE_MAX  16
#define HU_IMAP_SESSION_KEY_MAX 127
#define HU_IMAP_CONTENT_MAX     4095
#define HU_IMAP_MAX_IMAP_RESPONSE (128u * 1024u)
#define HU_IMAP_MAILBOX_ENC_MAX   512
#define HU_IMAP_URL_MAX           768
#define HU_IMAP_MIME_MAX          (64u * 1024u)

typedef struct hu_imap_outbox_entry {
    char *target;
    char *message;
} hu_imap_outbox_entry_t;

#if HU_IS_TEST
typedef struct hu_imap_mock_msg {
    char session_key[HU_IMAP_SESSION_KEY_MAX + 1];
    char content[HU_IMAP_CONTENT_MAX + 1];
} hu_imap_mock_msg_t;
#endif

typedef struct hu_imap_ctx {
    hu_allocator_t *alloc;
    char *imap_host;
    size_t imap_host_len;
    uint16_t imap_port;
    char *imap_username;
    size_t imap_username_len;
    char *imap_password;
    size_t imap_password_len;
    char *imap_folder;
    size_t imap_folder_len;
    bool imap_use_tls;
    char *smtp_host;
    size_t smtp_host_len;
    uint16_t smtp_port;
    char *from_address;
    size_t from_address_len;
    unsigned int last_uid;
    bool running;
    hu_imap_outbox_entry_t outbox[HU_IMAP_OUTBOX_MAX];
    size_t outbox_count;
#if HU_IS_TEST
    hu_imap_mock_msg_t mock_queue[HU_IMAP_MOCK_QUEUE_MAX];
    size_t mock_head;
    size_t mock_tail;
    size_t mock_count;
#endif
} hu_imap_ctx_t;

#if !HU_IS_TEST && defined(HU_HTTP_CURL)
#include <curl/curl.h>

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    hu_allocator_t *alloc;
} imap_wr_t;

static void imap_wr_free(hu_allocator_t *a, imap_wr_t *w) {
    if (!w)
        return;
    if (w->buf && a)
        a->free(a->ctx, w->buf, w->cap);
    memset(w, 0, sizeof(*w));
}

static size_t imap_wr_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    imap_wr_t *w = (imap_wr_t *)userdata;
    size_t n = size * nmemb;
    if (n == 0)
        return 0;
    if (w->len + n + 1 > HU_IMAP_MAX_IMAP_RESPONSE)
        return 0;
    if (w->len + n + 1 > w->cap) {
        size_t new_cap = w->cap ? w->cap * 2 : 4096;
        while (new_cap < w->len + n + 1)
            new_cap *= 2;
        if (new_cap > HU_IMAP_MAX_IMAP_RESPONSE)
            return 0;
        char *nbuf = (char *)w->alloc->realloc(w->alloc->ctx, w->buf, w->cap ? w->cap : 0, new_cap);
        if (!nbuf)
            return 0;
        w->buf = nbuf;
        w->cap = new_cap;
    }
    memcpy(w->buf + w->len, ptr, n);
    w->len += n;
    w->buf[w->len] = '\0';
    return n;
}

static int imap_encode_mailbox_path(const char *folder, char *out, size_t out_cap) {
    if (!folder || !out || out_cap < 2)
        return -1;
    size_t o = 0;
    for (size_t i = 0; folder[i] && o + 4 < out_cap; i++) {
        unsigned char c = (unsigned char)folder[i];
        if (c == ' ') {
            out[o++] = '%';
            out[o++] = '2';
            out[o++] = '0';
        } else if (c == '%' || c == '/' || c == '#' || c == '?' || c == '@') {
            if (o + 3 >= out_cap)
                return -1;
            (void)snprintf(out + o, out_cap - o, "%%%02X", c);
            o += 3;
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return (int)o;
}

static hu_error_t imap_build_mailbox_url(hu_imap_ctx_t *c, char *url, size_t url_cap) {
    char enc[HU_IMAP_MAILBOX_ENC_MAX];
    if (imap_encode_mailbox_path(c->imap_folder, enc, sizeof(enc)) < 0)
        return HU_ERR_INVALID_ARGUMENT;
    const char *scheme = c->imap_use_tls ? "imaps" : "imap";
    int n = snprintf(url, url_cap, "%s://%.*s:%u/%s", scheme, (int)c->imap_host_len, c->imap_host,
                     (unsigned)c->imap_port, enc);
    if (n < 0 || (size_t)n >= url_cap)
        return HU_ERR_IO;
    return HU_OK;
}

static hu_error_t imap_curl_imap(hu_allocator_t *alloc, hu_imap_ctx_t *c, const char *url,
                                 const char *custom_request, imap_wr_t *wr, long timeout_sec) {
    if (!alloc || !c || !url || !wr)
        return HU_ERR_INVALID_ARGUMENT;
    memset(wr, 0, sizeof(*wr));
    wr->alloc = alloc;
    wr->buf = (char *)alloc->alloc(alloc->ctx, 4096);
    if (!wr->buf)
        return HU_ERR_OUT_OF_MEMORY;
    wr->cap = 4096;
    wr->buf[0] = '\0';

    CURL *curl = curl_easy_init();
    if (!curl) {
        imap_wr_free(alloc, wr);
        return HU_ERR_NOT_SUPPORTED;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, c->imap_username);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, c->imap_password);
    if (custom_request && custom_request[0])
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, custom_request);
    else
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, imap_wr_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, wr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode cc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (cc != CURLE_OK) {
        imap_wr_free(alloc, wr);
        return HU_ERR_IO;
    }
    return HU_OK;
}

static const char *imap_find_header_ci(const char *hdr, size_t hdr_len, const char *name,
                                       size_t name_len) {
    for (size_t i = 0; i + name_len + 1 < hdr_len; i++) {
        if (i > 0 && hdr[i - 1] != '\n')
            continue;
        size_t j;
        for (j = 0; j < name_len; j++) {
            if (tolower((unsigned char)hdr[i + j]) != tolower((unsigned char)name[j]))
                break;
        }
        if (j == name_len && hdr[i + j] == ':') {
            size_t v = i + j + 1;
            while (v < hdr_len && hdr[v] == ' ')
                v++;
            return hdr + v;
        }
    }
    return NULL;
}

typedef struct {
    const char *base;
    size_t len;
    size_t pos;
} imap_smtp_read_t;

static size_t imap_smtp_read_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    imap_smtp_read_t *r = (imap_smtp_read_t *)userdata;
    size_t avail = size * nmemb;
    size_t left = r->len > r->pos ? r->len - r->pos : 0;
    size_t take = left < avail ? left : avail;
    if (take > 0)
        memcpy(ptr, r->base + r->pos, take);
    r->pos += take;
    return take;
}

static hu_error_t imap_smtp_send_mime(hu_imap_ctx_t *c, const char *target, size_t target_len,
                                      const char *message, size_t message_len) {
    if (!c->smtp_host || c->smtp_host_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    const char *from = c->from_address && c->from_address[0] ? c->from_address : c->imap_username;
    if (!from || !from[0])
        return HU_ERR_INVALID_ARGUMENT;

    char *mime = (char *)c->alloc->alloc(c->alloc->ctx, HU_IMAP_MIME_MAX);
    if (!mime)
        return HU_ERR_OUT_OF_MEMORY;
    int mn = snprintf(mime, HU_IMAP_MIME_MAX,
                      "From: %s\r\nTo: %.*s\r\nSubject: human\r\n"
                      "Content-Type: text/plain; charset=utf-8\r\n\r\n%.*s",
                      from, (int)target_len, target, (int)message_len, message);
    if (mn < 0 || (size_t)mn >= HU_IMAP_MIME_MAX) {
        c->alloc->free(c->alloc->ctx, mime, HU_IMAP_MIME_MAX);
        return HU_ERR_IO;
    }
    size_t mime_len = (size_t)mn;

    char *target_z = hu_strndup(c->alloc, target, target_len);
    if (!target_z) {
        c->alloc->free(c->alloc->ctx, mime, HU_IMAP_MIME_MAX);
        return HU_ERR_OUT_OF_MEMORY;
    }

    char smtp_url[512];
    bool implicit_tls = (c->smtp_port == 465);
    int sn = snprintf(smtp_url, sizeof(smtp_url), "%s://%.*s:%u", implicit_tls ? "smtps" : "smtp",
                      (int)c->smtp_host_len, c->smtp_host, (unsigned)c->smtp_port);
    if (sn < 0 || (size_t)sn >= sizeof(smtp_url)) {
        hu_str_free(c->alloc, target_z);
        c->alloc->free(c->alloc->ctx, mime, HU_IMAP_MIME_MAX);
        return HU_ERR_IO;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        hu_str_free(c->alloc, target_z);
        c->alloc->free(c->alloc->ctx, mime, HU_IMAP_MIME_MAX);
        return HU_ERR_NOT_SUPPORTED;
    }

    struct curl_slist *rcpt = curl_slist_append(NULL, target_z);
    if (!rcpt) {
        curl_easy_cleanup(curl);
        hu_str_free(c->alloc, target_z);
        c->alloc->free(c->alloc->ctx, mime, HU_IMAP_MIME_MAX);
        return HU_ERR_OUT_OF_MEMORY;
    }

    imap_smtp_read_t rd = {.base = mime, .len = mime_len, .pos = 0};

    curl_easy_setopt(curl, CURLOPT_URL, smtp_url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, c->imap_username);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, c->imap_password);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from);
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpt);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, imap_smtp_read_cb);
    curl_easy_setopt(curl, CURLOPT_READDATA, &rd);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)mime_len);
    if (c->smtp_port == 587)
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode cc = curl_easy_perform(curl);
    curl_slist_free_all(rcpt);
    curl_easy_cleanup(curl);
    hu_str_free(c->alloc, target_z);
    c->alloc->free(c->alloc->ctx, mime, HU_IMAP_MIME_MAX);

    return (cc == CURLE_OK) ? HU_OK : HU_ERR_IO;
}

static bool imap_curl_noop_ok(hu_imap_ctx_t *c) {
    char url[HU_IMAP_URL_MAX];
    if (imap_build_mailbox_url(c, url, sizeof(url)) != HU_OK)
        return false;
    imap_wr_t wr = {0};
    if (imap_curl_imap(c->alloc, c, url, "NOOP", &wr, 10L) != HU_OK) {
        imap_wr_free(c->alloc, &wr);
        return false;
    }
    imap_wr_free(c->alloc, &wr);
    return true;
}
#endif /* !HU_IS_TEST && HU_HTTP_CURL */

static hu_error_t imap_outbox_append(hu_imap_ctx_t *c, const char *target, size_t target_len,
                                     const char *message, size_t message_len) {
    if (c->outbox_count >= HU_IMAP_OUTBOX_MAX)
        return HU_ERR_OUT_OF_MEMORY;
    char *t = hu_strndup(c->alloc, target, target_len);
    char *m = hu_strndup(c->alloc, message, message_len);
    if (!t || !m) {
        if (t)
            c->alloc->free(c->alloc->ctx, t, target_len + 1);
        if (m)
            c->alloc->free(c->alloc->ctx, m, message_len + 1);
        return HU_ERR_OUT_OF_MEMORY;
    }
    c->outbox[c->outbox_count].target = t;
    c->outbox[c->outbox_count].message = m;
    c->outbox_count++;
    return HU_OK;
}

static hu_error_t imap_start(void *ctx) {
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)ctx;
    if (!c)
        return HU_ERR_INVALID_ARGUMENT;
    c->running = true;
    return HU_OK;
}

static void imap_stop(void *ctx) {
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)ctx;
    if (c)
        c->running = false;
}

static hu_error_t imap_send(void *ctx, const char *target, size_t target_len, const char *message,
                            size_t message_len, const char *const *media, size_t media_count) {
    (void)media;
    (void)media_count;
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)ctx;
    if (!c || !c->alloc)
        return HU_ERR_INVALID_ARGUMENT;
    if (!target || target_len == 0 || !message)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    return imap_outbox_append(c, target, target_len, message, message_len);
#else
#if defined(HU_HTTP_CURL)
    if (c->smtp_host && c->smtp_host_len > 0)
        return imap_smtp_send_mime(c, target, target_len, message, message_len);
#endif
    static bool imap_send_warned;
    if (!imap_send_warned) {
        fprintf(stderr,
                "[imap] send: no smtp_host configured; messages stored in local outbox only\n");
        imap_send_warned = true;
    }
    return imap_outbox_append(c, target, target_len, message, message_len);
#endif
}

static const char *imap_name(void *ctx) {
    (void)ctx;
    return "imap";
}

static bool imap_health_check(void *ctx) {
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)ctx;
    if (!c)
        return false;
    if (!(c->imap_host && c->imap_host[0] && c->imap_username && c->imap_username[0] &&
          c->imap_password && c->imap_password[0]))
        return false;
#if !HU_IS_TEST && defined(HU_HTTP_CURL)
    return imap_curl_noop_ok(c);
#else
    return true;
#endif
}

static const hu_channel_vtable_t imap_vtable = {
    .start = imap_start,
    .stop = imap_stop,
    .send = imap_send,
    .name = imap_name,
    .health_check = imap_health_check,
    .send_event = NULL,
    .start_typing = NULL,
    .stop_typing = NULL,
};

static hu_error_t copy_cfg_str(hu_allocator_t *alloc, const char *src, size_t src_len, char **out,
                               size_t *out_len) {
    if (!src || src_len == 0) {
        *out = NULL;
        *out_len = 0;
        return HU_OK;
    }
    *out = (char *)alloc->alloc(alloc->ctx, src_len + 1);
    if (!*out)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(*out, src, src_len);
    (*out)[src_len] = '\0';
    *out_len = src_len;
    return HU_OK;
}

hu_error_t hu_imap_create(hu_allocator_t *alloc, const hu_imap_config_t *config,
                          hu_channel_t *out) {
    if (!alloc || !out)
        return HU_ERR_INVALID_ARGUMENT;

    hu_imap_ctx_t *c = (hu_imap_ctx_t *)alloc->alloc(alloc->ctx, sizeof(*c));
    if (!c)
        return HU_ERR_OUT_OF_MEMORY;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->imap_port = config && config->imap_port > 0 ? config->imap_port : 993;
    c->imap_use_tls = config ? config->imap_use_tls : true;
    c->smtp_port = config && config->smtp_port > 0 ? config->smtp_port : 587;

    hu_error_t err = HU_OK;
    if (config && config->imap_host && config->imap_host_len > 0) {
        err = copy_cfg_str(alloc, config->imap_host, config->imap_host_len, &c->imap_host,
                           &c->imap_host_len);
        if (err != HU_OK)
            goto fail;
    }
    if (config && config->imap_username && config->imap_username_len > 0) {
        err = copy_cfg_str(alloc, config->imap_username, config->imap_username_len, &c->imap_username,
                           &c->imap_username_len);
        if (err != HU_OK)
            goto fail;
    }
    if (config && config->imap_password && config->imap_password_len > 0) {
        err = copy_cfg_str(alloc, config->imap_password, config->imap_password_len, &c->imap_password,
                           &c->imap_password_len);
        if (err != HU_OK)
            goto fail;
    }
    if (config && config->imap_folder && config->imap_folder_len > 0) {
        err = copy_cfg_str(alloc, config->imap_folder, config->imap_folder_len, &c->imap_folder,
                           &c->imap_folder_len);
        if (err != HU_OK)
            goto fail;
    } else {
        c->imap_folder = hu_strndup(alloc, "INBOX", 5);
        if (!c->imap_folder) {
            err = HU_ERR_OUT_OF_MEMORY;
            goto fail;
        }
        c->imap_folder_len = 5;
    }

    if (config && config->smtp_host && config->smtp_host_len > 0) {
        err = copy_cfg_str(alloc, config->smtp_host, config->smtp_host_len, &c->smtp_host,
                           &c->smtp_host_len);
        if (err != HU_OK)
            goto fail;
    }
    if (config && config->from_address && config->from_address_len > 0) {
        err = copy_cfg_str(alloc, config->from_address, config->from_address_len, &c->from_address,
                           &c->from_address_len);
        if (err != HU_OK)
            goto fail;
    } else if (c->imap_username && strchr(c->imap_username, '@')) {
        err = copy_cfg_str(alloc, c->imap_username, c->imap_username_len, &c->from_address,
                           &c->from_address_len);
        if (err != HU_OK)
            goto fail;
    }

    out->ctx = c;
    out->vtable = &imap_vtable;
    return HU_OK;

fail:
    if (c->imap_host)
        alloc->free(alloc->ctx, c->imap_host, c->imap_host_len + 1);
    if (c->imap_username)
        alloc->free(alloc->ctx, c->imap_username, c->imap_username_len + 1);
    if (c->imap_password)
        alloc->free(alloc->ctx, c->imap_password, c->imap_password_len + 1);
    if (c->imap_folder)
        alloc->free(alloc->ctx, c->imap_folder, c->imap_folder_len + 1);
    if (c->smtp_host)
        alloc->free(alloc->ctx, c->smtp_host, c->smtp_host_len + 1);
    if (c->from_address)
        alloc->free(alloc->ctx, c->from_address, c->from_address_len + 1);
    alloc->free(alloc->ctx, c, sizeof(*c));
    return err;
}

void hu_imap_destroy(hu_channel_t *ch) {
    if (!ch || !ch->ctx)
        return;
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)ch->ctx;
    if (c->alloc) {
        for (size_t i = 0; i < c->outbox_count; i++) {
            if (c->outbox[i].target)
                c->alloc->free(c->alloc->ctx, c->outbox[i].target, strlen(c->outbox[i].target) + 1);
            if (c->outbox[i].message)
                c->alloc->free(c->alloc->ctx, c->outbox[i].message,
                               strlen(c->outbox[i].message) + 1);
        }
        if (c->imap_host)
            c->alloc->free(c->alloc->ctx, c->imap_host, c->imap_host_len + 1);
        if (c->imap_username)
            c->alloc->free(c->alloc->ctx, c->imap_username, c->imap_username_len + 1);
        if (c->imap_password)
            c->alloc->free(c->alloc->ctx, c->imap_password, c->imap_password_len + 1);
        if (c->imap_folder)
            c->alloc->free(c->alloc->ctx, c->imap_folder, c->imap_folder_len + 1);
        if (c->smtp_host)
            c->alloc->free(c->alloc->ctx, c->smtp_host, c->smtp_host_len + 1);
        if (c->from_address)
            c->alloc->free(c->alloc->ctx, c->from_address, c->from_address_len + 1);
        c->alloc->free(c->alloc->ctx, c, sizeof(*c));
    }
    ch->ctx = NULL;
    ch->vtable = NULL;
}

bool hu_imap_is_configured(hu_channel_t *ch) {
    if (!ch || !ch->ctx)
        return false;
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)ch->ctx;
    return c->imap_host != NULL && c->imap_host[0] != '\0' && c->imap_username != NULL &&
           c->imap_username[0] != '\0' && c->imap_password != NULL && c->imap_password[0] != '\0';
}

hu_error_t hu_imap_poll(void *channel_ctx, hu_allocator_t *alloc, hu_channel_loop_msg_t *msgs,
                        size_t max_msgs, size_t *out_count) {
    if (!channel_ctx || !msgs || !out_count)
        return HU_ERR_INVALID_ARGUMENT;
    *out_count = 0;

#if HU_IS_TEST
    (void)alloc;
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)channel_ctx;
    if (c->mock_count == 0)
        return HU_OK;
    size_t n = 0;
    while (n < max_msgs && c->mock_count > 0) {
        size_t idx = c->mock_head;
        size_t sk_len = strlen(c->mock_queue[idx].session_key);
        size_t ct_len = strlen(c->mock_queue[idx].content);
        if (sk_len >= sizeof(msgs[n].session_key))
            sk_len = sizeof(msgs[n].session_key) - 1;
        if (ct_len >= sizeof(msgs[n].content))
            ct_len = sizeof(msgs[n].content) - 1;
        memcpy(msgs[n].session_key, c->mock_queue[idx].session_key, sk_len);
        msgs[n].session_key[sk_len] = '\0';
        memcpy(msgs[n].content, c->mock_queue[idx].content, ct_len);
        msgs[n].content[ct_len] = '\0';
        c->mock_head = (c->mock_head + 1) % HU_IMAP_MOCK_QUEUE_MAX;
        c->mock_count--;
        n++;
    }
    *out_count = n;
    return HU_OK;
#else
#if !defined(HU_HTTP_CURL)
    (void)alloc;
    (void)channel_ctx;
    (void)msgs;
    (void)max_msgs;
    return HU_OK;
#else
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)channel_ctx;
    if (!alloc || !c->imap_host || c->imap_host_len == 0)
        return HU_ERR_INVALID_ARGUMENT;

    char base_url[HU_IMAP_URL_MAX];
    if (imap_build_mailbox_url(c, base_url, sizeof(base_url)) != HU_OK)
        return HU_ERR_IO;

    char uid_search[80];
    if (c->last_uid > 0)
        snprintf(uid_search, sizeof(uid_search), "UID SEARCH UNSEEN UID %u:*", c->last_uid + 1);
    else
        snprintf(uid_search, sizeof(uid_search), "SEARCH UNSEEN");

    imap_wr_t search_wr = {0};
    hu_error_t err = imap_curl_imap(alloc, c, base_url, uid_search, &search_wr, 60L);
    if (err != HU_OK) {
        imap_wr_free(alloc, &search_wr);
        return err;
    }

    const char *output = search_wr.buf;
    unsigned int uids[32];
    size_t uid_count = 0;
    if (output && search_wr.len > 0) {
        const char *p = strstr(output, "* SEARCH");
        if (p) {
            p += 8;
            while (*p && uid_count < 32 && uid_count < max_msgs) {
                while (*p == ' ')
                    p++;
                if (*p >= '0' && *p <= '9') {
                    unsigned int uid = 0;
                    while (*p >= '0' && *p <= '9') {
                        if (uid > (unsigned int)(UINT32_MAX / 10))
                            break;
                        uid = uid * 10 + (unsigned int)(*p - '0');
                        p++;
                    }
                    if (uid > c->last_uid)
                        uids[uid_count++] = uid;
                } else {
                    break;
                }
            }
        }
    }
    imap_wr_free(alloc, &search_wr);

    size_t count = 0;
    for (size_t i = 0; i < uid_count && count < max_msgs; i++) {
        char fetch_url[HU_IMAP_URL_MAX];
        int fn = snprintf(fetch_url, sizeof(fetch_url),
                          "%s;UID=%u;SECTION=HEADER.FIELDS%%20(FROM%%20SUBJECT)", base_url,
                          uids[i]);
        if (fn < 0 || (size_t)fn >= sizeof(fetch_url))
            continue;

        imap_wr_t hdr_wr = {0};
        err = imap_curl_imap(alloc, c, fetch_url, NULL, &hdr_wr, 60L);
        if (err != HU_OK) {
            imap_wr_free(alloc, &hdr_wr);
            continue;
        }

        const char *from_line = NULL;
        const char *subj_line = NULL;
        const char *hdr = hdr_wr.buf;
        size_t hdr_len = hdr_wr.len;
        if (hdr && hdr_len > 0) {
            from_line = imap_find_header_ci(hdr, hdr_len, "From", 4);
            subj_line = imap_find_header_ci(hdr, hdr_len, "Subject", 7);
        }

        char session[128] = "unknown";
        char content[4096] = "";
        if (from_line) {
            size_t remaining = hdr_len - (size_t)(from_line - hdr);
            while (remaining > 0 && *from_line == ' ') {
                from_line++;
                remaining--;
            }
            size_t flen = 0;
            while (flen < remaining && from_line[flen] != '\r' && from_line[flen] != '\n' &&
                   flen < sizeof(session) - 1)
                flen++;
            memcpy(session, from_line, flen);
            session[flen] = '\0';
        }
        if (subj_line) {
            size_t remaining = hdr_len - (size_t)(subj_line - hdr);
            while (remaining > 0 && *subj_line == ' ') {
                subj_line++;
                remaining--;
            }
            size_t slen = 0;
            while (slen < remaining && subj_line[slen] != '\r' && subj_line[slen] != '\n' &&
                   slen < sizeof(content) - 1)
                slen++;
            memcpy(content, subj_line, slen);
            content[slen] = '\0';
        }
        imap_wr_free(alloc, &hdr_wr);

        int bn = snprintf(fetch_url, sizeof(fetch_url), "%s;UID=%u;SECTION=TEXT", base_url, uids[i]);
        if (bn >= 0 && (size_t)bn < sizeof(fetch_url)) {
            imap_wr_t body_wr = {0};
            err = imap_curl_imap(alloc, c, fetch_url, NULL, &body_wr, 60L);
            if (err == HU_OK && body_wr.buf && body_wr.len > 0) {
                size_t ct_len = strlen(content);
                if (ct_len + 3 < sizeof(content)) {
                    size_t blen = body_wr.len;
                    size_t avail = sizeof(content) - ct_len - 3;
                    if (blen > avail)
                        blen = avail;
                    if (blen > 0) {
                        if (ct_len > 0) {
                            memcpy(content + ct_len, "\n\n", 2);
                            ct_len += 2;
                        }
                        memcpy(content + ct_len, body_wr.buf, blen);
                        content[ct_len + blen] = '\0';
                    }
                }
            }
            imap_wr_free(alloc, &body_wr);
        }

        size_t sk_len = strlen(session);
        size_t ct_len = strlen(content);
        if (sk_len >= sizeof(msgs[count].session_key))
            sk_len = sizeof(msgs[count].session_key) - 1;
        if (ct_len >= sizeof(msgs[count].content))
            ct_len = sizeof(msgs[count].content) - 1;
        memcpy(msgs[count].session_key, session, sk_len);
        msgs[count].session_key[sk_len] = '\0';
        memcpy(msgs[count].content, content, ct_len);
        msgs[count].content[ct_len] = '\0';

        c->last_uid = uids[i];
        count++;
    }

    *out_count = count;
    return HU_OK;
#endif
#endif
}

#if HU_IS_TEST
hu_error_t hu_imap_test_push_mock(hu_channel_t *ch, const char *session_key, size_t session_key_len,
                                  const char *content, size_t content_len) {
    if (!ch || !ch->ctx)
        return HU_ERR_INVALID_ARGUMENT;
    hu_imap_ctx_t *c = (hu_imap_ctx_t *)ch->ctx;
    if (c->mock_count >= HU_IMAP_MOCK_QUEUE_MAX)
        return HU_ERR_OUT_OF_MEMORY;
    size_t idx = c->mock_tail;
    size_t sk_copy =
        session_key_len > HU_IMAP_SESSION_KEY_MAX ? HU_IMAP_SESSION_KEY_MAX : session_key_len;
    size_t ct_copy = content_len > HU_IMAP_CONTENT_MAX ? HU_IMAP_CONTENT_MAX : content_len;
    memcpy(c->mock_queue[idx].session_key, session_key, sk_copy);
    c->mock_queue[idx].session_key[sk_copy] = '\0';
    memcpy(c->mock_queue[idx].content, content, ct_copy);
    c->mock_queue[idx].content[ct_copy] = '\0';
    c->mock_tail = (c->mock_tail + 1) % HU_IMAP_MOCK_QUEUE_MAX;
    c->mock_count++;
    return HU_OK;
}
#endif
