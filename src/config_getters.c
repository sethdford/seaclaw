#include "seaclaw/config.h"
#include "seaclaw/core/error.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool sc_config_provider_requires_api_key(const char *provider) {
    if (!provider)
        return true;
    if (strcmp(provider, "ollama") == 0)
        return false;
    if (strcmp(provider, "lmstudio") == 0)
        return false;
    if (strcmp(provider, "lm-studio") == 0)
        return false;
    if (strcmp(provider, "claude_cli") == 0)
        return false;
    if (strcmp(provider, "codex_cli") == 0)
        return false;
    if (strcmp(provider, "llamacpp") == 0)
        return false;
    if (strcmp(provider, "llama.cpp") == 0)
        return false;
    if (strcmp(provider, "vllm") == 0)
        return false;
    if (strcmp(provider, "sglang") == 0)
        return false;
    return true;
}

/* sc_config_validate_strict implemented in config_validate.c */

sc_error_t sc_config_validate(const sc_config_t *cfg) {
    if (!cfg)
        return SC_ERR_INVALID_ARGUMENT;
    if (!cfg->default_provider || !cfg->default_provider[0])
        return SC_ERR_CONFIG_INVALID;
    if (!cfg->default_model || !cfg->default_model[0])
        return SC_ERR_CONFIG_INVALID;
    if (cfg->security.autonomy_level > 4)
        return SC_ERR_CONFIG_INVALID;
    if (cfg->gateway.port == 0)
        return SC_ERR_CONFIG_INVALID;
    if (sc_config_provider_requires_api_key(cfg->default_provider)) {
        const char *key = sc_config_default_provider_key(cfg);
        if (!key || !key[0])
            fprintf(stderr, "Warning: provider %s requires an API key but none is configured\n",
                    cfg->default_provider ? cfg->default_provider : "(unknown)");
    }
    return SC_OK;
}

const char *sc_config_get_provider_key(const sc_config_t *cfg, const char *name) {
    if (!cfg || !name)
        return NULL;
    for (size_t i = 0; i < cfg->providers_len; i++) {
        if (cfg->providers[i].name && strcmp(cfg->providers[i].name, name) == 0) {
            if (cfg->providers[i].api_key && cfg->providers[i].api_key[0])
                return cfg->providers[i].api_key;
            break;
        }
    }
    return (cfg->api_key && cfg->api_key[0]) ? cfg->api_key : NULL;
}

const char *sc_config_default_provider_key(const sc_config_t *cfg) {
    return cfg ? sc_config_get_provider_key(cfg, cfg->default_provider) : NULL;
}

const char *sc_config_get_provider_base_url(const sc_config_t *cfg, const char *name) {
    if (!cfg || !name)
        return NULL;
    for (size_t i = 0; i < cfg->providers_len; i++) {
        if (cfg->providers[i].name && strcmp(cfg->providers[i].name, name) == 0)
            return cfg->providers[i].base_url;
    }
    return NULL;
}

bool sc_config_get_provider_native_tools(const sc_config_t *cfg, const char *name) {
    if (!cfg || !name)
        return true;
    for (size_t i = 0; i < cfg->providers_len; i++) {
        if (cfg->providers[i].name && strcmp(cfg->providers[i].name, name) == 0)
            return cfg->providers[i].native_tools;
    }
    return true;
}

const char *sc_config_get_web_search_provider(const sc_config_t *cfg) {
    const char *v = getenv("WEB_SEARCH_PROVIDER");
    if (!v)
        v = getenv("SEACLAW_WEB_SEARCH_PROVIDER");
    if (v && v[0])
        return v;
    return (cfg && cfg->tools.web_search_provider && cfg->tools.web_search_provider[0])
               ? cfg->tools.web_search_provider
               : "duckduckgo";
}

size_t sc_config_get_channel_configured_count(const sc_config_t *cfg, const char *key) {
    if (!cfg || !key)
        return 0;
    for (size_t i = 0; i < cfg->channels.channel_config_len; i++) {
        if (cfg->channels.channel_config_keys[i] &&
            strcmp(cfg->channels.channel_config_keys[i], key) == 0)
            return cfg->channels.channel_config_counts[i];
    }
    return 0;
}

bool sc_config_get_provider_ws_streaming(const sc_config_t *cfg, const char *name) {
    if (!cfg || !name)
        return false;
    for (size_t i = 0; i < cfg->providers_len; i++) {
        if (cfg->providers[i].name && strcmp(cfg->providers[i].name, name) == 0)
            return cfg->providers[i].ws_streaming;
    }
    return false;
}

const char *sc_config_persona_for_channel(const sc_config_t *cfg, const char *channel) {
    if (!cfg)
        return NULL;
    if (channel && cfg->agent.persona_channels && cfg->agent.persona_channels_count > 0) {
        for (size_t i = 0; i < cfg->agent.persona_channels_count; i++) {
            if (cfg->agent.persona_channels[i].channel &&
                strcmp(cfg->agent.persona_channels[i].channel, channel) == 0)
                return cfg->agent.persona_channels[i].persona;
        }
    }
    return cfg->agent.persona;
}

bool sc_config_get_tool_model_override(const sc_config_t *cfg, const char *tool_name,
                                       const char **provider_out, const char **model_out) {
    if (!cfg || !tool_name)
        return false;
    for (size_t i = 0; i < cfg->tools.model_overrides_len; i++) {
        if (cfg->tools.model_overrides[i].tool_name &&
            strcmp(cfg->tools.model_overrides[i].tool_name, tool_name) == 0) {
            if (provider_out)
                *provider_out = cfg->tools.model_overrides[i].provider;
            if (model_out)
                *model_out = cfg->tools.model_overrides[i].model;
            return true;
        }
    }
    return false;
}

static volatile _Atomic int sc_reload_flag = 0;

void sc_config_set_reload_requested(void) {
    atomic_store(&sc_reload_flag, 1);
}

bool sc_config_get_and_clear_reload_requested(void) {
    return atomic_exchange(&sc_reload_flag, 0) != 0;
}
