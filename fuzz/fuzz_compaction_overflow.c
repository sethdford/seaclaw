/*
 * Fuzz harness for hu_compact_build_structured_summary and
 * hu_compact_extract_metadata.
 * Must not crash, overflow, or leak on any input.
 *
 * RED-TEAM-2: fuzz target for compaction with random message arrays.
 */
#include "human/agent/compaction_structured.h"
#include "human/agent.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Construct synthetic messages from fuzz data.
 * Format: [count:1] [for each: role:1, content_len:2, content:N] */
static hu_owned_message_t *build_messages(hu_allocator_t *alloc, const uint8_t *data,
                                           size_t size, size_t *out_count) {
    if (size < 1) {
        *out_count = 0;
        return NULL;
    }

    size_t count = data[0] % 32; /* 0–31 messages */
    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    hu_owned_message_t *msgs = (hu_owned_message_t *)alloc->alloc(
        alloc->ctx, count * sizeof(hu_owned_message_t));
    if (!msgs) {
        *out_count = 0;
        return NULL;
    }
    memset(msgs, 0, count * sizeof(hu_owned_message_t));

    size_t offset = 1;
    size_t actual_count = 0;
    for (size_t i = 0; i < count && offset < size; i++) {
        /* Role byte */
        hu_role_t role = (hu_role_t)(data[offset] % 4);
        offset++;
        if (offset >= size) break;

        /* Content length (2 bytes, little-endian, max 500) */
        size_t clen = data[offset];
        offset++;
        if (offset < size) {
            clen |= (size_t)(data[offset] << 8);
            offset++;
        }
        if (clen > 500) clen = 500;
        if (offset + clen > size) clen = size - offset;

        msgs[i].role = role;
        if (clen > 0) {
            msgs[i].content = (char *)alloc->alloc(alloc->ctx, clen + 1);
            if (msgs[i].content) {
                memcpy(msgs[i].content, data + offset, clen);
                msgs[i].content[clen] = '\0';
                msgs[i].content_len = clen;
            }
        }
        offset += clen;

        /* Optionally add a tool_call if role is assistant and we have bytes */
        if (role == HU_ROLE_ASSISTANT && offset < size && (data[offset - 1] & 0x80)) {
            msgs[i].tool_calls = (hu_tool_call_t *)alloc->alloc(
                alloc->ctx, sizeof(hu_tool_call_t));
            if (msgs[i].tool_calls) {
                memset(msgs[i].tool_calls, 0, sizeof(hu_tool_call_t));
                msgs[i].tool_calls_count = 1;
                /* Use a few bytes as tool name */
                size_t nlen = 8;
                if (offset + nlen > size) nlen = size - offset;
                if (nlen > 0) {
                    msgs[i].tool_calls[0].name = (char *)alloc->alloc(alloc->ctx, nlen + 1);
                    if (msgs[i].tool_calls[0].name) {
                        memcpy(msgs[i].tool_calls[0].name, data + offset, nlen);
                        msgs[i].tool_calls[0].name[nlen] = '\0';
                        msgs[i].tool_calls[0].name_len = nlen;
                    }
                }
                offset += nlen;
            }
        }

        actual_count++;
    }

    *out_count = actual_count;
    return msgs;
}

static void free_messages(hu_allocator_t *alloc, hu_owned_message_t *msgs, size_t count) {
    if (!msgs) return;
    for (size_t i = 0; i < count; i++) {
        if (msgs[i].content)
            alloc->free(alloc->ctx, msgs[i].content, msgs[i].content_len + 1);
        if (msgs[i].tool_calls) {
            for (size_t t = 0; t < msgs[i].tool_calls_count; t++) {
                if (msgs[i].tool_calls[t].name)
                    alloc->free(alloc->ctx, msgs[i].tool_calls[t].name,
                                msgs[i].tool_calls[t].name_len + 1);
                if (msgs[i].tool_calls[t].id)
                    alloc->free(alloc->ctx, msgs[i].tool_calls[t].id,
                                msgs[i].tool_calls[t].id_len + 1);
                if (msgs[i].tool_calls[t].arguments)
                    alloc->free(alloc->ctx, msgs[i].tool_calls[t].arguments,
                                msgs[i].tool_calls[t].arguments_len + 1);
            }
            alloc->free(alloc->ctx, msgs[i].tool_calls,
                        msgs[i].tool_calls_count * sizeof(hu_tool_call_t));
        }
    }
    alloc->free(alloc->ctx, msgs, count * sizeof(hu_owned_message_t));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192)
        return 0;

    hu_allocator_t alloc = hu_system_allocator();

    size_t msg_count = 0;
    hu_owned_message_t *msgs = build_messages(&alloc, data, size, &msg_count);

    if (msgs && msg_count > 0) {
        /* ── Fuzz extract_metadata ── */
        hu_compaction_summary_t summary;
        hu_error_t err = hu_compact_extract_metadata(
            &alloc, msgs, msg_count, 2, &summary);
        if (err == HU_OK) {
            /* ── Fuzz build_structured_summary ── */
            char *xml = NULL;
            size_t xml_len = 0;
            err = hu_compact_build_structured_summary(
                &alloc, msgs, msg_count, &summary, &xml, &xml_len);
            if (err == HU_OK && xml) {
                alloc.free(alloc.ctx, xml, xml_len + 1);
            }
            hu_compaction_summary_free(&alloc, &summary);
        }

        /* ── Fuzz strip_analysis on message content ── */
        for (size_t i = 0; i < msg_count; i++) {
            if (msgs[i].content && msgs[i].content_len > 0) {
                /* Make a copy for in-place modification */
                char *copy = (char *)alloc.alloc(alloc.ctx, msgs[i].content_len + 1);
                if (copy) {
                    memcpy(copy, msgs[i].content, msgs[i].content_len + 1);
                    (void)hu_compact_strip_analysis(copy, msgs[i].content_len);
                    alloc.free(alloc.ctx, copy, msgs[i].content_len + 1);
                }
            }
        }

        /* ── Fuzz artifact detection ── */
        {
            hu_artifact_pin_t *pins = NULL;
            size_t pin_count = 0;
            err = hu_compact_detect_artifacts(
                &alloc, msgs, msg_count, "/tmp/fuzz", 9, &pins, &pin_count);
            if (err == HU_OK && pins) {
                hu_artifact_pins_free(&alloc, pins, pin_count);
            }
        }

        /* ── Fuzz is_pinned ── */
        {
            hu_artifact_pin_t fake_pin;
            memset(&fake_pin, 0, sizeof(fake_pin));
            fake_pin.file_path = "/tmp/fuzz/test.c";
            fake_pin.file_path_len = 16;
            for (size_t i = 0; i < msg_count; i++) {
                (void)hu_compact_is_pinned(&msgs[i], &fake_pin, 1);
            }
        }
    }

    free_messages(&alloc, msgs, msg_count);
    return 0;
}
