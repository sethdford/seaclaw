#include "channel_harness.h"
#include "seaclaw/core/json.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char CONV_PROMPT[] =
    "You are a test case generator for messaging channel integration testing.\n"
    "Generate %d conversation scenarios for the channel '%s'.\n\n"
    "Return a JSON array where each element has:\n"
    "- \"session_key\": a realistic sender ID for this channel type (phone number, username, "
    "email)\n"
    "- \"turns\": array of objects with:\n"
    "  - \"user\": the message the user sends (realistic, varied)\n"
    "  - \"expect\": a pipe-separated regex pattern the response should match\n\n"
    "Scenarios should include:\n"
    "- Simple greetings and questions\n"
    "- Multi-turn follow-ups that reference previous context\n"
    "- Tool-triggering requests (weather, calculations)\n"
    "- Edge cases (very short messages, questions, emoji-only)\n\n"
    "Each scenario should have 2-4 turns.\n"
    "Return ONLY a JSON array, no markdown.";

static bool match_pattern(const char *text, const char *pattern) {
    if (!text || !pattern || !pattern[0])
        return true;
    /* Simple pipe-separated substring match */
    char buf[512];
    size_t plen = strlen(pattern);
    if (plen >= sizeof(buf))
        plen = sizeof(buf) - 1;
    memcpy(buf, pattern, plen);
    buf[plen] = '\0';
    char *tok = strtok(buf, "|");
    while (tok) {
        /* Case-insensitive substring search */
        const char *t = text;
        size_t tlen = strlen(tok);
        while (*t) {
            if (strncasecmp(t, tok, tlen) == 0)
                return true;
            t++;
        }
        tok = strtok(NULL, "|");
    }
    return false;
}

static sc_error_t run_scenario(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                               const sc_channel_test_entry_t *entry, sc_json_value_t *scenario,
                               sc_synth_metrics_t *metrics) {
    const char *session_key = sc_json_get_string(scenario, "session_key");
    if (!session_key)
        session_key = "test_user";
    size_t sk_len = strlen(session_key);

    sc_json_value_t *turns = sc_json_object_get(scenario, "turns");
    if (!turns || turns->type != SC_JSON_ARRAY || turns->data.array.len == 0)
        return SC_OK;

    sc_channel_t ch = {0};
    sc_error_t err = entry->create(alloc, &ch);
    if (err != SC_OK) {
        SC_CH_LOG("failed to create channel %s: %s", entry->name, sc_error_string(err));
        sc_synth_metrics_record(alloc, metrics, 0, SC_SYNTH_ERROR);
        return err;
    }

    for (size_t i = 0; i < turns->data.array.len; i++) {
        sc_json_value_t *turn = turns->data.array.items[i];
        const char *user_msg = sc_json_get_string(turn, "user");
        const char *expect = sc_json_get_string(turn, "expect");
        if (!user_msg)
            continue;

        double t0 = sc_synth_now_ms();

        /* Inject user message */
        err = entry->inject(&ch, session_key, sk_len, user_msg, strlen(user_msg));
        if (err != SC_OK) {
            sc_synth_metrics_record(alloc, metrics, sc_synth_now_ms() - t0, SC_SYNTH_ERROR);
            SC_CH_LOG("inject failed on %s: %s", entry->name, sc_error_string(err));
            continue;
        }

        /* Poll to verify injection */
        if (entry->poll) {
            sc_channel_loop_msg_t msgs[16];
            size_t count = 0;
            err = entry->poll(ch.ctx, alloc, msgs, 16, &count);
            if (err != SC_OK || count == 0) {
                sc_synth_metrics_record(alloc, metrics, sc_synth_now_ms() - t0, SC_SYNTH_FAIL);
                SC_CH_LOG("poll failed on %s turn %zu: err=%s count=%zu", entry->name, i,
                          sc_error_string(err), count);
                if (cfg->regression_dir) {
                    char reason[256];
                    snprintf(reason, sizeof(reason), "poll returned count=%zu", count);
                    sc_synth_test_case_t tc = {
                        .name = (char *)entry->name,
                        .category = (char *)"conversation",
                        .input_json = (char *)user_msg,
                        .verdict = SC_SYNTH_FAIL,
                        .verdict_reason = reason,
                    };
                    sc_synth_regression_save(alloc, cfg->regression_dir, &tc);
                }
                continue;
            }
        }

        /* Simulate agent response via channel send */
        const char *response = "This is a test response from the agent.";
        size_t resp_len = strlen(response);
        if (ch.vtable && ch.vtable->send) {
            err = ch.vtable->send(ch.ctx, session_key, sk_len, response, resp_len, NULL, 0);
        }

        /* Capture what was sent and verify pattern */
        if (entry->get_last) {
            size_t last_len = 0;
            const char *last = entry->get_last(&ch, &last_len);
            if (last && last_len > 0) {
                double lat = sc_synth_now_ms() - t0;
                sc_synth_verdict_t v =
                    match_pattern(last, expect ? expect : "") ? SC_SYNTH_PASS : SC_SYNTH_FAIL;
                if (v == SC_SYNTH_FAIL && cfg->regression_dir) {
                    char reason[256];
                    snprintf(reason, sizeof(reason), "response did not match expect pattern");
                    sc_synth_test_case_t tc = {
                        .name = (char *)entry->name,
                        .category = (char *)"conversation",
                        .input_json = (char *)user_msg,
                        .verdict = SC_SYNTH_FAIL,
                        .verdict_reason = reason,
                    };
                    sc_synth_regression_save(alloc, cfg->regression_dir, &tc);
                }
                SC_CH_VERBOSE(cfg, "%s %s turn %zu: injected '%.*s', sent '%.*s' (%.1fms)",
                              v == SC_SYNTH_PASS ? "PASS" : "FAIL", entry->name, i,
                              (int)(strlen(user_msg) > 40 ? 40 : strlen(user_msg)), user_msg,
                              (int)(last_len > 40 ? 40 : last_len), last, lat);
                sc_synth_metrics_record(alloc, metrics, lat, v);
            } else {
                sc_synth_metrics_record(alloc, metrics, sc_synth_now_ms() - t0, SC_SYNTH_FAIL);
                SC_CH_LOG("get_last returned NULL on %s turn %zu", entry->name, i);
            }
        } else {
            sc_synth_metrics_record(alloc, metrics, sc_synth_now_ms() - t0, SC_SYNTH_PASS);
        }
    }

    entry->destroy(&ch);
    return SC_OK;
}

sc_error_t sc_channel_run_conversations(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                                        sc_synth_gemini_ctx_t *gemini,
                                        sc_synth_metrics_t *metrics) {
    sc_synth_metrics_init(metrics);
    size_t reg_count = 0;
    const sc_channel_test_entry_t *reg = sc_channel_test_registry(&reg_count);

    for (size_t ci = 0; ci < reg_count; ci++) {
        const sc_channel_test_entry_t *entry = &reg[ci];

        /* Filter by channel list */
        if (!cfg->all_channels) {
            bool found = false;
            for (size_t j = 0; j < cfg->channel_count; j++) {
                if (strcmp(cfg->channels[j], entry->name) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found)
                continue;
        }

        SC_CH_LOG("testing channel: %s", entry->name);

        /* Generate scenarios via Gemini */
        int count = cfg->tests_per_channel > 0 ? cfg->tests_per_channel : 3;
        char prompt[4096];
        snprintf(prompt, sizeof(prompt), CONV_PROMPT, count, entry->name);

        char *response = NULL;
        size_t response_len = 0;
        sc_error_t err = sc_synth_gemini_generate(alloc, gemini, prompt, strlen(prompt), &response,
                                                  &response_len);
        if (err != SC_OK) {
            SC_CH_LOG("Gemini generation failed for %s: %s", entry->name, sc_error_string(err));
            sc_synth_metrics_record(alloc, metrics, 0, SC_SYNTH_ERROR);
            continue;
        }

        sc_json_value_t *root = NULL;
        err = sc_json_parse(alloc, response, response_len, &root);
        sc_synth_strfree(alloc, response, response_len);
        if (err != SC_OK || !root || root->type != SC_JSON_ARRAY) {
            SC_CH_LOG("failed to parse Gemini response for %s", entry->name);
            if (root)
                sc_json_free(alloc, root);
            sc_synth_metrics_record(alloc, metrics, 0, SC_SYNTH_ERROR);
            continue;
        }

        SC_CH_LOG("  generated %zu scenarios for %s", root->data.array.len, entry->name);

        for (size_t si = 0; si < root->data.array.len; si++) {
            run_scenario(alloc, cfg, entry, root->data.array.items[si], metrics);
        }

        sc_json_free(alloc, root);
    }

    return SC_OK;
}
