#include "channel_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(__MACH__)

static bool messages_app_running(void) {
    return system("pgrep -q Messages") == 0;
}

static sc_error_t send_imessage_osascript(const char *target, const char *message) {
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "osascript -e '"
             "tell application \"Messages\"\n"
             "  set targetBuddy to buddy \"%s\" of (service 1 whose service type is iMessage)\n"
             "  send \"%s\" to targetBuddy\n"
             "end tell' 2>/dev/null",
             target, message);
    int ret = system(cmd);
    return ret == 0 ? SC_OK : SC_ERR_IO;
}

sc_error_t sc_channel_run_real_imessage(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                                        sc_synth_gemini_ctx_t *gemini,
                                        sc_synth_metrics_t *metrics) {
    sc_synth_metrics_init(metrics);

    if (!cfg->real_imessage_target) {
        SC_CH_LOG("no --real-imessage target specified, skipping");
        return SC_OK;
    }

    SC_CH_LOG("Real iMessage test to: %s", cfg->real_imessage_target);

    if (!messages_app_running()) {
        SC_CH_LOG("WARNING: Messages.app not running, test may fail");
    }

    /* Generate a conversation starter via Gemini */
    const char *prompt =
        "Generate a short, friendly test message (1 sentence) that could be sent to someone "
        "to verify an iMessage integration is working. Include a unique identifier like a number "
        "so we can verify the response references it. Return ONLY the message text, no JSON.";
    char *response = NULL;
    size_t response_len = 0;
    sc_error_t err =
        sc_synth_gemini_generate(alloc, gemini, prompt, strlen(prompt), &response, &response_len);
    if (err != SC_OK) {
        SC_CH_LOG("Gemini failed to generate message: %s", sc_error_string(err));
        sc_synth_metrics_record(alloc, metrics, 0, SC_SYNTH_ERROR);
        return err;
    }

    /* Trim whitespace/quotes from response */
    char *msg = response;
    while (*msg == '"' || *msg == ' ' || *msg == '\n')
        msg++;
    size_t mlen = strlen(msg);
    while (mlen > 0 && (msg[mlen - 1] == '"' || msg[mlen - 1] == '\n' || msg[mlen - 1] == ' '))
        mlen--;
    msg[mlen] = '\0';

    SC_CH_LOG("sending: \"%s\"", msg);

    double t0 = sc_synth_now_ms();
    err = send_imessage_osascript(cfg->real_imessage_target, msg);
    double send_lat = sc_synth_now_ms() - t0;

    if (err != SC_OK) {
        SC_CH_LOG("FAIL: osascript send failed (%.1fms)", send_lat);
        sc_synth_metrics_record(alloc, metrics, send_lat, SC_SYNTH_FAIL);
        if (cfg->regression_dir) {
            sc_synth_test_case_t tc = {.name = (char *)"real_imessage_send",
                                       .category = (char *)"real_imessage",
                                       .input_json = msg,
                                       .verdict = SC_SYNTH_FAIL,
                                       .verdict_reason = (char *)"osascript failed"};
            sc_synth_regression_save(alloc, cfg->regression_dir, &tc);
        }
    } else {
        SC_CH_LOG("PASS: message sent successfully (%.1fms)", send_lat);
        sc_synth_metrics_record(alloc, metrics, send_lat, SC_SYNTH_PASS);
    }

    sc_synth_strfree(alloc, response, response_len);
    return SC_OK;
}

#else /* Not macOS */

sc_error_t sc_channel_run_real_imessage(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                                        sc_synth_gemini_ctx_t *gemini,
                                        sc_synth_metrics_t *metrics) {
    (void)alloc;
    (void)cfg;
    (void)gemini;
    sc_synth_metrics_init(metrics);
    SC_CH_LOG("real iMessage not supported on this platform");
    return SC_ERR_NOT_SUPPORTED;
}

#endif
