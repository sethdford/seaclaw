/* Config getters tests — sc_config_getters.c coverage. */
#include "seaclaw/config.h"
#include "seaclaw/core/allocator.h"
#include "seaclaw/core/arena.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/string.h"
#include "test_framework.h"
#include <stdlib.h>
#include <string.h>

static sc_config_t *make_config_with_arena(void) {
    sc_allocator_t backing = sc_system_allocator();
    sc_arena_t *arena = sc_arena_create(backing);
    SC_ASSERT_NOT_NULL(arena);
    sc_config_t *cfg = (sc_config_t *)backing.alloc(backing.ctx, sizeof(sc_config_t));
    SC_ASSERT_NOT_NULL(cfg);
    memset(cfg, 0, sizeof(*cfg));
    cfg->arena = arena;
    cfg->allocator = sc_arena_allocator(arena);
    return cfg;
}

static void free_config(sc_config_t *cfg) {
    if (!cfg)
        return;
    sc_allocator_t backing = sc_system_allocator();
    if (cfg->arena)
        sc_arena_destroy(cfg->arena);
    backing.free(backing.ctx, cfg, sizeof(*cfg));
}

/* ─── sc_config_provider_requires_api_key ─────────────────────────────────── */

static void test_provider_requires_api_key_null_returns_true(void) {
    SC_ASSERT_TRUE(sc_config_provider_requires_api_key(NULL));
}

static void test_provider_requires_api_key_local_providers_return_false(void) {
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("ollama"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("lmstudio"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("lm-studio"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("claude_cli"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("codex_cli"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("llamacpp"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("llama.cpp"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("vllm"));
    SC_ASSERT_FALSE(sc_config_provider_requires_api_key("sglang"));
}

static void test_provider_requires_api_key_cloud_providers_return_true(void) {
    SC_ASSERT_TRUE(sc_config_provider_requires_api_key("openai"));
    SC_ASSERT_TRUE(sc_config_provider_requires_api_key("anthropic"));
    SC_ASSERT_TRUE(sc_config_provider_requires_api_key("unknown"));
}

/* ─── sc_config_validate ───────────────────────────────────────────────────── */

static void test_config_validate_null_returns_invalid_argument(void) {
    sc_error_t err = sc_config_validate(NULL);
    SC_ASSERT_EQ(err, SC_ERR_INVALID_ARGUMENT);
}

static void test_config_validate_empty_provider_returns_config_invalid(void) {
    sc_config_t cfg = {0};
    cfg.default_model = "gpt-4";
    cfg.gateway.port = 3000;
    sc_error_t err = sc_config_validate(&cfg);
    SC_ASSERT_EQ(err, SC_ERR_CONFIG_INVALID);
}

static void test_config_validate_empty_model_returns_config_invalid(void) {
    sc_config_t cfg = {0};
    cfg.default_provider = "openai";
    cfg.gateway.port = 3000;
    sc_error_t err = sc_config_validate(&cfg);
    SC_ASSERT_EQ(err, SC_ERR_CONFIG_INVALID);
}

static void test_config_validate_autonomy_gt_4_returns_config_invalid(void) {
    sc_config_t cfg = {0};
    cfg.default_provider = "ollama";
    cfg.default_model = "llama2";
    cfg.gateway.port = 3000;
    cfg.security.autonomy_level = 5;
    sc_error_t err = sc_config_validate(&cfg);
    SC_ASSERT_EQ(err, SC_ERR_CONFIG_INVALID);
}

static void test_config_validate_port_zero_returns_config_invalid(void) {
    sc_config_t cfg = {0};
    cfg.default_provider = "ollama";
    cfg.default_model = "llama2";
    cfg.gateway.port = 0;
    sc_error_t err = sc_config_validate(&cfg);
    SC_ASSERT_EQ(err, SC_ERR_CONFIG_INVALID);
}

static void test_config_validate_valid_config_returns_ok(void) {
    sc_config_t cfg = {0};
    cfg.default_provider = "ollama";
    cfg.default_model = "llama2";
    cfg.gateway.port = 3000;
    sc_error_t err = sc_config_validate(&cfg);
    SC_ASSERT_EQ(err, SC_OK);
}

/* ─── sc_config_get_provider_key ───────────────────────────────────────────── */

static void test_get_provider_key_null_cfg_returns_null(void) {
    SC_ASSERT_NULL(sc_config_get_provider_key(NULL, "openai"));
}

static void test_get_provider_key_null_name_returns_null(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_NULL(sc_config_get_provider_key(&cfg, NULL));
}

static void test_get_provider_key_from_provider_entry(void) {
    sc_config_t *cfg = make_config_with_arena();
    sc_provider_entry_t *p = (sc_provider_entry_t *)cfg->allocator.alloc(
        cfg->allocator.ctx, sizeof(sc_provider_entry_t));
    SC_ASSERT_NOT_NULL(p);
    memset(p, 0, sizeof(*p));
    p->name = sc_strdup(&cfg->allocator, "anthropic");
    p->api_key = sc_strdup(&cfg->allocator, "sk-ant-test");
    cfg->providers = p;
    cfg->providers_len = 1;
    const char *key = sc_config_get_provider_key(cfg, "anthropic");
    SC_ASSERT_NOT_NULL(key);
    SC_ASSERT_STR_EQ(key, "sk-ant-test");
    free_config(cfg);
}

static void test_get_provider_key_falls_back_to_api_key(void) {
    sc_config_t *cfg = make_config_with_arena();
    cfg->api_key = sc_strdup(&cfg->allocator, "global-key");
    sc_provider_entry_t *p = (sc_provider_entry_t *)cfg->allocator.alloc(
        cfg->allocator.ctx, sizeof(sc_provider_entry_t));
    SC_ASSERT_NOT_NULL(p);
    memset(p, 0, sizeof(*p));
    p->name = sc_strdup(&cfg->allocator, "openai");
    p->api_key = NULL;
    cfg->providers = p;
    cfg->providers_len = 1;
    const char *key = sc_config_get_provider_key(cfg, "openai");
    SC_ASSERT_NOT_NULL(key);
    SC_ASSERT_STR_EQ(key, "global-key");
    free_config(cfg);
}

static void test_get_provider_key_unknown_provider_uses_api_key(void) {
    sc_config_t cfg = {0};
    char api_key[] = "sk-global";
    cfg.api_key = api_key;
    const char *key = sc_config_get_provider_key(&cfg, "nonexistent");
    SC_ASSERT_NOT_NULL(key);
    SC_ASSERT_STR_EQ(key, "sk-global");
}

static void test_get_provider_key_unknown_no_api_key_returns_null(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_NULL(sc_config_get_provider_key(&cfg, "nonexistent"));
}

/* ─── sc_config_default_provider_key ───────────────────────────────────────── */

static void test_default_provider_key_null_cfg_returns_null(void) {
    SC_ASSERT_NULL(sc_config_default_provider_key(NULL));
}

static void test_default_provider_key_returns_key_for_default_provider(void) {
    sc_config_t *cfg = make_config_with_arena();
    cfg->default_provider = sc_strdup(&cfg->allocator, "openai");
    cfg->api_key = sc_strdup(&cfg->allocator, "sk-openai");
    const char *key = sc_config_default_provider_key(cfg);
    SC_ASSERT_NOT_NULL(key);
    SC_ASSERT_STR_EQ(key, "sk-openai");
    free_config(cfg);
}

/* ─── sc_config_get_provider_base_url ──────────────────────────────────────── */

static void test_get_provider_base_url_null_inputs_return_null(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_NULL(sc_config_get_provider_base_url(NULL, "openai"));
    SC_ASSERT_NULL(sc_config_get_provider_base_url(&cfg, NULL));
}

static void test_get_provider_base_url_returns_url_for_provider(void) {
    sc_config_t *cfg = make_config_with_arena();
    sc_provider_entry_t *p = (sc_provider_entry_t *)cfg->allocator.alloc(
        cfg->allocator.ctx, sizeof(sc_provider_entry_t));
    SC_ASSERT_NOT_NULL(p);
    memset(p, 0, sizeof(*p));
    p->name = sc_strdup(&cfg->allocator, "compatible");
    p->base_url = sc_strdup(&cfg->allocator, "https://api.example.com");
    cfg->providers = p;
    cfg->providers_len = 1;
    const char *url = sc_config_get_provider_base_url(cfg, "compatible");
    SC_ASSERT_NOT_NULL(url);
    SC_ASSERT_STR_EQ(url, "https://api.example.com");
    free_config(cfg);
}

static void test_get_provider_base_url_unknown_returns_null(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_NULL(sc_config_get_provider_base_url(&cfg, "unknown"));
}

/* ─── sc_config_get_provider_native_tools ──────────────────────────────────── */

static void test_get_provider_native_tools_null_inputs_default_true(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_TRUE(sc_config_get_provider_native_tools(NULL, "openai"));
    SC_ASSERT_TRUE(sc_config_get_provider_native_tools(&cfg, NULL));
}

static void test_get_provider_native_tools_respects_provider_setting(void) {
    sc_config_t *cfg = make_config_with_arena();
    sc_provider_entry_t *p = (sc_provider_entry_t *)cfg->allocator.alloc(
        cfg->allocator.ctx, sizeof(sc_provider_entry_t));
    SC_ASSERT_NOT_NULL(p);
    memset(p, 0, sizeof(*p));
    p->name = sc_strdup(&cfg->allocator, "ollama");
    p->native_tools = false;
    cfg->providers = p;
    cfg->providers_len = 1;
    SC_ASSERT_FALSE(sc_config_get_provider_native_tools(cfg, "ollama"));
    p->native_tools = true;
    SC_ASSERT_TRUE(sc_config_get_provider_native_tools(cfg, "ollama"));
    free_config(cfg);
}

static void test_get_provider_native_tools_unknown_defaults_true(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_TRUE(sc_config_get_provider_native_tools(&cfg, "unknown"));
}

/* ─── sc_config_get_web_search_provider ────────────────────────────────────── */

static void test_get_web_search_provider_null_cfg_returns_duckduckgo(void) {
    unsetenv("WEB_SEARCH_PROVIDER");
    unsetenv("SEACLAW_WEB_SEARCH_PROVIDER");
    const char *p = sc_config_get_web_search_provider(NULL);
    SC_ASSERT_NOT_NULL(p);
    SC_ASSERT_STR_EQ(p, "duckduckgo");
}

static void test_get_web_search_provider_uses_config_value(void) {
    sc_config_t cfg = {0};
    char provider[] = "brave";
    cfg.tools.web_search_provider = provider;
    unsetenv("WEB_SEARCH_PROVIDER");
    unsetenv("SEACLAW_WEB_SEARCH_PROVIDER");
    const char *p = sc_config_get_web_search_provider(&cfg);
    SC_ASSERT_NOT_NULL(p);
    SC_ASSERT_STR_EQ(p, "brave");
}

/* ─── sc_config_get_channel_configured_count ──────────────────────────────── */

static void test_get_channel_configured_count_null_inputs_return_zero(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_EQ(sc_config_get_channel_configured_count(NULL, "telegram"), 0u);
    SC_ASSERT_EQ(sc_config_get_channel_configured_count(&cfg, NULL), 0u);
}

static void test_get_channel_configured_count_returns_count_for_key(void) {
    sc_config_t cfg = {0};
    char key_telegram[] = "telegram";
    cfg.channels.channel_config_keys[0] = key_telegram;
    cfg.channels.channel_config_counts[0] = 3;
    cfg.channels.channel_config_len = 1;
    SC_ASSERT_EQ(sc_config_get_channel_configured_count(&cfg, "telegram"), 3u);
}

static void test_get_channel_configured_count_unknown_returns_zero(void) {
    sc_config_t cfg = {0};
    cfg.channels.channel_config_len = 0;
    SC_ASSERT_EQ(sc_config_get_channel_configured_count(&cfg, "unknown"), 0u);
}

/* ─── sc_config_get_provider_ws_streaming ──────────────────────────────────── */

static void test_get_provider_ws_streaming_null_inputs_return_false(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_FALSE(sc_config_get_provider_ws_streaming(NULL, "openai"));
    SC_ASSERT_FALSE(sc_config_get_provider_ws_streaming(&cfg, NULL));
}

static void test_get_provider_ws_streaming_respects_provider_setting(void) {
    sc_config_t *cfg = make_config_with_arena();
    sc_provider_entry_t *p = (sc_provider_entry_t *)cfg->allocator.alloc(
        cfg->allocator.ctx, sizeof(sc_provider_entry_t));
    SC_ASSERT_NOT_NULL(p);
    memset(p, 0, sizeof(*p));
    p->name = sc_strdup(&cfg->allocator, "test");
    p->ws_streaming = true;
    cfg->providers = p;
    cfg->providers_len = 1;
    SC_ASSERT_TRUE(sc_config_get_provider_ws_streaming(cfg, "test"));
    free_config(cfg);
}

static void test_get_provider_ws_streaming_unknown_defaults_false(void) {
    sc_config_t cfg = {0};
    SC_ASSERT_FALSE(sc_config_get_provider_ws_streaming(&cfg, "unknown"));
}

/* ─── sc_config_persona_for_channel ────────────────────────────────────────── */

static void test_persona_for_channel_null_cfg_returns_null(void) {
    SC_ASSERT_NULL(sc_config_persona_for_channel(NULL, "telegram"));
}

static void test_persona_for_channel_channel_override(void) {
    sc_config_t *cfg = make_config_with_arena();
    cfg->agent.persona = sc_strdup(&cfg->allocator, "default");
    sc_persona_channel_entry_t *entries = (sc_persona_channel_entry_t *)cfg->allocator.alloc(
        cfg->allocator.ctx, sizeof(sc_persona_channel_entry_t) * 2);
    SC_ASSERT_NOT_NULL(entries);
    memset(entries, 0, sizeof(sc_persona_channel_entry_t) * 2);
    entries[0].channel = sc_strdup(&cfg->allocator, "imessage");
    entries[0].persona = sc_strdup(&cfg->allocator, "casual");
    entries[1].channel = sc_strdup(&cfg->allocator, "gmail");
    entries[1].persona = sc_strdup(&cfg->allocator, "professional");
    cfg->agent.persona_channels = entries;
    cfg->agent.persona_channels_count = 2;

    const char *im = sc_config_persona_for_channel(cfg, "imessage");
    SC_ASSERT_NOT_NULL(im);
    SC_ASSERT_STR_EQ(im, "casual");

    const char *gm = sc_config_persona_for_channel(cfg, "gmail");
    SC_ASSERT_NOT_NULL(gm);
    SC_ASSERT_STR_EQ(gm, "professional");

    free_config(cfg);
}

static void test_persona_for_channel_falls_back_to_default(void) {
    sc_config_t cfg = {0};
    char default_persona[] = "seth";
    cfg.agent.persona = default_persona;
    cfg.agent.persona_channels = NULL;
    cfg.agent.persona_channels_count = 0;

    const char *p = sc_config_persona_for_channel(&cfg, "telegram");
    SC_ASSERT_NOT_NULL(p);
    SC_ASSERT_STR_EQ(p, "seth");

    const char *p_null = sc_config_persona_for_channel(&cfg, NULL);
    SC_ASSERT_NOT_NULL(p_null);
    SC_ASSERT_STR_EQ(p_null, "seth");
}

/* ─── sc_config_set_reload_requested / sc_config_get_and_clear_reload_requested ─ */

static void test_reload_flag_set_then_get_returns_true(void) {
    sc_config_get_and_clear_reload_requested();
    sc_config_set_reload_requested();
    SC_ASSERT_TRUE(sc_config_get_and_clear_reload_requested());
}

static void test_reload_flag_get_again_returns_false_cleared(void) {
    sc_config_get_and_clear_reload_requested();
    sc_config_set_reload_requested();
    (void)sc_config_get_and_clear_reload_requested();
    SC_ASSERT_FALSE(sc_config_get_and_clear_reload_requested());
}

/* ─── run ──────────────────────────────────────────────────────────────────── */

void run_config_getters_tests(void) {
    SC_TEST_SUITE("Config getters");
    SC_RUN_TEST(test_provider_requires_api_key_null_returns_true);
    SC_RUN_TEST(test_provider_requires_api_key_local_providers_return_false);
    SC_RUN_TEST(test_provider_requires_api_key_cloud_providers_return_true);
    SC_RUN_TEST(test_config_validate_null_returns_invalid_argument);
    SC_RUN_TEST(test_config_validate_empty_provider_returns_config_invalid);
    SC_RUN_TEST(test_config_validate_empty_model_returns_config_invalid);
    SC_RUN_TEST(test_config_validate_autonomy_gt_4_returns_config_invalid);
    SC_RUN_TEST(test_config_validate_port_zero_returns_config_invalid);
    SC_RUN_TEST(test_config_validate_valid_config_returns_ok);
    SC_RUN_TEST(test_get_provider_key_null_cfg_returns_null);
    SC_RUN_TEST(test_get_provider_key_null_name_returns_null);
    SC_RUN_TEST(test_get_provider_key_from_provider_entry);
    SC_RUN_TEST(test_get_provider_key_falls_back_to_api_key);
    SC_RUN_TEST(test_get_provider_key_unknown_provider_uses_api_key);
    SC_RUN_TEST(test_get_provider_key_unknown_no_api_key_returns_null);
    SC_RUN_TEST(test_default_provider_key_null_cfg_returns_null);
    SC_RUN_TEST(test_default_provider_key_returns_key_for_default_provider);
    SC_RUN_TEST(test_get_provider_base_url_null_inputs_return_null);
    SC_RUN_TEST(test_get_provider_base_url_returns_url_for_provider);
    SC_RUN_TEST(test_get_provider_base_url_unknown_returns_null);
    SC_RUN_TEST(test_get_provider_native_tools_null_inputs_default_true);
    SC_RUN_TEST(test_get_provider_native_tools_respects_provider_setting);
    SC_RUN_TEST(test_get_provider_native_tools_unknown_defaults_true);
    SC_RUN_TEST(test_get_web_search_provider_null_cfg_returns_duckduckgo);
    SC_RUN_TEST(test_get_web_search_provider_uses_config_value);
    SC_RUN_TEST(test_get_channel_configured_count_null_inputs_return_zero);
    SC_RUN_TEST(test_get_channel_configured_count_returns_count_for_key);
    SC_RUN_TEST(test_get_channel_configured_count_unknown_returns_zero);
    SC_RUN_TEST(test_get_provider_ws_streaming_null_inputs_return_false);
    SC_RUN_TEST(test_get_provider_ws_streaming_respects_provider_setting);
    SC_RUN_TEST(test_get_provider_ws_streaming_unknown_defaults_false);
    SC_RUN_TEST(test_persona_for_channel_null_cfg_returns_null);
    SC_RUN_TEST(test_persona_for_channel_channel_override);
    SC_RUN_TEST(test_persona_for_channel_falls_back_to_default);
    SC_RUN_TEST(test_reload_flag_set_then_get_returns_true);
    SC_RUN_TEST(test_reload_flag_get_again_returns_false_cleared);
}
