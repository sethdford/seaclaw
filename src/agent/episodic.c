#include "seaclaw/agent/episodic.h"
#include "seaclaw/core/string.h"
#include <string.h>

char *sc_episodic_summarize_session(sc_allocator_t *alloc, const char *const *messages,
                                    const size_t *message_lens, size_t message_count,
                                    size_t *out_len) {
    if (!alloc || !messages || !message_lens || message_count == 0)
        return NULL;

    /* Build a naive summary: first user message + topic signal.
     * In production this would use the LLM, but the rule-based approach is zero-cost. */
    const char *first_user = NULL;
    size_t first_user_len = 0;
    for (size_t i = 0; i < message_count; i += 2) {
        if (messages[i] && message_lens[i] > 0) {
            first_user = messages[i];
            first_user_len = message_lens[i];
            break;
        }
    }

    if (!first_user)
        return NULL;

    const char *prefix = "Session topic: ";
    size_t prefix_len = 15;
    size_t cap = first_user_len > (SC_EPISODIC_MAX_SUMMARY - prefix_len - 1)
                     ? (SC_EPISODIC_MAX_SUMMARY - prefix_len - 1)
                     : first_user_len;

    size_t total = prefix_len + cap;
    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!buf)
        return NULL;

    memcpy(buf, prefix, prefix_len);
    memcpy(buf + prefix_len, first_user, cap);
    buf[total] = '\0';

    if (out_len)
        *out_len = total;
    return buf;
}

sc_error_t sc_episodic_store(sc_memory_t *memory, sc_allocator_t *alloc, const char *session_id,
                             size_t session_id_len, const char *summary, size_t summary_len) {
    if (!memory || !memory->vtable || !memory->vtable->store || !summary)
        return SC_ERR_INVALID_ARGUMENT;

    /* Key: _ep:<session_id or "global"> */
    char key[128];
    size_t klen = SC_EPISODIC_KEY_PREFIX_LEN;
    memcpy(key, SC_EPISODIC_KEY_PREFIX, klen);

    if (session_id && session_id_len > 0) {
        size_t copy = session_id_len > 100 ? 100 : session_id_len;
        memcpy(key + klen, session_id, copy);
        klen += copy;
    } else {
        memcpy(key + klen, "global", 6);
        klen += 6;
    }
    key[klen] = '\0';

    (void)alloc;
    return memory->vtable->store(memory->ctx, key, klen, summary, summary_len, NULL,
                                 session_id ? session_id : "", session_id_len);
}

sc_error_t sc_episodic_load(sc_memory_t *memory, sc_allocator_t *alloc, char **out,
                            size_t *out_len) {
    if (!out)
        return SC_ERR_INVALID_ARGUMENT;
    *out = NULL;
    if (out_len)
        *out_len = 0;

    if (!memory || !memory->vtable || !memory->vtable->recall)
        return SC_OK;

    sc_memory_entry_t *entries = NULL;
    size_t count = 0;
    sc_error_t err = memory->vtable->recall(memory->ctx, alloc, SC_EPISODIC_KEY_PREFIX,
                                            SC_EPISODIC_KEY_PREFIX_LEN, SC_EPISODIC_MAX_LOAD, "", 0,
                                            &entries, &count);
    if (err != SC_OK || !entries || count == 0)
        return SC_OK;

    /* Format as "## Recent Sessions\n- <summary>\n- <summary>\n" */
    const char *header = "## Recent Sessions\n";
    size_t header_len = 19;

    size_t total = header_len;
    for (size_t i = 0; i < count; i++)
        total += 2 + entries[i].content_len + 1;

    char *buf = (char *)alloc->alloc(alloc->ctx, total + 1);
    if (!buf) {
        err = SC_ERR_OUT_OF_MEMORY;
        goto cleanup;
    }

    size_t pos = 0;
    memcpy(buf + pos, header, header_len);
    pos += header_len;
    for (size_t i = 0; i < count; i++) {
        buf[pos++] = '-';
        buf[pos++] = ' ';
        if (entries[i].content && entries[i].content_len > 0) {
            memcpy(buf + pos, entries[i].content, entries[i].content_len);
            pos += entries[i].content_len;
        }
        buf[pos++] = '\n';
    }
    buf[pos] = '\0';

    *out = buf;
    if (out_len)
        *out_len = pos;

cleanup:
    for (size_t i = 0; i < count; i++) {
        if (entries[i].key)
            alloc->free(alloc->ctx, (void *)entries[i].key, entries[i].key_len + 1);
        if (entries[i].content)
            alloc->free(alloc->ctx, (void *)entries[i].content, entries[i].content_len + 1);
        if (entries[i].id)
            alloc->free(alloc->ctx, (void *)entries[i].id, entries[i].id_len + 1);
        if (entries[i].timestamp)
            alloc->free(alloc->ctx, (void *)entries[i].timestamp, entries[i].timestamp_len + 1);
        if (entries[i].session_id)
            alloc->free(alloc->ctx, (void *)entries[i].session_id, entries[i].session_id_len + 1);
    }
    alloc->free(alloc->ctx, entries, count * sizeof(sc_memory_entry_t));
    return err;
}
