#include "human/security/history_scorer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool ci_prefix(const char *name, size_t nlen, const char *prefix) {
    size_t pl = strlen(prefix);
    if (nlen < pl)
        return false;
    for (size_t i = 0; i < pl; i++) {
        if (tolower((unsigned char)name[i]) != tolower((unsigned char)prefix[i]))
            return false;
    }
    return true;
}

static bool name_suggests_read(const char *n, size_t len) {
    return ci_prefix(n, len, "read") || ci_prefix(n, len, "file_read") ||
           ci_prefix(n, len, "get") || ci_prefix(n, len, "list") ||
           ci_prefix(n, len, "memory_recall");
}

static bool name_suggests_send(const char *n, size_t len) {
    return ci_prefix(n, len, "send") || ci_prefix(n, len, "broadcast") ||
           ci_prefix(n, len, "http") || ci_prefix(n, len, "web_fetch");
}

static bool name_suggests_modify(const char *n, size_t len) {
    return ci_prefix(n, len, "write") || ci_prefix(n, len, "edit") || ci_prefix(n, len, "update") ||
           ci_prefix(n, len, "delete") || ci_prefix(n, len, "shell");
}

hu_error_t hu_history_scorer_evaluate(const hu_tool_history_entry_t *history, size_t count,
                                      const char *proposed_tool, size_t tool_len,
                                      uint32_t proposed_risk, hu_history_score_result_t *out) {
    (void)proposed_tool;
    (void)tool_len;
    if (!out)
        return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));

    if (!history || count == 0)
        return HU_OK;

    double score = 0.0;

    /* Risk trend: each step where risk increases adds weight */
    uint32_t prev_risk = history[0].risk_level;
    for (size_t i = 1; i < count; i++) {
        if (history[i].risk_level > prev_risk && history[i].succeeded)
            score += 0.2;
        prev_risk = history[i].risk_level;
    }

    if (proposed_risk > prev_risk)
        score += 0.15;

    /* Data-flow heuristics: read → extract-like → send */
    for (size_t i = 0; i + 2 < count; i++) {
        const char *a = history[i].tool_name;
        const char *b = history[i + 1].tool_name;
        const char *c = history[i + 2].tool_name;
        size_t al = history[i].name_len;
        size_t bl = history[i + 1].name_len;
        size_t cl = history[i + 2].name_len;
        if (!a || !b || !c)
            continue;
        if (name_suggests_read(a, al) && name_suggests_read(b, bl) && name_suggests_send(c, cl) &&
            history[i].succeeded && history[i + 1].succeeded) {
            score += 0.3;
            if (out->pattern_len == 0) {
                int n = snprintf(out->pattern, sizeof(out->pattern), "read→read→send chain");
                if (n > 0)
                    out->pattern_len = (size_t)n;
            }
        }
    }

    /* list → read → modify escalation */
    for (size_t i = 0; i + 2 < count; i++) {
        const char *a = history[i].tool_name;
        const char *b = history[i + 1].tool_name;
        const char *c = history[i + 2].tool_name;
        size_t al = history[i].name_len;
        size_t bl = history[i + 1].name_len;
        size_t cl = history[i + 2].name_len;
        if (!a || !b || !c)
            continue;
        if (ci_prefix(a, al, "list") && name_suggests_read(b, bl) && name_suggests_modify(c, cl)) {
            score += 0.3;
            if (out->pattern_len == 0) {
                int n = snprintf(out->pattern, sizeof(out->pattern), "list→read→modify chain");
                if (n > 0)
                    out->pattern_len = (size_t)n;
            }
        }
    }

    if (score > 1.0)
        score = 1.0;
    out->escalation_score = score;
    out->is_suspicious = (score >= 0.6);
    return HU_OK;
}
