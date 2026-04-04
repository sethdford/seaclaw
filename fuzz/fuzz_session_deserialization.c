#define _DEFAULT_SOURCE
/*
 * Fuzz harness for session persistence JSON deserialization.
 * Must not crash, overflow, or leak on any input.
 *
 * Exercises: hu_session_persist_load with malformed JSON,
 *            hu_session_persist_save/load round-trip with adversarial history,
 *            hu_session_generate_id.
 */
#include "human/agent/session_persist.h"
#include "human/agent.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Write fuzz data as a session JSON file and try to load it */
static void fuzz_load_raw_json(hu_allocator_t *alloc, const uint8_t *data, size_t size) {
    /* Create a temp directory for the session */
    char tmpdir[] = "/tmp/fuzz_session_XXXXXX";
    if (!mkdtemp(tmpdir))
        return;

    /* Write the fuzz data as a session file */
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/fuzz_session.json", tmpdir);

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        rmdir(tmpdir);
        return;
    }
    fwrite(data, 1, size, f);
    fclose(f);

    /* Create a minimal agent to load into */
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = alloc;

    /* Try to load — should not crash regardless of input */
    hu_error_t err = hu_session_persist_load(alloc, &agent, tmpdir, "fuzz_session");

    /* Clean up loaded history if any */
    if (err == HU_OK && agent.history) {
        for (size_t i = 0; i < agent.history_count; i++) {
            if (agent.history[i].content)
                alloc->free(alloc->ctx, agent.history[i].content,
                           agent.history[i].content_len + 1);
            if (agent.history[i].tool_calls) {
                for (size_t t = 0; t < agent.history[i].tool_calls_count; t++) {
                    if (agent.history[i].tool_calls[t].name)
                        alloc->free(alloc->ctx, agent.history[i].tool_calls[t].name,
                                   agent.history[i].tool_calls[t].name_len + 1);
                    if (agent.history[i].tool_calls[t].id)
                        alloc->free(alloc->ctx, agent.history[i].tool_calls[t].id,
                                   agent.history[i].tool_calls[t].id_len + 1);
                    if (agent.history[i].tool_calls[t].arguments)
                        alloc->free(alloc->ctx, agent.history[i].tool_calls[t].arguments,
                                   agent.history[i].tool_calls[t].arguments_len + 1);
                }
                alloc->free(alloc->ctx, agent.history[i].tool_calls,
                           agent.history[i].tool_calls_count * sizeof(hu_tool_call_t));
            }
        }
        alloc->free(alloc->ctx, agent.history,
                   agent.history_cap * sizeof(hu_owned_message_t));
    }

    /* Clean up temp files */
    unlink(filepath);
    rmdir(tmpdir);
}

/* Fuzz session ID generation (should never crash on any buffer size) */
static void fuzz_generate_id(const uint8_t *data, size_t size) {
    if (size < 1) return;

    /* Use first byte as buffer size (0-255) */
    size_t buf_size = data[0];
    char buf[256];
    memset(buf, 0, sizeof(buf));

    /* Even with tiny or zero buffers, should not crash */
    if (buf_size > 0) {
        hu_session_generate_id(buf, buf_size);
    }
}

/* Build synthetic agent with fuzz-derived messages and try save→load round-trip */
static void fuzz_save_load_roundtrip(hu_allocator_t *alloc, const uint8_t *data, size_t size) {
    if (size < 5) return;

    char tmpdir[] = "/tmp/fuzz_session_rt_XXXXXX";
    if (!mkdtemp(tmpdir))
        return;

    /* Build a minimal agent with fuzz-derived messages */
    hu_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.alloc = alloc;

    size_t msg_count = data[0] % 8; /* 0-7 messages */
    if (msg_count == 0) {
        rmdir(tmpdir);
        return;
    }

    agent.history = (hu_owned_message_t *)alloc->alloc(
        alloc->ctx, msg_count * sizeof(hu_owned_message_t));
    if (!agent.history) {
        rmdir(tmpdir);
        return;
    }
    memset(agent.history, 0, msg_count * sizeof(hu_owned_message_t));
    agent.history_cap = msg_count;

    size_t offset = 1;
    size_t actual = 0;
    for (size_t i = 0; i < msg_count && offset < size; i++) {
        agent.history[i].role = (hu_role_t)(data[offset] % 4);
        offset++;
        if (offset >= size) break;

        size_t clen = data[offset];
        offset++;
        if (clen > 200) clen = 200;
        if (offset + clen > size) clen = size - offset;

        if (clen > 0) {
            agent.history[i].content = (char *)alloc->alloc(alloc->ctx, clen + 1);
            if (agent.history[i].content) {
                memcpy(agent.history[i].content, data + offset, clen);
                agent.history[i].content[clen] = '\0';
                agent.history[i].content_len = clen;
            }
        }
        offset += clen;
        actual++;
    }
    agent.history_count = actual;

    /* Save */
    char session_id[HU_SESSION_ID_MAX];
    hu_error_t err = hu_session_persist_save(alloc, &agent, tmpdir, session_id);

    if (err == HU_OK) {
        /* Load into a fresh agent */
        hu_agent_t agent2;
        memset(&agent2, 0, sizeof(agent2));
        agent2.alloc = alloc;

        (void)hu_session_persist_load(alloc, &agent2, tmpdir, session_id);

        /* Free loaded agent history */
        if (agent2.history) {
            for (size_t i = 0; i < agent2.history_count; i++) {
                if (agent2.history[i].content)
                    alloc->free(alloc->ctx, agent2.history[i].content,
                               agent2.history[i].content_len + 1);
            }
            alloc->free(alloc->ctx, agent2.history,
                       agent2.history_cap * sizeof(hu_owned_message_t));
        }

        /* Delete the session file */
        (void)hu_session_persist_delete(tmpdir, session_id);
    }

    /* Free original agent history */
    for (size_t i = 0; i < actual; i++) {
        if (agent.history[i].content)
            alloc->free(alloc->ctx, agent.history[i].content,
                       agent.history[i].content_len + 1);
    }
    alloc->free(alloc->ctx, agent.history, msg_count * sizeof(hu_owned_message_t));

    rmdir(tmpdir);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 16384)
        return 0;

    hu_allocator_t alloc = hu_system_allocator();

    /* Strategy 1: Feed raw bytes as session JSON */
    fuzz_load_raw_json(&alloc, data, size);

    /* Strategy 2: Fuzz ID generation */
    fuzz_generate_id(data, size);

    /* Strategy 3: Save→load round-trip with fuzz messages */
    fuzz_save_load_roundtrip(&alloc, data, size);

    return 0;
}
