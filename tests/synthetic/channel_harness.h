#ifndef SC_CHANNEL_HARNESS_H
#define SC_CHANNEL_HARNESS_H
#include "seaclaw/channel.h"
#include "seaclaw/channel_loop.h"
#include "synthetic_harness.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum sc_chaos_mode {
    SC_CHAOS_NONE = 0,
    SC_CHAOS_MESSAGE = 1,
    SC_CHAOS_INFRA = 2,
    SC_CHAOS_ALL = 3,
} sc_chaos_mode_t;

typedef struct sc_channel_test_config {
    const char *binary_path;
    const char *gemini_api_key;
    const char *gemini_model;
    uint16_t gateway_port;
    int tests_per_channel;
    int concurrency;
    int duration_secs;
    sc_chaos_mode_t chaos;
    const char *regression_dir;
    const char *real_imessage_target;
    const char **channels;
    size_t channel_count;
    bool all_channels;
    bool verbose;
} sc_channel_test_config_t;

typedef struct sc_conversation_turn {
    char user_message[4096];
    char expect_pattern[512];
} sc_conversation_turn_t;

typedef struct sc_conversation_scenario {
    char channel_name[32];
    char session_key[128];
    sc_conversation_turn_t turns[16];
    size_t turn_count;
} sc_conversation_scenario_t;

typedef struct sc_channel_test_entry {
    const char *name;
    sc_error_t (*create)(sc_allocator_t *alloc, sc_channel_t *out);
    void (*destroy)(sc_channel_t *ch);
    sc_error_t (*inject)(sc_channel_t *ch, const char *session_key, size_t sk_len,
                         const char *content, size_t c_len);
    sc_error_t (*poll)(void *ctx, sc_allocator_t *alloc, sc_channel_loop_msg_t *msgs, size_t max,
                       size_t *count);
    const char *(*get_last)(sc_channel_t *ch, size_t *out_len);
} sc_channel_test_entry_t;

sc_error_t sc_channel_run_conversations(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                                        sc_synth_gemini_ctx_t *gemini, sc_synth_metrics_t *metrics);
sc_error_t sc_channel_run_chaos(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                                sc_synth_gemini_ctx_t *gemini, sc_synth_metrics_t *metrics);
sc_error_t sc_channel_run_pressure(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                                   sc_synth_gemini_ctx_t *gemini, sc_synth_metrics_t *metrics);
sc_error_t sc_channel_run_real_imessage(sc_allocator_t *alloc, const sc_channel_test_config_t *cfg,
                                        sc_synth_gemini_ctx_t *gemini, sc_synth_metrics_t *metrics);

const sc_channel_test_entry_t *sc_channel_test_registry(size_t *count);
const sc_channel_test_entry_t *sc_channel_test_find(const char *name);

#define SC_CH_LOG(fmt, ...) fprintf(stderr, "[channel] " fmt "\n", ##__VA_ARGS__)
#define SC_CH_VERBOSE(cfg, fmt, ...)                           \
    do {                                                       \
        if ((cfg)->verbose)                                    \
            fprintf(stderr, "  [v] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#endif
