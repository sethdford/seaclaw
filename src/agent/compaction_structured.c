#include "human/agent/compaction_structured.h"
#include "human/core/string.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ── Helpers ─────────────────────────────────────────────────────────── */

#define HU_COMPACT_PATH_MAX 4096

/* Append src to *buf at *pos, growing if needed. Returns false on OOM. */
static bool buf_append(hu_allocator_t *alloc, char **buf, size_t *cap, size_t *pos,
                       const char *src, size_t src_len) {
    while (*pos + src_len + 1 > *cap) {
        size_t new_cap;
        if (*cap > SIZE_MAX / 2) {
            new_cap = *pos + src_len + 1;
            if (new_cap < *pos || new_cap < src_len)
                return false;
        } else {
            new_cap = *cap * 2;
            if (new_cap < *pos + src_len + 1)
                new_cap = *pos + src_len + 1;
            if (new_cap < *pos || new_cap < src_len)
                return false;
        }
        char *nb = (char *)alloc->realloc(alloc->ctx, *buf, *cap, new_cap);
        if (!nb)
            return false;
        *buf = nb;
        *cap = new_cap;
    }
    memcpy(*buf + *pos, src, src_len);
    *pos += src_len;
    (*buf)[*pos] = '\0';
    return true;
}

static bool buf_append_str(hu_allocator_t *alloc, char **buf, size_t *cap, size_t *pos,
                           const char *s) {
    return buf_append(alloc, buf, cap, pos, s, strlen(s));
}

/* XML-escape a string into buf. */
static bool buf_append_xml_escaped(hu_allocator_t *alloc, char **buf, size_t *cap, size_t *pos,
                                   const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        const char *esc = NULL;
        switch (s[i]) {
        case '&': esc = "&amp;"; break;
        case '<': esc = "&lt;"; break;
        case '>': esc = "&gt;"; break;
        case '"': esc = "&quot;"; break;
        default: break;
        }
        if (esc) {
            if (!buf_append_str(alloc, buf, cap, pos, esc))
                return false;
        } else {
            if (!buf_append(alloc, buf, cap, pos, &s[i], 1))
                return false;
        }
    }
    return true;
}

/* ── Strip analysis blocks ───────────────────────────────────────────── */

size_t hu_compact_strip_analysis(char *content, size_t len) {
    if (!content || len == 0)
        return 0;

    static const char open_tag[] = "<analysis>";
    static const char close_tag[] = "</analysis>";
    const size_t open_len = sizeof(open_tag) - 1;
    const size_t close_len = sizeof(close_tag) - 1;

    size_t write_pos = 0;
    size_t read_pos = 0;

    while (read_pos < len) {
        /* Look for <analysis> */
        char *found = NULL;
        if (read_pos + open_len <= len) {
            found = (char *)memmem(content + read_pos, len - read_pos, open_tag, open_len);
        }
        if (!found) {
            /* No more analysis blocks; copy rest */
            size_t remain = len - read_pos;
            if (write_pos != read_pos)
                memmove(content + write_pos, content + read_pos, remain);
            write_pos += remain;
            break;
        }

        /* Copy everything before the tag */
        size_t before = (size_t)(found - (content + read_pos));
        if (before > 0 && write_pos != read_pos)
            memmove(content + write_pos, content + read_pos, before);
        write_pos += before;
        read_pos = (size_t)(found - content) + open_len;

        /* Find closing tag */
        char *end = NULL;
        if (read_pos + close_len <= len) {
            end = (char *)memmem(content + read_pos, len - read_pos, close_tag, close_len);
        }
        if (end) {
            read_pos = (size_t)(end - content) + close_len;
        } else {
            /* No closing tag — strip to end */
            break;
        }
    }

    content[write_pos] = '\0';
    return write_pos;
}

/* ── Free helpers ────────────────────────────────────────────────────── */

void hu_compaction_summary_free(hu_allocator_t *alloc, hu_compaction_summary_t *s) {
    if (!alloc || !s)
        return;
    if (s->tool_mentions) {
        for (size_t i = 0; i < s->tool_mentions_count; i++) {
            if (s->tool_mentions[i])
                alloc->free(alloc->ctx, s->tool_mentions[i], strlen(s->tool_mentions[i]) + 1);
        }
        alloc->free(alloc->ctx, s->tool_mentions, s->tool_mentions_count * sizeof(char *));
        s->tool_mentions = NULL;
        s->tool_mentions_count = 0;
    }
    if (s->recent_user_requests) {
        alloc->free(alloc->ctx, s->recent_user_requests, s->recent_user_requests_len + 1);
        s->recent_user_requests = NULL;
        s->recent_user_requests_len = 0;
    }
    if (s->pending_work_inference) {
        alloc->free(alloc->ctx, s->pending_work_inference, s->pending_work_inference_len + 1);
        s->pending_work_inference = NULL;
        s->pending_work_inference_len = 0;
    }
}

void hu_artifact_pins_free(hu_allocator_t *alloc, hu_artifact_pin_t *pins, size_t count) {
    if (!alloc || !pins)
        return;
    for (size_t i = 0; i < count; i++) {
        if (pins[i].file_path)
            alloc->free(alloc->ctx, pins[i].file_path, pins[i].file_path_len + 1);
    }
    alloc->free(alloc->ctx, pins, count * sizeof(hu_artifact_pin_t));
}

/* ── Metadata extraction ─────────────────────────────────────────────── */

/* Check if tool_name is already in the mentions list. */
static bool tool_already_mentioned(char **mentions, size_t count, const char *name, size_t nlen) {
    for (size_t i = 0; i < count; i++) {
        if (strlen(mentions[i]) == nlen && memcmp(mentions[i], name, nlen) == 0)
            return true;
    }
    return false;
}

hu_error_t hu_compact_extract_metadata(
    hu_allocator_t *alloc,
    const hu_owned_message_t *messages, size_t count,
    uint32_t preserve_recent,
    hu_compaction_summary_t *out) {

    if (!alloc || !messages || !out)
        return HU_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));
    out->total_messages = (uint32_t)count;
    out->preserved_count = preserve_recent;

    bool has_system = count > 0 && messages[0].role == HU_ROLE_SYSTEM;
    size_t start = has_system ? 1 : 0;
    size_t non_system = count - start;
    uint32_t keep = preserve_recent;
    if (keep > (uint32_t)non_system) keep = (uint32_t)non_system;
    out->summarized_count = (uint32_t)(non_system - keep);

    /* Collect tool mentions from assistant messages with tool_calls */
    char **tools = NULL;
    size_t tools_count = 0;
    size_t tools_cap = 0;

    for (size_t i = 0; i < count; i++) {
        const hu_owned_message_t *m = &messages[i];
        if (m->tool_calls && m->tool_calls_count > 0) {
            for (size_t t = 0; t < m->tool_calls_count; t++) {
                const char *tname = m->tool_calls[t].name;
                size_t tlen = m->tool_calls[t].name_len;
                if (!tname || tlen == 0) continue;
                if (tools_count >= HU_MAX_TOOL_MENTIONS) break;
                if (tool_already_mentioned(tools, tools_count, tname, tlen)) continue;

                if (tools_count >= tools_cap) {
                    size_t new_cap = tools_cap == 0 ? 8 : tools_cap * 2;
                    if (new_cap > HU_MAX_TOOL_MENTIONS) new_cap = HU_MAX_TOOL_MENTIONS;
                    char **nt = (char **)alloc->realloc(alloc->ctx, tools,
                        tools_cap * sizeof(char *), new_cap * sizeof(char *));
                    if (!nt) goto oom;
                    tools = nt;
                    tools_cap = new_cap;
                }
                tools[tools_count] = hu_strndup(alloc, tname, tlen);
                if (!tools[tools_count]) goto oom;
                tools_count++;
            }
        }
    }

    out->tool_mentions = tools;
    out->tool_mentions_count = tools_count;

    /* Collect last N user messages as recent_user_requests */
    size_t user_indices[HU_MAX_RECENT_REQUESTS];
    size_t user_found = 0;
    for (size_t i = count; i > 0 && user_found < HU_MAX_RECENT_REQUESTS; i--) {
        if (messages[i - 1].role == HU_ROLE_USER && messages[i - 1].content) {
            user_indices[user_found++] = i - 1;
        }
    }

    if (user_found > 0) {
        /* Concatenate in chronological order */
        size_t total_len = 0;
        for (size_t i = user_found; i > 0; i--) {
            size_t idx = user_indices[i - 1];
            size_t clen = messages[idx].content_len;
            if (clen > 200) clen = 200; /* truncate each */
            total_len += clen + 1; /* +1 for newline */
        }

        char *reqs = (char *)alloc->alloc(alloc->ctx, total_len + 1);
        if (!reqs) goto oom;
        size_t pos = 0;
        for (size_t i = user_found; i > 0; i--) {
            size_t idx = user_indices[i - 1];
            size_t clen = messages[idx].content_len;
            if (clen > 200) clen = 200;
            memcpy(reqs + pos, messages[idx].content, clen);
            pos += clen;
            reqs[pos++] = '\n';
        }
        reqs[pos] = '\0';
        out->recent_user_requests = reqs;
        out->recent_user_requests_len = pos;
    }

    /* Pending work inference: look for patterns in last assistant message */
    for (size_t i = count; i > 0; i--) {
        if (messages[i - 1].role == HU_ROLE_ASSISTANT && messages[i - 1].content) {
            const char *c = messages[i - 1].content;
            size_t clen = messages[i - 1].content_len;
            /* Simple heuristic: pending-work cues (TODO only at word boundary) */
            bool todo_wb = false;
            {
                const char *todo_hit = (const char *)memmem(c, clen, "TODO", 4);
                if (todo_hit) {
                    size_t off = (size_t)(todo_hit - c);
                    bool left_ok =
                        (off == 0 || !isalpha((unsigned char)todo_hit[-1]));
                    bool right_ok =
                        (off + 4 >= clen || !isalpha((unsigned char)todo_hit[4]));
                    todo_wb = left_ok && right_ok;
                }
            }
            if (todo_wb || memmem(c, clen, "next step", 9) ||
                memmem(c, clen, "need to", 7) || memmem(c, clen, "will ", 5)) {
                size_t plen = clen > 300 ? 300 : clen;
                out->pending_work_inference = hu_strndup(alloc, c, plen);
                if (!out->pending_work_inference) goto oom;
                out->pending_work_inference_len = plen;
            }
            break;
        }
    }

    return HU_OK;

oom:
    if (tools) {
        for (size_t i = 0; i < tools_count; i++) {
            if (tools[i]) alloc->free(alloc->ctx, tools[i], strlen(tools[i]) + 1);
        }
        alloc->free(alloc->ctx, tools, tools_cap * sizeof(char *));
    }
    out->tool_mentions = NULL;
    out->tool_mentions_count = 0;
    hu_compaction_summary_free(alloc, out);
    return HU_ERR_OUT_OF_MEMORY;
}

/* ── Structured summary XML generation ───────────────────────────────── */

hu_error_t hu_compact_build_structured_summary(
    hu_allocator_t *alloc,
    const hu_owned_message_t *messages, size_t count,
    const hu_compaction_summary_t *metadata,
    char **out_xml, size_t *out_xml_len) {

    if (!alloc || !metadata || !out_xml || !out_xml_len)
        return HU_ERR_INVALID_ARGUMENT;

    size_t cap = 2048;
    char *buf = (char *)alloc->alloc(alloc->ctx, cap);
    if (!buf) return HU_ERR_OUT_OF_MEMORY;
    size_t pos = 0;
    buf[0] = '\0';

#define APPEND(s) do { if (!buf_append_str(alloc, &buf, &cap, &pos, (s))) goto oom; } while(0)
#define APPEND_N(s, n) do { if (!buf_append(alloc, &buf, &cap, &pos, (s), (n))) goto oom; } while(0)
#define APPEND_ESC(s, n) do { if (!buf_append_xml_escaped(alloc, &buf, &cap, &pos, (s), (n))) goto oom; } while(0)

    APPEND("<summary>\n");

    /* Message counts */
    {
        char tmp[128];
        int n = snprintf(tmp, sizeof(tmp),
            "  <stats total=\"%u\" summarized=\"%u\" preserved=\"%u\" />\n",
            metadata->total_messages, metadata->summarized_count, metadata->preserved_count);
        if (n > 0) APPEND_N(tmp, (size_t)n);
    }

    /* Tool mentions */
    if (metadata->tool_mentions_count > 0) {
        APPEND("  <tools_used>\n");
        for (size_t i = 0; i < metadata->tool_mentions_count; i++) {
            APPEND("    <tool>");
            APPEND_ESC(metadata->tool_mentions[i], strlen(metadata->tool_mentions[i]));
            APPEND("</tool>\n");
        }
        APPEND("  </tools_used>\n");
    }

    /* Recent user requests */
    if (metadata->recent_user_requests && metadata->recent_user_requests_len > 0) {
        APPEND("  <recent_requests>\n");
        /* Split by newline and emit each as a <request> */
        const char *p = metadata->recent_user_requests;
        const char *end = p + metadata->recent_user_requests_len;
        while (p < end) {
            const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
            size_t line_len = nl ? (size_t)(nl - p) : (size_t)(end - p);
            if (line_len > 0) {
                APPEND("    <request>");
                APPEND_ESC(p, line_len);
                APPEND("</request>\n");
            }
            p += line_len + 1;
        }
        APPEND("  </recent_requests>\n");
    }

    /* Pending work */
    if (metadata->pending_work_inference && metadata->pending_work_inference_len > 0) {
        APPEND("  <pending_work>");
        APPEND_ESC(metadata->pending_work_inference, metadata->pending_work_inference_len);
        APPEND("</pending_work>\n");
    }

    /* Conversation content summary (stripped of analysis blocks) */
    if (messages && count > 0) {
        APPEND("  <conversation>\n");
        for (size_t i = 0; i < count; i++) {
            if (!messages[i].content || messages[i].content_len == 0) continue;

            /* Make a copy so we can strip analysis blocks */
            size_t clen = messages[i].content_len;
            if (clen > 500) clen = 500; /* truncate per message */
            char *copy = hu_strndup(alloc, messages[i].content, clen);
            if (!copy) goto oom;
            size_t new_len = hu_compact_strip_analysis(copy, clen);

            const char *role_name = "unknown";
            /* NB: duplicated role→string mapping also exists in compaction.c:role_str */
            switch (messages[i].role) {
            case HU_ROLE_SYSTEM: role_name = "system"; break;
            case HU_ROLE_USER: role_name = "user"; break;
            case HU_ROLE_ASSISTANT: role_name = "assistant"; break;
            case HU_ROLE_TOOL: role_name = "tool"; break;
            default: break;
            }

            APPEND("    <message role=\"");
            APPEND(role_name);
            APPEND("\">");
            APPEND_ESC(copy, new_len);
            APPEND("</message>\n");

            alloc->free(alloc->ctx, copy, clen + 1);
        }
        APPEND("  </conversation>\n");
    }

    APPEND("</summary>");

#undef APPEND
#undef APPEND_N
#undef APPEND_ESC

    *out_xml = buf;
    *out_xml_len = pos;
    return HU_OK;

oom:
    alloc->free(alloc->ctx, buf, cap);
    return HU_ERR_OUT_OF_MEMORY;
}

/* ── Artifact detection ──────────────────────────────────────────────── */

/* Simple heuristic: look for file path patterns (containing / and a file extension)
 * that start with workspace_dir or common relative paths. */
static bool find_file_ref(const char *content, size_t content_len,
                          const char *workspace_dir, size_t ws_len,
                          const char **out_start, size_t *out_len) {
    /* Look for workspace_dir prefix in content */
    if (ws_len == 0 || !content)
        return false;

    const char *found = (const char *)memmem(content, content_len, workspace_dir, ws_len);
    if (!found)
        return false;

    /* Extend to end of path (until whitespace, quote, or end) */
    const char *path_start = found;
    const char *end = content + content_len;
    const char *path_end = found + ws_len;
    while (path_end < end && *path_end != ' ' && *path_end != '\n' &&
           *path_end != '\t' && *path_end != '"' && *path_end != '\'' &&
           *path_end != ')' && *path_end != '>' && *path_end != '`') {
        path_end++;
    }

    size_t plen = (size_t)(path_end - path_start);
    if (plen <= ws_len)
        return false; /* just the dir itself, no file */

    *out_start = path_start;
    *out_len = plen;
    return true;
}

hu_error_t hu_compact_detect_artifacts(
    hu_allocator_t *alloc,
    const hu_owned_message_t *messages, size_t count,
    const char *workspace_dir, size_t workspace_dir_len,
    hu_artifact_pin_t **out_pins, size_t *out_count) {

    if (!alloc || !out_pins || !out_count)
        return HU_ERR_INVALID_ARGUMENT;

    *out_pins = NULL;
    *out_count = 0;

    if (!messages || count == 0 || !workspace_dir || workspace_dir_len == 0)
        return HU_OK;

    hu_artifact_pin_t *pins = NULL;
    size_t pin_count = 0;
    size_t pin_cap = 0;

    for (size_t i = 0; i < count && pin_count < HU_MAX_ARTIFACT_PINS; i++) {
        const char *content = messages[i].content;
        size_t content_len = messages[i].content_len;
        if (!content || content_len == 0) continue;

        const char *search_pos = content;
        size_t search_len = content_len;

        while (pin_count < HU_MAX_ARTIFACT_PINS) {
            const char *ref_start = NULL;
            size_t ref_len = 0;
            if (!find_file_ref(search_pos, search_len, workspace_dir, workspace_dir_len,
                               &ref_start, &ref_len))
                break;

            /* Check for duplicates */
            bool dup = false;
            for (size_t j = 0; j < pin_count; j++) {
                if (pins[j].file_path_len == ref_len &&
                    memcmp(pins[j].file_path, ref_start, ref_len) == 0) {
                    dup = true;
                    break;
                }
            }

            if (!dup) {
                /* Verify file exists before pinning */
                char candidate_path[HU_COMPACT_PATH_MAX];
                if (ref_len >= HU_COMPACT_PATH_MAX) {
                    /* Path too long, skip */
                } else {
                    memcpy(candidate_path, ref_start, ref_len);
                    candidate_path[ref_len] = '\0';

#ifndef HU_IS_TEST
                    struct stat st;
                    if (stat(candidate_path, &st) != 0) {
                        /* File doesn't exist — skip this artifact */
                    } else
#endif
                    {
                        /* File exists — add to pins */
                        if (pin_count >= pin_cap) {
                            size_t new_cap = pin_cap == 0 ? 8 : pin_cap * 2;
                            if (new_cap > HU_MAX_ARTIFACT_PINS) new_cap = HU_MAX_ARTIFACT_PINS;
                            hu_artifact_pin_t *np = (hu_artifact_pin_t *)alloc->realloc(
                                alloc->ctx, pins,
                                pin_cap * sizeof(hu_artifact_pin_t),
                                new_cap * sizeof(hu_artifact_pin_t));
                            if (!np) goto oom;
                            pins = np;
                            pin_cap = new_cap;
                        }
                        pins[pin_count].file_path = hu_strndup(alloc, ref_start, ref_len);
                        if (!pins[pin_count].file_path) goto oom;
                        pins[pin_count].file_path_len = ref_len;
#ifndef HU_IS_TEST
                        pins[pin_count].last_modified_ts = (uint64_t)st.st_mtime;
#else
                        pins[pin_count].last_modified_ts = 0;
#endif
                        pin_count++;
                    }
                }
            }

            /* Advance past this reference */
            size_t offset = (size_t)(ref_start - search_pos) + ref_len;
            search_pos += offset;
            search_len -= offset;
        }
    }

    *out_pins = pins;
    *out_count = pin_count;
    return HU_OK;

oom:
    hu_artifact_pins_free(alloc, pins, pin_count);
    return HU_ERR_OUT_OF_MEMORY;
}

/* ── Artifact pinning check ──────────────────────────────────────────── */

bool hu_compact_is_pinned(
    const hu_owned_message_t *msg,
    const hu_artifact_pin_t *pins, size_t pin_count) {

    if (!msg || !msg->content || msg->content_len == 0 || !pins || pin_count == 0)
        return false;

    for (size_t i = 0; i < pin_count; i++) {
        if (pins[i].file_path && pins[i].file_path_len > 0) {
            if (memmem(msg->content, msg->content_len,
                       pins[i].file_path, pins[i].file_path_len)) {
                return true;
            }
        }
    }
    return false;
}

/* ── Continuation preamble injection ─────────────────────────────────── */

hu_error_t hu_compact_inject_continuation_preamble(
    hu_allocator_t *alloc,
    const hu_compaction_summary_t *summary,
    hu_owned_message_t **history, size_t *history_count, size_t *history_cap) {

    if (!alloc || !summary || !history || !*history || !history_count || !history_cap)
        return HU_ERR_INVALID_ARGUMENT;

    hu_owned_message_t *h = *history;
    size_t count = *history_count;
    size_t cap = *history_cap;

    /* Build preamble content */
    const char *base = HU_CONTINUATION_PREAMBLE;
    size_t base_len = strlen(base);

    /* Add context about what was summarized */
    char stats[256];
    int stats_len = snprintf(stats, sizeof(stats),
        "\n\nPrevious context: %u messages (%u summarized, %u preserved).",
        summary->total_messages, summary->summarized_count, summary->preserved_count);
    if (stats_len < 0) stats_len = 0;

    size_t total_len = base_len + (size_t)stats_len;
    char *content = (char *)alloc->alloc(alloc->ctx, total_len + 1);
    if (!content) return HU_ERR_OUT_OF_MEMORY;
    memcpy(content, base, base_len);
    if (stats_len > 0)
        memcpy(content + base_len, stats, (size_t)stats_len);
    content[total_len] = '\0';

    /* Insert after system prompt (index 1) or at index 0 */
    size_t insert_idx = (count > 0 && h[0].role == HU_ROLE_SYSTEM) ? 1 : 0;

    /* Ensure capacity */
    if (count + 1 > cap) {
        size_t new_cap = cap == 0 ? 4 : cap * 2;
        hu_owned_message_t *nh = (hu_owned_message_t *)alloc->realloc(
            alloc->ctx, h, cap * sizeof(hu_owned_message_t),
            new_cap * sizeof(hu_owned_message_t));
        if (!nh) {
            alloc->free(alloc->ctx, content, total_len + 1);
            return HU_ERR_OUT_OF_MEMORY;
        }
        h = nh;
        *history = nh;
        *history_cap = new_cap;
        cap = new_cap;
    }

    /* Shift messages to make room */
    if (insert_idx < count) {
        memmove(&h[insert_idx + 1], &h[insert_idx],
                (count - insert_idx) * sizeof(hu_owned_message_t));
    }

    /* Insert preamble as user message */
    memset(&h[insert_idx], 0, sizeof(hu_owned_message_t));
    h[insert_idx].role = HU_ROLE_USER;
    h[insert_idx].content = content;
    h[insert_idx].content_len = total_len;

    *history_count = count + 1;
    return HU_OK;
}
