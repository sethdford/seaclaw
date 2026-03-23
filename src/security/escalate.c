#include "human/security/escalate.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

const char *hu_escalate_level_string(hu_escalate_level_t level) {
    switch (level) {
    case HU_ESCALATE_AUTO:    return "auto";
    case HU_ESCALATE_NOTIFY:  return "notify";
    case HU_ESCALATE_APPROVE: return "approve";
    case HU_ESCALATE_DENY:    return "deny";
    }
    return "unknown";
}

static hu_escalate_level_t parse_level(const char *s, size_t len) {
    if (!s || len == 0) return HU_ESCALATE_AUTO;
    /* Skip whitespace */
    while (len > 0 && (*s == ' ' || *s == '\t')) { s++; len--; }
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) len--;

    if (len == 4 && strncasecmp(s, "auto", 4) == 0) return HU_ESCALATE_AUTO;
    if (len == 6 && strncasecmp(s, "notify", 6) == 0) return HU_ESCALATE_NOTIFY;
    if (len == 7 && strncasecmp(s, "approve", 7) == 0) return HU_ESCALATE_APPROVE;
    if (len == 4 && strncasecmp(s, "deny", 4) == 0) return HU_ESCALATE_DENY;
    return HU_ESCALATE_AUTO;
}

static uint32_t parse_timeout(const char *s, size_t len) {
    if (!s || len == 0) return 0;
    while (len > 0 && *s == ' ') { s++; len--; }
    uint32_t val = 0;
    for (size_t i = 0; i < len && isdigit((unsigned char)s[i]); i++)
        val = val * 10 + (uint32_t)(s[i] - '0');
    return val;
}

static const char *find_next_pipe(const char *s, const char *end) {
    while (s < end && *s != '|') s++;
    return s < end ? s : NULL;
}

static bool is_separator_row(const char *line, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = line[i];
        if (c != '-' && c != '|' && c != ' ' && c != ':' && c != '\t')
            return false;
    }
    return len > 2;
}

hu_error_t hu_escalate_parse(const char *text, size_t text_len, hu_escalate_protocol_t *out) {
    if (!out) return HU_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->default_level = HU_ESCALATE_APPROVE;

    if (!text || text_len == 0) return HU_OK;

    const char *pos = text;
    const char *end = text + text_len;
    bool in_table = false;
    bool past_header = false;

    while (pos < end) {
        const char *line_end = pos;
        while (line_end < end && *line_end != '\n') line_end++;
        size_t line_len = (size_t)(line_end - pos);

        /* Skip empty lines */
        if (line_len == 0) {
            pos = line_end + 1;
            continue;
        }

        /* Detect table start: line contains '|' */
        if (!in_table) {
            if (memchr(pos, '|', line_len)) {
                in_table = true;
                past_header = false;
                pos = line_end + (line_end < end ? 1 : 0);
                continue;
            }
            pos = line_end + (line_end < end ? 1 : 0);
            continue;
        }

        /* Skip separator rows (---|---) */
        if (is_separator_row(pos, line_len)) {
            past_header = true;
            pos = line_end + (line_end < end ? 1 : 0);
            continue;
        }

        /* Not past header row yet — skip the header */
        if (!past_header) {
            pos = line_end + (line_end < end ? 1 : 0);
            continue;
        }

        /* End of table: no '|' on this line */
        if (!memchr(pos, '|', line_len)) {
            in_table = false;
            pos = line_end + (line_end < end ? 1 : 0);
            continue;
        }

        /* Parse table row: | Action | Level | Timeout | Channel | */
        if (out->rule_count >= HU_ESCALATE_MAX_RULES)
            break;

        const char *p = pos;
        const char *le = pos + line_len;
        /* Skip leading '|' */
        if (*p == '|') p++;

        /* Column 1: Action */
        const char *col1_start = p;
        const char *pipe = find_next_pipe(p, le);
        if (!pipe) goto next_line;
        size_t col1_len = (size_t)(pipe - col1_start);
        while (col1_len > 0 && col1_start[0] == ' ') { col1_start++; col1_len--; }
        while (col1_len > 0 && col1_start[col1_len - 1] == ' ') col1_len--;

        /* Column 2: Level */
        p = pipe + 1;
        const char *col2_start = p;
        pipe = find_next_pipe(p, le);
        if (!pipe) goto next_line;
        size_t col2_len = (size_t)(pipe - col2_start);

        /* Column 3: Timeout */
        p = pipe + 1;
        const char *col3_start = p;
        pipe = find_next_pipe(p, le);
        size_t col3_len = pipe ? (size_t)(pipe - col3_start) : (size_t)(le - col3_start);

        /* Column 4: Channel (optional) */
        const char *col4_start = NULL;
        size_t col4_len = 0;
        if (pipe) {
            p = pipe + 1;
            col4_start = p;
            const char *pipe2 = find_next_pipe(p, le);
            col4_len = pipe2 ? (size_t)(pipe2 - col4_start) : (size_t)(le - col4_start);
        }

        hu_escalate_rule_t *rule = &out->rules[out->rule_count];
        memset(rule, 0, sizeof(*rule));

        size_t copy_len = col1_len < sizeof(rule->action_pattern) - 1
                              ? col1_len : sizeof(rule->action_pattern) - 1;
        memcpy(rule->action_pattern, col1_start, copy_len);
        rule->action_pattern[copy_len] = '\0';

        rule->level = parse_level(col2_start, col2_len);
        rule->timeout_s = parse_timeout(col3_start, col3_len);

        if (col4_start && col4_len > 0) {
            while (col4_len > 0 && *col4_start == ' ') { col4_start++; col4_len--; }
            while (col4_len > 0 && col4_start[col4_len - 1] == ' ') col4_len--;
            size_t ch_len = col4_len < sizeof(rule->notify_channel) - 1
                                ? col4_len : sizeof(rule->notify_channel) - 1;
            memcpy(rule->notify_channel, col4_start, ch_len);
            rule->notify_channel[ch_len] = '\0';
        }

        out->rule_count++;

next_line:
        pos = line_end + (line_end < end ? 1 : 0);
    }

    return HU_OK;
}

/* Simple glob matching: supports '*' wildcard */
static bool glob_match(const char *pattern, const char *str) {
    while (*pattern && *str) {
        if (*pattern == '*') {
            pattern++;
            if (*pattern == '\0') return true;
            while (*str) {
                if (glob_match(pattern, str)) return true;
                str++;
            }
            return *pattern == '\0';
        }
        if (*pattern != *str) return false;
        pattern++;
        str++;
    }
    while (*pattern == '*') pattern++;
    return *pattern == '\0' && *str == '\0';
}

hu_escalate_level_t hu_escalate_evaluate(const hu_escalate_protocol_t *protocol,
                                         const char *action_name, size_t action_name_len) {
    if (!protocol || !action_name || action_name_len == 0)
        return HU_ESCALATE_APPROVE;

    char name_buf[256];
    size_t copy = action_name_len < sizeof(name_buf) - 1 ? action_name_len : sizeof(name_buf) - 1;
    memcpy(name_buf, action_name, copy);
    name_buf[copy] = '\0';

    for (size_t i = 0; i < protocol->rule_count; i++) {
        if (glob_match(protocol->rules[i].action_pattern, name_buf))
            return protocol->rules[i].level;
    }

    return protocol->default_level;
}

hu_error_t hu_escalate_log_decision(hu_audit_logger_t *logger,
                                    const char *action_name,
                                    hu_escalate_level_t level,
                                    bool approved) {
    if (!logger || !action_name)
        return HU_ERR_INVALID_ARGUMENT;

    hu_audit_event_t ev;
    hu_audit_event_init(&ev, HU_AUDIT_COMMAND_EXECUTION);
    hu_audit_event_with_action(&ev, action_name,
                               hu_escalate_level_string(level), approved, approved);
    hu_audit_event_with_reasoning(&ev, approved ? "escalate_approved" : "escalate_denied",
                                  hu_escalate_level_string(level), -1.0f, 0);
    return hu_audit_logger_log(logger, &ev);
}

size_t hu_escalate_generate_report(const hu_escalate_report_t *report, char *buf, size_t buf_size) {
    if (!report || !buf || buf_size == 0)
        return 0;

    int n = snprintf(buf, buf_size,
                     "ESCALATE Compliance Report\n"
                     "=========================\n"
                     "Total decisions: %zu\n"
                     "  Auto:    %zu\n"
                     "  Notify:  %zu\n"
                     "  Approve: %zu (approved: %zu, denied: %zu)\n"
                     "  Deny:    %zu\n",
                     report->total_decisions,
                     report->auto_count,
                     report->notify_count,
                     report->approve_count, report->approved_count, report->denied_count,
                     report->deny_count);
    if (n < 0) return 0;
    return (size_t)n < buf_size ? (size_t)n : buf_size - 1;
}
