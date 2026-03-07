#include "seaclaw/agent/episodic.h"
#include "seaclaw/core/string.h"
#include "seaclaw/provider.h"
#include <string.h>

#define SC_EPISODIC_LLM_USER_CAP     4000
#define SC_EPISODIC_STUB_PREFIX      "LLM summary: "
#define SC_EPISODIC_STUB_PREFIX_LEN  13
#define SC_EPISODIC_STUB_FIRST_CHARS 50

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

#ifndef SC_IS_TEST
/* Build user content from messages: "User: <msg>\nAssistant: <msg>\n..." capped at 4000 chars. */
static char *build_llm_user_content(sc_allocator_t *alloc, const char *const *messages,
                                    const size_t *message_lens, size_t message_count,
                                    size_t *out_alloc) {
    static const char user_prefix[] = "User: ";
    static const char asst_prefix[] = "Assistant: ";
    const size_t user_prefix_len = 6;
    const size_t asst_prefix_len = 11;

    size_t buf_cap = SC_EPISODIC_LLM_USER_CAP + 1;
    char *buf = (char *)alloc->alloc(alloc->ctx, buf_cap);
    if (!buf)
        return NULL;

    size_t pos = 0;
    for (size_t i = 0; i < message_count && pos < SC_EPISODIC_LLM_USER_CAP; i++) {
        const char *prefix = (i % 2 == 0) ? user_prefix : asst_prefix;
        size_t prefix_len = (i % 2 == 0) ? user_prefix_len : asst_prefix_len;
        size_t msg_len = (messages[i] && message_lens) ? message_lens[i] : 0;

        if (pos + prefix_len > SC_EPISODIC_LLM_USER_CAP)
            break;
        memcpy(buf + pos, prefix, prefix_len);
        pos += prefix_len;

        size_t remain = SC_EPISODIC_LLM_USER_CAP - pos;
        size_t to_copy = msg_len < remain ? msg_len : remain;
        if (to_copy > 0 && messages[i])
            memcpy(buf + pos, messages[i], to_copy);
        pos += to_copy;

        if (pos < SC_EPISODIC_LLM_USER_CAP)
            buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    *out_alloc = pos + 1;
    return buf;
}
#endif /* !SC_IS_TEST */

char *sc_episodic_summarize_session_llm(sc_allocator_t *alloc, sc_provider_t *provider,
                                        const char *const *messages, const size_t *message_lens,
                                        size_t message_count, size_t *out_len) {
    if (!alloc || !messages)
        return NULL;

    if (!provider)
        return sc_episodic_summarize_session(alloc, messages, message_lens, message_count, out_len);

#ifdef SC_IS_TEST
    /* Deterministic stub: no provider call in tests. */
    const char *first_msg = NULL;
    size_t first_len = 0;
    for (size_t i = 0; i < message_count; i++) {
        if (messages[i] && (!message_lens || message_lens[i] > 0)) {
            first_msg = messages[i];
            first_len = message_lens ? message_lens[i] : strlen(messages[i]);
            break;
        }
    }
    size_t copy_len = first_len > SC_EPISODIC_STUB_FIRST_CHARS ? SC_EPISODIC_STUB_FIRST_CHARS
                                                               : (first_len ? first_len : 0);
    size_t total = SC_EPISODIC_STUB_PREFIX_LEN + copy_len + 1;
    char *buf = (char *)alloc->alloc(alloc->ctx, total);
    if (!buf)
        return NULL;
    memcpy(buf, SC_EPISODIC_STUB_PREFIX, SC_EPISODIC_STUB_PREFIX_LEN);
    if (copy_len > 0 && first_msg)
        memcpy(buf + SC_EPISODIC_STUB_PREFIX_LEN, first_msg, copy_len);
    buf[SC_EPISODIC_STUB_PREFIX_LEN + copy_len] = '\0';
    if (out_len)
        *out_len = SC_EPISODIC_STUB_PREFIX_LEN + copy_len;
    return buf;
#else
    if (!provider || !provider->vtable || !provider->vtable->chat_with_system)
        return sc_episodic_summarize_session(alloc, messages, message_lens, message_count, out_len);

    if (message_count == 0)
        return sc_episodic_summarize_session(alloc, messages, message_lens, message_count, out_len);

    static const char sys_prompt[] =
        "Summarize this conversation in 2-3 concise sentences. Focus on the key topic, "
        "actions taken, and outcome. No preamble.";
    size_t sys_len = sizeof(sys_prompt) - 1;

    size_t user_alloc = 0;
    char *user_content =
        build_llm_user_content(alloc, messages, message_lens, message_count, &user_alloc);
    if (!user_content)
        return sc_episodic_summarize_session(alloc, messages, message_lens, message_count, out_len);

    static const char model[] = "gpt-4o-mini";
    char *llm_out = NULL;
    size_t llm_out_len = 0;
    sc_error_t err = provider->vtable->chat_with_system(
        provider->ctx, alloc, sys_prompt, sys_len, user_content, strlen(user_content), model,
        sizeof(model) - 1, 0.0, &llm_out, &llm_out_len);

    alloc->free(alloc->ctx, user_content, user_alloc);

    if (err != SC_OK || !llm_out || llm_out_len == 0) {
        if (llm_out)
            alloc->free(alloc->ctx, llm_out, llm_out_len + 1);
        return sc_episodic_summarize_session(alloc, messages, message_lens, message_count, out_len);
    }

    if (llm_out_len > SC_EPISODIC_MAX_SUMMARY) {
        llm_out[SC_EPISODIC_MAX_SUMMARY] = '\0';
        llm_out_len = SC_EPISODIC_MAX_SUMMARY;
    }

    if (out_len)
        *out_len = llm_out_len;
    return llm_out;
#endif
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
    if (!out || !alloc)
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
    for (size_t i = 0; i < count; i++)
        sc_memory_entry_free_fields(alloc, &entries[i]);
    alloc->free(alloc->ctx, entries, count * sizeof(sc_memory_entry_t));
    return err;
}
