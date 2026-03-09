#include "seaclaw/config_parse.h"
#include "seaclaw/config.h"
#include "seaclaw/core/string.h"
#include <stdio.h>
#include <string.h>

#include "config_internal.h"
#include "config_parse_internal.h"

static sc_error_t parse_nodes(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *arr) {
    if (!arr || arr->type != SC_JSON_ARRAY)
        return SC_OK;
    for (size_t i = 0; i < cfg->nodes_len; i++) {
        if (cfg->nodes[i].name) {
            a->free(a->ctx, cfg->nodes[i].name, strlen(cfg->nodes[i].name) + 1);
            cfg->nodes[i].name = NULL;
        }
        if (cfg->nodes[i].status) {
            a->free(a->ctx, cfg->nodes[i].status, strlen(cfg->nodes[i].status) + 1);
            cfg->nodes[i].status = NULL;
        }
    }
    cfg->nodes_len = 0;
    size_t cap = arr->data.array.len;
    if (cap > SC_NODES_MAX)
        cap = SC_NODES_MAX;
    size_t n = 0;
    for (size_t i = 0; i < cap; i++) {
        const sc_json_value_t *item = arr->data.array.items[i];
        if (!item || item->type != SC_JSON_OBJECT)
            continue;
        const char *name = sc_json_get_string(item, "name");
        if (!name)
            continue;
        const char *status = sc_json_get_string(item, "status");
        cfg->nodes[n].name = sc_strdup(a, name);
        cfg->nodes[n].status = status && status[0] ? sc_strdup(a, status) : sc_strdup(a, "online");
        n++;
    }
    cfg->nodes_len = n;
    return SC_OK;
}

sc_error_t parse_string_array(sc_allocator_t *a, char ***out, size_t *out_len,
                              const sc_json_value_t *arr) {
    if (!arr || arr->type != SC_JSON_ARRAY)
        return SC_OK;
    size_t n = 0;
    for (size_t i = 0; i < arr->data.array.len; i++) {
        if (arr->data.array.items[i] && arr->data.array.items[i]->type == SC_JSON_STRING)
            n++;
    }
    if (n == 0)
        return SC_OK;

    char **list = (char **)a->alloc(a->ctx, n * sizeof(char *));
    if (!list)
        return SC_ERR_OUT_OF_MEMORY;

    size_t j = 0;
    for (size_t i = 0; i < arr->data.array.len && j < n; i++) {
        const sc_json_value_t *v = arr->data.array.items[i];
        if (!v || v->type != SC_JSON_STRING)
            continue;
        const char *s = v->data.string.ptr;
        if (s)
            list[j++] = sc_strdup(a, s);
    }
    *out = list;
    *out_len = j;
    return SC_OK;
}

static sc_error_t parse_autonomy(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;

    const char *level = sc_json_get_string(obj, "level");
    if (level) {
        if (cfg->autonomy.level)
            a->free(a->ctx, cfg->autonomy.level, strlen(cfg->autonomy.level) + 1);
        cfg->autonomy.level = sc_strdup(a, level);
    }
    cfg->autonomy.workspace_only =
        sc_json_get_bool(obj, "workspace_only", cfg->autonomy.workspace_only);
    double max_act =
        sc_json_get_number(obj, "max_actions_per_hour", cfg->autonomy.max_actions_per_hour);
    if (max_act >= 0 && max_act <= 1000000)
        cfg->autonomy.max_actions_per_hour = (uint32_t)max_act;
    cfg->autonomy.require_approval_for_medium_risk = sc_json_get_bool(
        obj, "require_approval_for_medium_risk", cfg->autonomy.require_approval_for_medium_risk);
    cfg->autonomy.block_high_risk_commands =
        sc_json_get_bool(obj, "block_high_risk_commands", cfg->autonomy.block_high_risk_commands);

    sc_json_value_t *ac = sc_json_object_get(obj, "allowed_commands");
    if (ac && ac->type == SC_JSON_ARRAY) {
        if (cfg->autonomy.allowed_commands) {
            for (size_t i = 0; i < cfg->autonomy.allowed_commands_len; i++)
                a->free(a->ctx, cfg->autonomy.allowed_commands[i],
                        strlen(cfg->autonomy.allowed_commands[i]) + 1);
            a->free(a->ctx, cfg->autonomy.allowed_commands,
                    cfg->autonomy.allowed_commands_len * sizeof(char *));
        }
        parse_string_array(a, &cfg->autonomy.allowed_commands, &cfg->autonomy.allowed_commands_len,
                           ac);
    }
    return SC_OK;
}

static sc_error_t parse_cron(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    (void)a;
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->cron.enabled = sc_json_get_bool(obj, "enabled", cfg->cron.enabled);
    double im = sc_json_get_number(obj, "interval_minutes", cfg->cron.interval_minutes);
    if (im >= 1 && im <= 1440)
        cfg->cron.interval_minutes = (uint32_t)im;
    double mrh = sc_json_get_number(obj, "max_run_history", cfg->cron.max_run_history);
    if (mrh >= 0 && mrh <= 10000)
        cfg->cron.max_run_history = (uint32_t)mrh;
    return SC_OK;
}

static sc_error_t parse_scheduler(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    (void)a;
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    double mc = sc_json_get_number(obj, "max_concurrent", cfg->scheduler.max_concurrent);
    if (mc >= 0 && mc <= 256)
        cfg->scheduler.max_concurrent = (uint32_t)mc;
    return SC_OK;
}

static sc_error_t parse_gateway(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->gateway.enabled = sc_json_get_bool(obj, "enabled", cfg->gateway.enabled);
    double port = sc_json_get_number(obj, "port", cfg->gateway.port);
    if (port < 1)
        port = 1;
    else if (port > 65535)
        port = 65535;
    cfg->gateway.port = (uint16_t)port;
    const char *host = sc_json_get_string(obj, "host");
    if (host) {
        if (cfg->gateway.host)
            a->free(a->ctx, cfg->gateway.host, strlen(cfg->gateway.host) + 1);
        cfg->gateway.host = sc_strdup(a, host);
    }
    cfg->gateway.require_pairing =
        sc_json_get_bool(obj, "require_pairing", cfg->gateway.require_pairing);
    const char *at = sc_json_get_string(obj, "auth_token");
    if (at) {
        if (cfg->gateway.auth_token)
            a->free(a->ctx, cfg->gateway.auth_token, strlen(cfg->gateway.auth_token) + 1);
        cfg->gateway.auth_token = sc_strdup(a, at);
    }
    cfg->gateway.allow_public_bind =
        sc_json_get_bool(obj, "allow_public_bind", cfg->gateway.allow_public_bind);
    double prl = sc_json_get_number(obj, "pair_rate_limit_per_minute",
                                    cfg->gateway.pair_rate_limit_per_minute);
    if (prl >= 0 && prl <= 1000)
        cfg->gateway.pair_rate_limit_per_minute = (uint32_t)prl;
    double rlr = sc_json_get_number(obj, "rate_limit_requests", cfg->gateway.rate_limit_requests);
    if (rlr >= 0 && rlr <= 10000)
        cfg->gateway.rate_limit_requests = (int)rlr;
    double rlw = sc_json_get_number(obj, "rate_limit_window", cfg->gateway.rate_limit_window);
    if (rlw >= 0 && rlw <= 86400)
        cfg->gateway.rate_limit_window = (int)rlw;
    const char *whs = sc_json_get_string(obj, "webhook_hmac_secret");
    if (whs) {
        if (cfg->gateway.webhook_hmac_secret)
            a->free(a->ctx, cfg->gateway.webhook_hmac_secret,
                    strlen(cfg->gateway.webhook_hmac_secret) + 1);
        cfg->gateway.webhook_hmac_secret = sc_strdup(a, whs);
    }
    const char *uid = sc_json_get_string(obj, "control_ui_dir");
    if (uid) {
        if (cfg->gateway.control_ui_dir)
            a->free(a->ctx, cfg->gateway.control_ui_dir, strlen(cfg->gateway.control_ui_dir) + 1);
        cfg->gateway.control_ui_dir = sc_strdup(a, uid);
    }
    sc_json_value_t *cors = sc_json_object_get(obj, "cors_origins");
    if (cors && cors->type == SC_JSON_ARRAY) {
        if (cfg->gateway.cors_origins) {
            for (size_t i = 0; i < cfg->gateway.cors_origins_len; i++)
                if (cfg->gateway.cors_origins[i])
                    a->free(a->ctx, cfg->gateway.cors_origins[i],
                            strlen(cfg->gateway.cors_origins[i]) + 1);
            a->free(a->ctx, cfg->gateway.cors_origins,
                    cfg->gateway.cors_origins_len * sizeof(char *));
        }
        parse_string_array(a, &cfg->gateway.cors_origins, &cfg->gateway.cors_origins_len, cors);
    }
    return SC_OK;
}

static sc_error_t parse_memory(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    const char *profile = sc_json_get_string(obj, "profile");
    if (profile) {
        if (cfg->memory.profile)
            a->free(a->ctx, cfg->memory.profile, strlen(cfg->memory.profile) + 1);
        cfg->memory.profile = sc_strdup(a, profile);
    }
    const char *backend = sc_json_get_string(obj, "backend");
    if (backend) {
        if (cfg->memory.backend)
            a->free(a->ctx, cfg->memory.backend, strlen(cfg->memory.backend) + 1);
        cfg->memory.backend = sc_strdup(a, backend);
    }
    const char *sqlite_path = sc_json_get_string(obj, "sqlite_path");
    if (sqlite_path) {
        if (cfg->memory.sqlite_path)
            a->free(a->ctx, cfg->memory.sqlite_path, strlen(cfg->memory.sqlite_path) + 1);
        cfg->memory.sqlite_path = sc_strdup(a, sqlite_path);
    }
    double max_ent = sc_json_get_number(obj, "max_entries", cfg->memory.max_entries);
    if (max_ent >= 0 && max_ent <= 1000000)
        cfg->memory.max_entries = (uint32_t)max_ent;
    cfg->memory.auto_save = sc_json_get_bool(obj, "auto_save", cfg->memory.auto_save);
    double cih = sc_json_get_number(obj, "consolidation_interval_hours",
                                    cfg->memory.consolidation_interval_hours);
    if (cih >= 0 && cih <= 8760)
        cfg->memory.consolidation_interval_hours = (uint32_t)cih;

    const char *pg_url = sc_json_get_string(obj, "postgres_url");
    if (pg_url) {
        if (cfg->memory.postgres_url)
            a->free(a->ctx, cfg->memory.postgres_url, strlen(cfg->memory.postgres_url) + 1);
        cfg->memory.postgres_url = sc_strdup(a, pg_url);
    }
    const char *pg_schema = sc_json_get_string(obj, "postgres_schema");
    if (pg_schema) {
        if (cfg->memory.postgres_schema)
            a->free(a->ctx, cfg->memory.postgres_schema, strlen(cfg->memory.postgres_schema) + 1);
        cfg->memory.postgres_schema = sc_strdup(a, pg_schema);
    }
    const char *pg_table = sc_json_get_string(obj, "postgres_table");
    if (pg_table) {
        if (cfg->memory.postgres_table)
            a->free(a->ctx, cfg->memory.postgres_table, strlen(cfg->memory.postgres_table) + 1);
        cfg->memory.postgres_table = sc_strdup(a, pg_table);
    }
    const char *r_host = sc_json_get_string(obj, "redis_host");
    if (r_host) {
        if (cfg->memory.redis_host)
            a->free(a->ctx, cfg->memory.redis_host, strlen(cfg->memory.redis_host) + 1);
        cfg->memory.redis_host = sc_strdup(a, r_host);
    }
    double r_port = sc_json_get_number(obj, "redis_port", cfg->memory.redis_port);
    if (r_port >= 1 && r_port <= 65535)
        cfg->memory.redis_port = (uint16_t)r_port;
    const char *r_prefix = sc_json_get_string(obj, "redis_key_prefix");
    if (r_prefix) {
        if (cfg->memory.redis_key_prefix)
            a->free(a->ctx, cfg->memory.redis_key_prefix, strlen(cfg->memory.redis_key_prefix) + 1);
        cfg->memory.redis_key_prefix = sc_strdup(a, r_prefix);
    }
    const char *api_url = sc_json_get_string(obj, "api_base_url");
    if (api_url) {
        if (cfg->memory.api_base_url)
            a->free(a->ctx, cfg->memory.api_base_url, strlen(cfg->memory.api_base_url) + 1);
        cfg->memory.api_base_url = sc_strdup(a, api_url);
    }
    const char *api_k = sc_json_get_string(obj, "api_key");
    if (api_k) {
        if (cfg->memory.api_key)
            a->free(a->ctx, cfg->memory.api_key, strlen(cfg->memory.api_key) + 1);
        cfg->memory.api_key = sc_strdup(a, api_k);
    }
    double api_tm = sc_json_get_number(obj, "api_timeout_ms", cfg->memory.api_timeout_ms);
    if (api_tm >= 0 && api_tm <= 300000)
        cfg->memory.api_timeout_ms = (uint32_t)api_tm;
    return SC_OK;
}

static sc_error_t parse_tools(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    double sts = sc_json_get_number(obj, "shell_timeout_secs", cfg->tools.shell_timeout_secs);
    if (sts >= 1 && sts <= 86400)
        cfg->tools.shell_timeout_secs = (uint64_t)sts;
    double smob =
        sc_json_get_number(obj, "shell_max_output_bytes", cfg->tools.shell_max_output_bytes);
    if (smob >= 0 && smob <= 1073741824)
        cfg->tools.shell_max_output_bytes = (uint32_t)smob;
    double mfsb = sc_json_get_number(obj, "max_file_size_bytes", cfg->tools.max_file_size_bytes);
    if (mfsb >= 0 && mfsb <= 1073741824)
        cfg->tools.max_file_size_bytes = (uint32_t)mfsb;
    double wfmc = sc_json_get_number(obj, "web_fetch_max_chars", cfg->tools.web_fetch_max_chars);
    if (wfmc >= 0 && wfmc <= 10000000)
        cfg->tools.web_fetch_max_chars = (uint32_t)wfmc;
    const char *provider = sc_json_get_string(obj, "web_search_provider");
    if (provider && provider[0]) {
        if (cfg->tools.web_search_provider)
            a->free(a->ctx, cfg->tools.web_search_provider,
                    strlen(cfg->tools.web_search_provider) + 1);
        cfg->tools.web_search_provider = sc_strdup(a, provider);
    }
    sc_json_value_t *en = sc_json_object_get(obj, "enabled_tools");
    if (en && en->type == SC_JSON_ARRAY) {
        if (cfg->tools.enabled_tools) {
            for (size_t i = 0; i < cfg->tools.enabled_tools_len; i++)
                a->free(a->ctx, cfg->tools.enabled_tools[i],
                        strlen(cfg->tools.enabled_tools[i]) + 1);
            a->free(a->ctx, cfg->tools.enabled_tools,
                    cfg->tools.enabled_tools_len * sizeof(char *));
        }
        parse_string_array(a, &cfg->tools.enabled_tools, &cfg->tools.enabled_tools_len, en);
    }
    sc_json_value_t *dis = sc_json_object_get(obj, "disabled_tools");
    if (dis && dis->type == SC_JSON_ARRAY) {
        if (cfg->tools.disabled_tools) {
            for (size_t i = 0; i < cfg->tools.disabled_tools_len; i++)
                a->free(a->ctx, cfg->tools.disabled_tools[i],
                        strlen(cfg->tools.disabled_tools[i]) + 1);
            a->free(a->ctx, cfg->tools.disabled_tools,
                    cfg->tools.disabled_tools_len * sizeof(char *));
        }
        parse_string_array(a, &cfg->tools.disabled_tools, &cfg->tools.disabled_tools_len, dis);
    }
    sc_json_value_t *tmo = sc_json_object_get(obj, "tool_model_overrides");
    if (tmo && tmo->type == SC_JSON_OBJECT && tmo->data.object.pairs) {
        for (size_t i = 0; i < cfg->tools.model_overrides_len; i++) {
            sc_tool_model_override_t *o = &cfg->tools.model_overrides[i];
            if (o->tool_name) {
                a->free(a->ctx, o->tool_name, strlen(o->tool_name) + 1);
                o->tool_name = NULL;
            }
            if (o->provider) {
                a->free(a->ctx, o->provider, strlen(o->provider) + 1);
                o->provider = NULL;
            }
            if (o->model) {
                a->free(a->ctx, o->model, strlen(o->model) + 1);
                o->model = NULL;
            }
        }
        cfg->tools.model_overrides_len = 0;
        for (size_t i = 0; i < tmo->data.object.len &&
                           cfg->tools.model_overrides_len < SC_TOOL_MODEL_OVERRIDES_MAX;
             i++) {
            sc_json_pair_t *p = &tmo->data.object.pairs[i];
            if (!p->key || !p->value || p->value->type != SC_JSON_OBJECT)
                continue;
            const char *prov = sc_json_get_string(p->value, "provider");
            const char *mod = sc_json_get_string(p->value, "model");
            if (!prov || !prov[0] || !mod || !mod[0])
                continue;
            sc_tool_model_override_t *o =
                &cfg->tools.model_overrides[cfg->tools.model_overrides_len];
            o->tool_name = sc_strdup(a, p->key);
            o->provider = sc_strdup(a, prov);
            o->model = sc_strdup(a, mod);
            if (o->tool_name && o->provider && o->model)
                cfg->tools.model_overrides_len++;
            else {
                if (o->tool_name)
                    a->free(a->ctx, o->tool_name, strlen(o->tool_name) + 1);
                if (o->provider)
                    a->free(a->ctx, o->provider, strlen(o->provider) + 1);
                if (o->model)
                    a->free(a->ctx, o->model, strlen(o->model) + 1);
            }
        }
    }
    return SC_OK;
}

static sc_error_t parse_runtime(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    const char *kind = sc_json_get_string(obj, "kind");
    if (!kind)
        kind = sc_json_get_string(obj, "type");
    if (kind) {
        if (cfg->runtime.kind)
            a->free(a->ctx, cfg->runtime.kind, strlen(cfg->runtime.kind) + 1);
        cfg->runtime.kind = sc_strdup(a, kind);
    }
    const char *docker_image = sc_json_get_string(obj, "docker_image");
    if (docker_image) {
        if (cfg->runtime.docker_image)
            a->free(a->ctx, cfg->runtime.docker_image, strlen(cfg->runtime.docker_image) + 1);
        cfg->runtime.docker_image = sc_strdup(a, docker_image);
    }
    const char *gce_project = sc_json_get_string(obj, "gce_project");
    if (gce_project) {
        if (cfg->runtime.gce_project)
            a->free(a->ctx, cfg->runtime.gce_project, strlen(cfg->runtime.gce_project) + 1);
        cfg->runtime.gce_project = sc_strdup(a, gce_project);
    }
    const char *gce_zone = sc_json_get_string(obj, "gce_zone");
    if (gce_zone) {
        if (cfg->runtime.gce_zone)
            a->free(a->ctx, cfg->runtime.gce_zone, strlen(cfg->runtime.gce_zone) + 1);
        cfg->runtime.gce_zone = sc_strdup(a, gce_zone);
    }
    const char *gce_instance = sc_json_get_string(obj, "gce_instance");
    if (gce_instance) {
        if (cfg->runtime.gce_instance)
            a->free(a->ctx, cfg->runtime.gce_instance, strlen(cfg->runtime.gce_instance) + 1);
        cfg->runtime.gce_instance = sc_strdup(a, gce_instance);
    }
    return SC_OK;
}

static sc_error_t parse_tunnel(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    const char *provider = sc_json_get_string(obj, "provider");
    if (provider) {
        if (cfg->tunnel.provider)
            a->free(a->ctx, cfg->tunnel.provider, strlen(cfg->tunnel.provider) + 1);
        cfg->tunnel.provider = sc_strdup(a, provider);
    }
    const char *domain = sc_json_get_string(obj, "domain");
    if (domain) {
        if (cfg->tunnel.domain)
            a->free(a->ctx, cfg->tunnel.domain, strlen(cfg->tunnel.domain) + 1);
        cfg->tunnel.domain = sc_strdup(a, domain);
    }
    return SC_OK;
}

static sc_error_t parse_mcp_servers(sc_allocator_t *a, sc_config_t *cfg,
                                    const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->mcp_servers_len = 0;
    for (size_t i = 0; i < obj->data.object.len && cfg->mcp_servers_len < SC_MCP_SERVERS_MAX; i++) {
        sc_json_pair_t *p = &obj->data.object.pairs[i];
        if (!p->key || !p->value || p->value->type != SC_JSON_OBJECT)
            continue;

        sc_mcp_server_entry_t *entry = &cfg->mcp_servers[cfg->mcp_servers_len];
        memset(entry, 0, sizeof(*entry));
        entry->name = sc_strdup(a, p->key);

        const char *cmd = sc_json_get_string(p->value, "command");
        if (cmd)
            entry->command = sc_strdup(a, cmd);

        sc_json_value_t *args_arr = sc_json_object_get(p->value, "args");
        if (args_arr && args_arr->type == SC_JSON_ARRAY) {
            for (size_t j = 0;
                 j < args_arr->data.array.len && entry->args_count < SC_MCP_SERVER_ARGS_MAX; j++) {
                sc_json_value_t *arg_val = args_arr->data.array.items[j];
                if (arg_val && arg_val->type == SC_JSON_STRING && arg_val->data.string.ptr) {
                    entry->args[entry->args_count++] = sc_strdup(a, arg_val->data.string.ptr);
                }
            }
        }
        cfg->mcp_servers_len++;
    }
    return SC_OK;
}

static sc_error_t parse_policy_cfg(sc_allocator_t *a, sc_config_t *cfg,
                                   const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->policy.enabled = sc_json_get_bool(obj, "enabled", cfg->policy.enabled);
    const char *rj = sc_json_get_string(obj, "rules_json");
    if (rj) {
        if (cfg->policy.rules_json)
            a->free(a->ctx, cfg->policy.rules_json, strlen(cfg->policy.rules_json) + 1);
        cfg->policy.rules_json = sc_strdup(a, rj);
    }
    return SC_OK;
}
static sc_error_t parse_plugins_cfg(sc_allocator_t *a, sc_config_t *cfg,
                                    const sc_json_value_t *obj) {
    if (!obj)
        return SC_OK;
    /* Support "plugins" as object {"enabled": true, "paths": [...]} or as array ["a.so", "b.so"] */
    const sc_json_value_t *paths_arr = NULL;
    if (obj->type == SC_JSON_OBJECT) {
        cfg->plugins.enabled = sc_json_get_bool(obj, "enabled", cfg->plugins.enabled);
        paths_arr = sc_json_object_get(obj, "paths");
        const char *pd = sc_json_get_string(obj, "plugin_dir");
        if (pd) {
            if (cfg->plugins.plugin_dir)
                a->free(a->ctx, cfg->plugins.plugin_dir, strlen(cfg->plugins.plugin_dir) + 1);
            cfg->plugins.plugin_dir = sc_strdup(a, pd);
        }
    } else if (obj->type == SC_JSON_ARRAY) {
        paths_arr = obj;
    } else {
        return SC_OK;
    }
    if (!paths_arr || paths_arr->type != SC_JSON_ARRAY)
        return SC_OK;
    size_t n = paths_arr->data.array.len;
    if (n == 0)
        return SC_OK;
    /* Free existing plugin_paths if re-parsing */
    if (cfg->plugins.plugin_paths) {
        for (size_t i = 0; i < cfg->plugins.plugin_paths_len; i++)
            if (cfg->plugins.plugin_paths[i])
                a->free(a->ctx, cfg->plugins.plugin_paths[i],
                        strlen(cfg->plugins.plugin_paths[i]) + 1);
        a->free(a->ctx, cfg->plugins.plugin_paths, cfg->plugins.plugin_paths_len * sizeof(char *));
        cfg->plugins.plugin_paths = NULL;
        cfg->plugins.plugin_paths_len = 0;
    }
    cfg->plugins.plugin_paths = (char **)a->alloc(a->ctx, n * sizeof(char *));
    if (!cfg->plugins.plugin_paths)
        return SC_ERR_OUT_OF_MEMORY;
    for (size_t i = 0; i < n; i++) {
        const sc_json_value_t *item = paths_arr->data.array.items[i];
        if (item && item->type == SC_JSON_STRING && item->data.string.ptr)
            cfg->plugins.plugin_paths[i] =
                sc_strndup(a, item->data.string.ptr, item->data.string.len);
        else
            cfg->plugins.plugin_paths[i] = NULL;
    }
    cfg->plugins.plugin_paths_len = n;
    return SC_OK;
}
static sc_error_t parse_heartbeat(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    (void)a;
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->heartbeat.enabled = sc_json_get_bool(obj, "enabled", cfg->heartbeat.enabled);
    double im = sc_json_get_number(obj, "interval_minutes", cfg->heartbeat.interval_minutes);
    if (!im)
        im = sc_json_get_number(obj, "interval_secs", 0);
    if (im > 0 && im <= 1440)
        cfg->heartbeat.interval_minutes = (uint32_t)im;
    return SC_OK;
}

static sc_error_t parse_reliability(sc_allocator_t *a, sc_config_t *cfg,
                                    const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    const char *pp = sc_json_get_string(obj, "primary_provider");
    if (pp) {
        if (cfg->reliability.primary_provider)
            a->free(a->ctx, cfg->reliability.primary_provider,
                    strlen(cfg->reliability.primary_provider) + 1);
        cfg->reliability.primary_provider = sc_strdup(a, pp);
    }
    double pr = sc_json_get_number(obj, "provider_retries", cfg->reliability.provider_retries);
    if (pr >= 0 && pr <= 20)
        cfg->reliability.provider_retries = (uint32_t)pr;
    double pbm =
        sc_json_get_number(obj, "provider_backoff_ms", cfg->reliability.provider_backoff_ms);
    if (pbm >= 0)
        cfg->reliability.provider_backoff_ms = (uint64_t)pbm;
    double cibs = sc_json_get_number(obj, "channel_initial_backoff_secs",
                                     cfg->reliability.channel_initial_backoff_secs);
    if (cibs >= 0)
        cfg->reliability.channel_initial_backoff_secs = (uint64_t)cibs;
    double cmbs = sc_json_get_number(obj, "channel_max_backoff_secs",
                                     cfg->reliability.channel_max_backoff_secs);
    if (cmbs >= 0)
        cfg->reliability.channel_max_backoff_secs = (uint64_t)cmbs;
    double sps =
        sc_json_get_number(obj, "scheduler_poll_secs", cfg->reliability.scheduler_poll_secs);
    if (sps >= 0)
        cfg->reliability.scheduler_poll_secs = (uint64_t)sps;
    double sr = sc_json_get_number(obj, "scheduler_retries", cfg->reliability.scheduler_retries);
    if (sr >= 0 && sr <= 20)
        cfg->reliability.scheduler_retries = (uint32_t)sr;
    sc_json_value_t *fp = sc_json_object_get(obj, "fallback_providers");
    if (fp && fp->type == SC_JSON_ARRAY) {
        if (cfg->reliability.fallback_providers) {
            for (size_t i = 0; i < cfg->reliability.fallback_providers_len; i++)
                a->free(a->ctx, cfg->reliability.fallback_providers[i],
                        strlen(cfg->reliability.fallback_providers[i]) + 1);
            a->free(a->ctx, cfg->reliability.fallback_providers,
                    cfg->reliability.fallback_providers_len * sizeof(char *));
        }
        parse_string_array(a, &cfg->reliability.fallback_providers,
                           &cfg->reliability.fallback_providers_len, fp);
    }
    return SC_OK;
}

static sc_error_t parse_session(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    (void)a;
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    double im = sc_json_get_number(obj, "idle_minutes", cfg->session.idle_minutes);
    if (im >= 0 && im <= 1440)
        cfg->session.idle_minutes = (uint32_t)im;
    const char *scope = sc_json_get_string(obj, "dm_scope");
    if (scope) {
        if (strcmp(scope, "main") == 0)
            cfg->session.dm_scope = DirectScopeMain;
        else if (strcmp(scope, "per_peer") == 0)
            cfg->session.dm_scope = DirectScopePerPeer;
        else if (strcmp(scope, "per_channel_peer") == 0)
            cfg->session.dm_scope = DirectScopePerChannelPeer;
        else if (strcmp(scope, "per_account_channel_peer") == 0)
            cfg->session.dm_scope = DirectScopePerAccountChannelPeer;
    }
    return SC_OK;
}

static sc_error_t parse_peripherals(sc_allocator_t *a, sc_config_t *cfg,
                                    const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->peripherals.enabled = sc_json_get_bool(obj, "enabled", cfg->peripherals.enabled);
    const char *dd = sc_json_get_string(obj, "datasheet_dir");
    if (dd) {
        if (cfg->peripherals.datasheet_dir)
            a->free(a->ctx, cfg->peripherals.datasheet_dir,
                    strlen(cfg->peripherals.datasheet_dir) + 1);
        cfg->peripherals.datasheet_dir = sc_strdup(a, dd);
    }
    return SC_OK;
}

static sc_error_t parse_hardware(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->hardware.enabled = sc_json_get_bool(obj, "enabled", cfg->hardware.enabled);
    const char *transport = sc_json_get_string(obj, "transport");
    if (transport) {
        if (cfg->hardware.transport)
            a->free(a->ctx, cfg->hardware.transport, strlen(cfg->hardware.transport) + 1);
        cfg->hardware.transport = sc_strdup(a, transport);
    }
    const char *serial_port = sc_json_get_string(obj, "serial_port");
    if (serial_port) {
        if (cfg->hardware.serial_port)
            a->free(a->ctx, cfg->hardware.serial_port, strlen(cfg->hardware.serial_port) + 1);
        cfg->hardware.serial_port = sc_strdup(a, serial_port);
    }
    double br = sc_json_get_number(obj, "baud_rate", cfg->hardware.baud_rate);
    if (br >= 0 && br <= 4000000)
        cfg->hardware.baud_rate = (uint32_t)br;
    const char *probe_target = sc_json_get_string(obj, "probe_target");
    if (probe_target) {
        if (cfg->hardware.probe_target)
            a->free(a->ctx, cfg->hardware.probe_target, strlen(cfg->hardware.probe_target) + 1);
        cfg->hardware.probe_target = sc_strdup(a, probe_target);
    }
    return SC_OK;
}

static sc_error_t parse_browser(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    (void)a;
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->browser.enabled = sc_json_get_bool(obj, "enabled", cfg->browser.enabled);
    return SC_OK;
}

static sc_error_t parse_cost(sc_allocator_t *a, sc_config_t *cfg, const sc_json_value_t *obj) {
    (void)a;
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    cfg->cost.enabled = sc_json_get_bool(obj, "enabled", cfg->cost.enabled);
    double dl = sc_json_get_number(obj, "daily_limit_usd", cfg->cost.daily_limit_usd);
    if (dl >= 0 && dl <= 1000000.0)
        cfg->cost.daily_limit_usd = dl;
    double ml = sc_json_get_number(obj, "monthly_limit_usd", cfg->cost.monthly_limit_usd);
    if (ml >= 0 && ml <= 10000000.0)
        cfg->cost.monthly_limit_usd = ml;
    double wp = sc_json_get_number(obj, "warn_at_percent", cfg->cost.warn_at_percent);
    if (wp >= 0 && wp <= 100)
        cfg->cost.warn_at_percent = (uint8_t)wp;
    cfg->cost.allow_override = sc_json_get_bool(obj, "allow_override", cfg->cost.allow_override);
    return SC_OK;
}

static sc_error_t parse_diagnostics(sc_allocator_t *a, sc_config_t *cfg,
                                    const sc_json_value_t *obj) {
    if (!obj || obj->type != SC_JSON_OBJECT)
        return SC_OK;
    const char *backend = sc_json_get_string(obj, "backend");
    if (backend) {
        if (cfg->diagnostics.backend)
            a->free(a->ctx, cfg->diagnostics.backend, strlen(cfg->diagnostics.backend) + 1);
        cfg->diagnostics.backend = sc_strdup(a, backend);
    }
    const char *ote = sc_json_get_string(obj, "otel_endpoint");
    if (ote) {
        if (cfg->diagnostics.otel_endpoint)
            a->free(a->ctx, cfg->diagnostics.otel_endpoint,
                    strlen(cfg->diagnostics.otel_endpoint) + 1);
        cfg->diagnostics.otel_endpoint = sc_strdup(a, ote);
    }
    const char *ots = sc_json_get_string(obj, "otel_service_name");
    if (ots) {
        if (cfg->diagnostics.otel_service_name)
            a->free(a->ctx, cfg->diagnostics.otel_service_name,
                    strlen(cfg->diagnostics.otel_service_name) + 1);
        cfg->diagnostics.otel_service_name = sc_strdup(a, ots);
    }
    cfg->diagnostics.log_tool_calls =
        sc_json_get_bool(obj, "log_tool_calls", cfg->diagnostics.log_tool_calls);
    cfg->diagnostics.log_message_receipts =
        sc_json_get_bool(obj, "log_message_receipts", cfg->diagnostics.log_message_receipts);
    cfg->diagnostics.log_message_payloads =
        sc_json_get_bool(obj, "log_message_payloads", cfg->diagnostics.log_message_payloads);
    cfg->diagnostics.log_llm_io = sc_json_get_bool(obj, "log_llm_io", cfg->diagnostics.log_llm_io);
    if (cfg->diagnostics.log_llm_io || cfg->diagnostics.log_message_payloads) {
        fprintf(stderr, "[SECURITY WARNING] Diagnostic logging of payloads is enabled. "
                        "Logs may contain sensitive data (PII, API keys). "
                        "Do not use in production.\n");
    }
    return SC_OK;
}

sc_error_t sc_config_parse_json(sc_config_t *cfg, const char *content, size_t len) {
    if (!cfg || !content)
        return SC_ERR_INVALID_ARGUMENT;
    sc_allocator_t *a = &cfg->allocator;

    sc_json_value_t *root = NULL;
    sc_error_t err = sc_json_parse(a, content, len, &root);
    if (err != SC_OK)
        return err;
    if (!root || root->type != SC_JSON_OBJECT) {
        if (root)
            sc_json_free(a, root);
        return SC_ERR_JSON_PARSE;
    }

    sc_error_t mig_err = sc_config_migrate(a, root);
    if (mig_err != SC_OK) {
        sc_json_free(a, root);
        return mig_err;
    }

    cfg->config_version = (int)sc_json_get_number(root, "config_version", 1.0);

    const char *workspace = sc_json_get_string(root, "workspace");
    if (workspace && strstr(workspace, "..")) {
        workspace = NULL; /* Reject path traversal */
    }
    if (workspace && cfg->workspace_dir_override) {
        a->free(a->ctx, cfg->workspace_dir_override, strlen(cfg->workspace_dir_override) + 1);
    }
    if (workspace) {
        cfg->workspace_dir_override = sc_strdup(a, workspace);
        if (cfg->workspace_dir)
            a->free(a->ctx, cfg->workspace_dir, strlen(cfg->workspace_dir) + 1);
        cfg->workspace_dir = sc_strdup(a, workspace);
    }

    const char *prov = sc_json_get_string(root, "default_provider");
    if (prov) {
        if (cfg->default_provider)
            a->free(a->ctx, cfg->default_provider, strlen(cfg->default_provider) + 1);
        cfg->default_provider = sc_strdup(a, prov);
    }
    const char *model = sc_json_get_string(root, "default_model");
    if (model && model[0]) {
        if (cfg->default_model)
            a->free(a->ctx, cfg->default_model, strlen(cfg->default_model) + 1);
        cfg->default_model = sc_strdup(a, model);
    }
    double temp = sc_json_get_number(root, "default_temperature", cfg->default_temperature);
    if (temp >= 0.0 && temp <= 2.0)
        cfg->default_temperature = temp;

    double mt = sc_json_get_number(root, "max_tokens", (double)cfg->max_tokens);
    if (mt >= 0.0 && mt <= 1000000)
        cfg->max_tokens = (uint32_t)mt;

    sc_json_value_t *prov_arr = sc_json_object_get(root, "providers");
    if (prov_arr) {
        if (prov_arr->type == SC_JSON_ARRAY)
            parse_providers(a, cfg, prov_arr);
        else if (prov_arr->type == SC_JSON_OBJECT) {
            sc_json_value_t *router_obj = sc_json_object_get(prov_arr, "router");
            if (router_obj)
                parse_router(a, cfg, router_obj);
        }
    }

    sc_json_value_t *router_obj = sc_json_object_get(root, "router");
    if (router_obj)
        parse_router(a, cfg, router_obj);

    sc_json_value_t *aut = sc_json_object_get(root, "autonomy");
    if (aut)
        parse_autonomy(a, cfg, aut);

    sc_json_value_t *gw = sc_json_object_get(root, "gateway");
    if (gw)
        parse_gateway(a, cfg, gw);

    sc_json_value_t *mem = sc_json_object_get(root, "memory");
    if (mem)
        parse_memory(a, cfg, mem);

    sc_json_value_t *tools_obj = sc_json_object_get(root, "tools");
    if (tools_obj)
        parse_tools(a, cfg, tools_obj);

    sc_json_value_t *cron_obj = sc_json_object_get(root, "cron");
    if (cron_obj)
        parse_cron(a, cfg, cron_obj);

    sc_json_value_t *sched_obj = sc_json_object_get(root, "scheduler");
    if (sched_obj)
        parse_scheduler(a, cfg, sched_obj);

    sc_json_value_t *rt_obj = sc_json_object_get(root, "runtime");
    if (rt_obj)
        parse_runtime(a, cfg, rt_obj);

    sc_json_value_t *tunnel_obj = sc_json_object_get(root, "tunnel");
    if (tunnel_obj)
        parse_tunnel(a, cfg, tunnel_obj);

    sc_json_value_t *ch_obj = sc_json_object_get(root, "channels");
    if (ch_obj)
        parse_channels(a, cfg, ch_obj);

    sc_json_value_t *agent_obj = sc_json_object_get(root, "agent");
    if (agent_obj)
        parse_agent(a, cfg, agent_obj);

    sc_json_value_t *heartbeat_obj = sc_json_object_get(root, "heartbeat");
    if (heartbeat_obj)
        parse_heartbeat(a, cfg, heartbeat_obj);

    sc_json_value_t *reliability_obj = sc_json_object_get(root, "reliability");
    if (reliability_obj)
        parse_reliability(a, cfg, reliability_obj);

    sc_json_value_t *diagnostics_obj = sc_json_object_get(root, "diagnostics");
    if (diagnostics_obj)
        parse_diagnostics(a, cfg, diagnostics_obj);

    sc_json_value_t *session_obj = sc_json_object_get(root, "session");
    if (session_obj)
        parse_session(a, cfg, session_obj);

    sc_json_value_t *peripherals_obj = sc_json_object_get(root, "peripherals");
    if (peripherals_obj)
        parse_peripherals(a, cfg, peripherals_obj);

    sc_json_value_t *hardware_obj = sc_json_object_get(root, "hardware");
    if (hardware_obj)
        parse_hardware(a, cfg, hardware_obj);

    sc_json_value_t *browser_obj = sc_json_object_get(root, "browser");
    if (browser_obj)
        parse_browser(a, cfg, browser_obj);

    sc_json_value_t *cost_obj = sc_json_object_get(root, "cost");
    if (cost_obj)
        parse_cost(a, cfg, cost_obj);

    sc_json_value_t *mcp_obj = sc_json_object_get(root, "mcp_servers");
    if (mcp_obj)
        parse_mcp_servers(a, cfg, mcp_obj);

    sc_json_value_t *nodes_obj = sc_json_object_get(root, "nodes");
    if (nodes_obj)
        parse_nodes(a, cfg, nodes_obj);

    sc_json_value_t *policy_obj = sc_json_object_get(root, "policy");
    if (policy_obj)
        parse_policy_cfg(a, cfg, policy_obj);
    sc_json_value_t *plugins_obj = sc_json_object_get(root, "plugins");
    if (plugins_obj)
        parse_plugins_cfg(a, cfg, plugins_obj);
    sc_json_value_t *sec = sc_json_object_get(root, "security");
    if (sec && sec->type == SC_JSON_OBJECT) {
        double al = sc_json_get_number(sec, "autonomy_level", cfg->security.autonomy_level);
        if (al < 0)
            al = 0;
        else if (al > 4)
            al = 4;
        cfg->security.autonomy_level = (uint8_t)al;
        const char *sb = sc_json_get_string(sec, "sandbox");
        if (sb) {
            if (cfg->security.sandbox)
                a->free(a->ctx, cfg->security.sandbox, strlen(cfg->security.sandbox) + 1);
            cfg->security.sandbox = sc_strdup(a, sb);
            cfg->security.sandbox_config.backend = sc_config_parse_sandbox_backend(sb);
        }
        sc_json_value_t *sbox = sc_json_object_get(sec, "sandbox_config");
        if (sbox && sbox->type == SC_JSON_OBJECT) {
            cfg->security.sandbox_config.enabled =
                sc_json_get_bool(sbox, "enabled", cfg->security.sandbox_config.enabled);
            const char *be = sc_json_get_string(sbox, "backend");
            if (be)
                cfg->security.sandbox_config.backend = sc_config_parse_sandbox_backend(be);
            sc_json_value_t *fa = sc_json_object_get(sbox, "firejail_args");
            if (fa && fa->type == SC_JSON_ARRAY) {
                if (cfg->security.sandbox_config.firejail_args) {
                    for (size_t i = 0; i < cfg->security.sandbox_config.firejail_args_len; i++)
                        a->free(a->ctx, cfg->security.sandbox_config.firejail_args[i],
                                strlen(cfg->security.sandbox_config.firejail_args[i]) + 1);
                    a->free(a->ctx, cfg->security.sandbox_config.firejail_args,
                            cfg->security.sandbox_config.firejail_args_len * sizeof(char *));
                }
                parse_string_array(a, &cfg->security.sandbox_config.firejail_args,
                                   &cfg->security.sandbox_config.firejail_args_len, fa);
            }
            sc_json_value_t *np = sc_json_object_get(sbox, "net_proxy");
            if (np && np->type == SC_JSON_OBJECT) {
                cfg->security.sandbox_config.net_proxy.enabled =
                    sc_json_get_bool(np, "enabled", false);
                cfg->security.sandbox_config.net_proxy.deny_all =
                    sc_json_get_bool(np, "deny_all", true);
                const char *pa = sc_json_get_string(np, "proxy_addr");
                if (pa) {
                    if (cfg->security.sandbox_config.net_proxy.proxy_addr)
                        a->free(a->ctx, cfg->security.sandbox_config.net_proxy.proxy_addr,
                                strlen(cfg->security.sandbox_config.net_proxy.proxy_addr) + 1);
                    cfg->security.sandbox_config.net_proxy.proxy_addr = sc_strdup(a, pa);
                }
                sc_json_value_t *ad = sc_json_object_get(np, "allowed_domains");
                if (ad && ad->type == SC_JSON_ARRAY) {
                    if (cfg->security.sandbox_config.net_proxy.allowed_domains) {
                        for (size_t i = 0;
                             i < cfg->security.sandbox_config.net_proxy.allowed_domains_len; i++)
                            a->free(
                                a->ctx, cfg->security.sandbox_config.net_proxy.allowed_domains[i],
                                strlen(cfg->security.sandbox_config.net_proxy.allowed_domains[i]) +
                                    1);
                        a->free(a->ctx, cfg->security.sandbox_config.net_proxy.allowed_domains,
                                cfg->security.sandbox_config.net_proxy.allowed_domains_len *
                                    sizeof(char *));
                    }
                    parse_string_array(a, &cfg->security.sandbox_config.net_proxy.allowed_domains,
                                       &cfg->security.sandbox_config.net_proxy.allowed_domains_len,
                                       ad);
                }
            }
        }
        sc_json_value_t *res = sc_json_object_get(sec, "resources");
        if (res && res->type == SC_JSON_OBJECT) {
            double mfs = sc_json_get_number(res, "max_file_size",
                                            cfg->security.resource_limits.max_file_size);
            if (mfs >= 0)
                cfg->security.resource_limits.max_file_size = (uint64_t)mfs;
            double mrs = sc_json_get_number(res, "max_read_size",
                                            cfg->security.resource_limits.max_read_size);
            if (mrs >= 0)
                cfg->security.resource_limits.max_read_size = (uint64_t)mrs;
            double mmb = sc_json_get_number(res, "max_memory_mb",
                                            cfg->security.resource_limits.max_memory_mb);
            if (mmb >= 0 && mmb <= 1048576)
                cfg->security.resource_limits.max_memory_mb = (uint32_t)mmb;
        }
        sc_json_value_t *aud = sc_json_object_get(sec, "audit");
        if (aud && aud->type == SC_JSON_OBJECT) {
            cfg->security.audit.enabled =
                sc_json_get_bool(aud, "enabled", cfg->security.audit.enabled);
            const char *lp = sc_json_get_string(aud, "log_path");
            if (!lp)
                lp = sc_json_get_string(aud, "log_file");
            if (lp) {
                if (cfg->security.audit.log_path)
                    a->free(a->ctx, cfg->security.audit.log_path,
                            strlen(cfg->security.audit.log_path) + 1);
                cfg->security.audit.log_path = sc_strdup(a, lp);
            }
        }
    }

    sc_json_value_t *gw_obj = sc_json_object_get(root, "gateway");
    if (gw_obj) {
        double port = sc_json_get_number(gw_obj, "port", cfg->gateway.port);
        if (port < 1)
            port = 1;
        else if (port > 65535)
            port = 65535;
        cfg->gateway.port = (uint16_t)port;
    }

    const char *api_k_root = sc_json_get_string(root, "api_key");
    if (api_k_root && api_k_root[0]) {
        if (cfg->api_key)
            a->free(a->ctx, cfg->api_key, strlen(cfg->api_key) + 1);
        cfg->api_key = sc_strdup(a, api_k_root);
    }

    sc_json_value_t *secrets_obj = sc_json_object_get(root, "secrets");
    if (secrets_obj && secrets_obj->type == SC_JSON_OBJECT) {
        cfg->secrets.encrypt = sc_json_get_bool(secrets_obj, "encrypt", cfg->secrets.encrypt);
    }

    sc_json_value_t *identity_obj = sc_json_object_get(root, "identity");
    if (identity_obj && identity_obj->type == SC_JSON_OBJECT) {
        const char *fmt = sc_json_get_string(identity_obj, "format");
        if (fmt) {
            if (cfg->identity.format)
                a->free(a->ctx, cfg->identity.format, strlen(cfg->identity.format) + 1);
            cfg->identity.format = sc_strdup(a, fmt);
        }
    }

    sc_json_free(a, root);
    return SC_OK;
}

sc_error_t sc_config_parse_string_array(sc_allocator_t *alloc, const sc_json_value_t *arr,
                                        char ***out_strings, size_t *out_count) {
    if (!alloc || !out_strings || !out_count)
        return SC_ERR_INVALID_ARGUMENT;
    *out_strings = NULL;
    *out_count = 0;

    if (!arr || arr->type != SC_JSON_ARRAY)
        return SC_OK;

    size_t n = 0;
    sc_json_value_t **items = arr->data.array.items;
    if (!items)
        return SC_OK;
    for (size_t i = 0; i < arr->data.array.len; i++) {
        if (items[i] && items[i]->type == SC_JSON_STRING)
            n++;
    }
    if (n == 0)
        return SC_OK;

    char **strings = (char **)alloc->alloc(alloc->ctx, sizeof(char *) * n);
    if (!strings)
        return SC_ERR_OUT_OF_MEMORY;

    size_t j = 0;
    for (size_t i = 0; i < arr->data.array.len && j < n; i++) {
        if (!items[i] || items[i]->type != SC_JSON_STRING)
            continue;
        const char *s = items[i]->data.string.ptr;
        size_t len = items[i]->data.string.len;
        strings[j] = len > 0 ? sc_strndup(alloc, s, len) : sc_strdup(alloc, "");
        if (!strings[j]) {
            while (j > 0) {
                j--;
                alloc->free(alloc->ctx, strings[j], strlen(strings[j]) + 1);
            }
            alloc->free(alloc->ctx, strings, sizeof(char *) * n);
            return SC_ERR_OUT_OF_MEMORY;
        }
        j++;
    }

    *out_strings = strings;
    *out_count = j;
    return SC_OK;
}
