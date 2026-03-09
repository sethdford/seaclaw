#include "config_internal.h"
#include "seaclaw/config.h"
#include "seaclaw/core/error.h"
#include "seaclaw/core/json.h"
#include "seaclaw/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

sc_error_t sc_config_save(const sc_config_t *cfg) {
    if (!cfg || !cfg->config_path)
        return SC_ERR_INVALID_ARGUMENT;
    sc_allocator_t a = cfg->allocator;

    char dir_buf[SC_MAX_PATH];
    const char *home = getenv("HOME");
    if (home) {
        int n = snprintf(dir_buf, sizeof(dir_buf), "%s/%s", home, SC_CONFIG_DIR);
        if (n > 0 && (size_t)n < sizeof(dir_buf))
            (void)mkdir(dir_buf, 0700);
    }

    sc_json_value_t *root = sc_json_object_new(&a);
    if (!root)
        return SC_ERR_OUT_OF_MEMORY;

    if (cfg->workspace_dir) {
        sc_json_value_t *ws =
            sc_json_string_new(&a, cfg->workspace_dir, strlen(cfg->workspace_dir));
        if (ws)
            sc_json_object_set(&a, root, "workspace", ws);
    }
    if (cfg->default_provider) {
        sc_json_value_t *dp =
            sc_json_string_new(&a, cfg->default_provider, strlen(cfg->default_provider));
        if (dp)
            sc_json_object_set(&a, root, "default_provider", dp);
    }
    if (cfg->default_model) {
        sc_json_value_t *dm =
            sc_json_string_new(&a, cfg->default_model, strlen(cfg->default_model));
        if (dm)
            sc_json_object_set(&a, root, "default_model", dm);
    }
    sc_json_object_set(&a, root, "default_temperature",
                       sc_json_number_new(&a, cfg->default_temperature));
    if (cfg->max_tokens > 0)
        sc_json_object_set(&a, root, "max_tokens", sc_json_number_new(&a, (double)cfg->max_tokens));

    sc_json_object_set(&a, root, "temperature", sc_json_number_new(&a, cfg->temperature));

    /* gateway */
    sc_json_value_t *gw = sc_json_object_new(&a);
    if (gw) {
        sc_json_object_set(&a, gw, "enabled", sc_json_bool_new(&a, cfg->gateway.enabled));
        sc_json_object_set(&a, gw, "port", sc_json_number_new(&a, cfg->gateway.port));
        if (cfg->gateway.host)
            sc_json_object_set(
                &a, gw, "host",
                sc_json_string_new(&a, cfg->gateway.host, strlen(cfg->gateway.host)));
        if (cfg->gateway.control_ui_dir)
            sc_json_object_set(&a, gw, "control_ui_dir",
                               sc_json_string_new(&a, cfg->gateway.control_ui_dir,
                                                  strlen(cfg->gateway.control_ui_dir)));
        sc_json_object_set(&a, gw, "require_pairing",
                           sc_json_bool_new(&a, cfg->gateway.require_pairing));
        sc_json_object_set(&a, root, "gateway", gw);
    }

    /* memory */
    sc_json_value_t *mem = sc_json_object_new(&a);
    if (mem) {
        if (cfg->memory.backend)
            sc_json_object_set(
                &a, mem, "backend",
                sc_json_string_new(&a, cfg->memory.backend, strlen(cfg->memory.backend)));
        if (cfg->memory.sqlite_path)
            sc_json_object_set(
                &a, mem, "sqlite_path",
                sc_json_string_new(&a, cfg->memory.sqlite_path, strlen(cfg->memory.sqlite_path)));
        sc_json_object_set(&a, mem, "auto_save", sc_json_bool_new(&a, cfg->memory.auto_save));
        sc_json_object_set(&a, mem, "consolidation_interval_hours",
                           sc_json_number_new(&a, cfg->memory.consolidation_interval_hours));
        sc_json_object_set(&a, root, "memory", mem);
    }

    /* security */
    sc_json_value_t *sec = sc_json_object_new(&a);
    if (sec) {
        sc_json_object_set(&a, sec, "autonomy_level",
                           sc_json_number_new(&a, cfg->security.autonomy_level));
        if (cfg->security.sandbox)
            sc_json_object_set(
                &a, sec, "sandbox",
                sc_json_string_new(&a, cfg->security.sandbox, strlen(cfg->security.sandbox)));

        /* sandbox_config */
        sc_json_value_t *sbc = sc_json_object_new(&a);
        if (sbc) {
            sc_json_object_set(&a, sbc, "enabled",
                               sc_json_bool_new(&a, cfg->security.sandbox_config.enabled));

            const char *be_str =
                sc_config_sandbox_backend_to_string(cfg->security.sandbox_config.backend);
            sc_json_object_set(&a, sbc, "backend", sc_json_string_new(&a, be_str, strlen(be_str)));

            if (cfg->security.sandbox_config.firejail_args_len > 0 &&
                cfg->security.sandbox_config.firejail_args) {
                sc_json_value_t *fja = sc_json_array_new(&a);
                if (fja) {
                    for (size_t fi = 0; fi < cfg->security.sandbox_config.firejail_args_len; fi++) {
                        const char *arg = cfg->security.sandbox_config.firejail_args[fi];
                        if (arg)
                            sc_json_array_push(&a, fja, sc_json_string_new(&a, arg, strlen(arg)));
                    }
                    sc_json_object_set(&a, sbc, "firejail_args", fja);
                }
            }

            /* net_proxy */
            sc_json_value_t *np = sc_json_object_new(&a);
            if (np) {
                sc_json_object_set(
                    &a, np, "enabled",
                    sc_json_bool_new(&a, cfg->security.sandbox_config.net_proxy.enabled));
                sc_json_object_set(
                    &a, np, "deny_all",
                    sc_json_bool_new(&a, cfg->security.sandbox_config.net_proxy.deny_all));
                if (cfg->security.sandbox_config.net_proxy.proxy_addr)
                    sc_json_object_set(
                        &a, np, "proxy_addr",
                        sc_json_string_new(
                            &a, cfg->security.sandbox_config.net_proxy.proxy_addr,
                            strlen(cfg->security.sandbox_config.net_proxy.proxy_addr)));
                if (cfg->security.sandbox_config.net_proxy.allowed_domains_len > 0 &&
                    cfg->security.sandbox_config.net_proxy.allowed_domains) {
                    sc_json_value_t *da = sc_json_array_new(&a);
                    if (da) {
                        for (size_t di = 0;
                             di < cfg->security.sandbox_config.net_proxy.allowed_domains_len;
                             di++) {
                            const char *dom =
                                cfg->security.sandbox_config.net_proxy.allowed_domains[di];
                            if (dom)
                                sc_json_array_push(&a, da,
                                                   sc_json_string_new(&a, dom, strlen(dom)));
                        }
                        sc_json_object_set(&a, np, "allowed_domains", da);
                    }
                }
                sc_json_object_set(&a, sbc, "net_proxy", np);
            }

            sc_json_object_set(&a, sec, "sandbox_config", sbc);
        }

        sc_json_object_set(&a, root, "security", sec);
    }

    /* tools */
    sc_json_value_t *tools_obj = sc_json_object_new(&a);
    if (tools_obj) {
        if (cfg->tools.shell_timeout_secs > 0)
            sc_json_object_set(&a, tools_obj, "shell_timeout_secs",
                               sc_json_number_new(&a, (double)cfg->tools.shell_timeout_secs));
        if (cfg->tools.max_file_size_bytes > 0)
            sc_json_object_set(&a, tools_obj, "max_file_size_bytes",
                               sc_json_number_new(&a, (double)cfg->tools.max_file_size_bytes));
        sc_json_object_set(&a, root, "tools", tools_obj);
    }

    /* cost */
    sc_json_value_t *cost = sc_json_object_new(&a);
    if (cost) {
        sc_json_object_set(&a, cost, "enabled", sc_json_bool_new(&a, cfg->cost.enabled));
        if (cfg->cost.daily_limit_usd > 0)
            sc_json_object_set(&a, cost, "daily_limit_usd",
                               sc_json_number_new(&a, cfg->cost.daily_limit_usd));
        if (cfg->cost.monthly_limit_usd > 0)
            sc_json_object_set(&a, cost, "monthly_limit_usd",
                               sc_json_number_new(&a, cfg->cost.monthly_limit_usd));
        sc_json_object_set(&a, root, "cost", cost);
    }

    /* agent */
    sc_json_value_t *agent_obj = sc_json_object_new(&a);
    if (agent_obj) {
        if (cfg->agent.max_tool_iterations > 0)
            sc_json_object_set(&a, agent_obj, "max_tool_iterations",
                               sc_json_number_new(&a, (double)cfg->agent.max_tool_iterations));
        if (cfg->agent.max_history_messages > 0)
            sc_json_object_set(&a, agent_obj, "max_history_messages",
                               sc_json_number_new(&a, (double)cfg->agent.max_history_messages));
        sc_json_object_set(&a, root, "agent", agent_obj);
    }

    /* secrets */
    sc_json_value_t *secrets_obj = sc_json_object_new(&a);
    if (secrets_obj) {
        sc_json_object_set(&a, secrets_obj, "encrypt", sc_json_bool_new(&a, cfg->secrets.encrypt));
        sc_json_object_set(&a, root, "secrets", secrets_obj);
    }

    /* identity */
    if (cfg->identity.format) {
        sc_json_value_t *id_obj = sc_json_object_new(&a);
        if (id_obj) {
            sc_json_object_set(
                &a, id_obj, "format",
                sc_json_string_new(&a, cfg->identity.format, strlen(cfg->identity.format)));
            sc_json_object_set(&a, root, "identity", id_obj);
        }
    }

    /* diagnostics */
    sc_json_value_t *diag = sc_json_object_new(&a);
    if (diag) {
        if (cfg->diagnostics.backend)
            sc_json_object_set(
                &a, diag, "backend",
                sc_json_string_new(&a, cfg->diagnostics.backend, strlen(cfg->diagnostics.backend)));
        if (cfg->diagnostics.otel_endpoint)
            sc_json_object_set(&a, diag, "otel_endpoint",
                               sc_json_string_new(&a, cfg->diagnostics.otel_endpoint,
                                                  strlen(cfg->diagnostics.otel_endpoint)));
        sc_json_object_set(&a, diag, "log_tool_calls",
                           sc_json_bool_new(&a, cfg->diagnostics.log_tool_calls));
        sc_json_object_set(&a, diag, "log_llm_io",
                           sc_json_bool_new(&a, cfg->diagnostics.log_llm_io));
        sc_json_object_set(&a, root, "diagnostics", diag);
    }

    /* autonomy */
    sc_json_value_t *auton = sc_json_object_new(&a);
    if (auton) {
        if (cfg->autonomy.level)
            sc_json_object_set(
                &a, auton, "level",
                sc_json_string_new(&a, cfg->autonomy.level, strlen(cfg->autonomy.level)));
        sc_json_object_set(&a, auton, "workspace_only",
                           sc_json_bool_new(&a, cfg->autonomy.workspace_only));
        if (cfg->autonomy.max_actions_per_hour > 0)
            sc_json_object_set(&a, auton, "max_actions_per_hour",
                               sc_json_number_new(&a, (double)cfg->autonomy.max_actions_per_hour));
        sc_json_object_set(&a, root, "autonomy", auton);
    }

    /* reliability */
    sc_json_value_t *rel = sc_json_object_new(&a);
    if (rel) {
        if (cfg->reliability.provider_retries > 0)
            sc_json_object_set(&a, rel, "provider_retries",
                               sc_json_number_new(&a, (double)cfg->reliability.provider_retries));
        if (cfg->reliability.provider_backoff_ms > 0)
            sc_json_object_set(
                &a, rel, "provider_backoff_ms",
                sc_json_number_new(&a, (double)cfg->reliability.provider_backoff_ms));
        sc_json_object_set(&a, root, "reliability", rel);
    }

    /* channels */
    if (cfg->channels.default_channel || cfg->channels.cli) {
        sc_json_value_t *ch = sc_json_object_new(&a);
        if (ch) {
            sc_json_object_set(&a, ch, "cli", sc_json_bool_new(&a, cfg->channels.cli));
            if (cfg->channels.default_channel)
                sc_json_object_set(&a, ch, "default",
                                   sc_json_string_new(&a, cfg->channels.default_channel,
                                                      strlen(cfg->channels.default_channel)));
            sc_json_object_set(&a, root, "channels", ch);
        }
    }

    /* tunnel */
    if (cfg->tunnel.provider) {
        sc_json_value_t *tun = sc_json_object_new(&a);
        if (tun) {
            sc_json_object_set(
                &a, tun, "provider",
                sc_json_string_new(&a, cfg->tunnel.provider, strlen(cfg->tunnel.provider)));
            if (cfg->tunnel.domain)
                sc_json_object_set(
                    &a, tun, "domain",
                    sc_json_string_new(&a, cfg->tunnel.domain, strlen(cfg->tunnel.domain)));
            sc_json_object_set(&a, root, "tunnel", tun);
        }
    }

    /* cron */
    sc_json_value_t *cron_obj = sc_json_object_new(&a);
    if (cron_obj) {
        sc_json_object_set(&a, cron_obj, "enabled", sc_json_bool_new(&a, cfg->cron.enabled));
        if (cfg->cron.interval_minutes > 0)
            sc_json_object_set(&a, cron_obj, "interval_minutes",
                               sc_json_number_new(&a, (double)cfg->cron.interval_minutes));
        sc_json_object_set(&a, root, "cron", cron_obj);
    }

    /* scheduler */
    if (cfg->scheduler.max_concurrent > 0) {
        sc_json_value_t *sched = sc_json_object_new(&a);
        if (sched) {
            sc_json_object_set(&a, sched, "max_concurrent",
                               sc_json_number_new(&a, (double)cfg->scheduler.max_concurrent));
            sc_json_object_set(&a, root, "scheduler", sched);
        }
    }

    /* session */
    if (cfg->session.idle_minutes > 0) {
        sc_json_value_t *sess = sc_json_object_new(&a);
        if (sess) {
            sc_json_object_set(&a, sess, "idle_minutes",
                               sc_json_number_new(&a, (double)cfg->session.idle_minutes));
            sc_json_object_set(&a, root, "session", sess);
        }
    }

    /* peripherals */
    sc_json_value_t *periph = sc_json_object_new(&a);
    if (periph) {
        sc_json_object_set(&a, periph, "enabled", sc_json_bool_new(&a, cfg->peripherals.enabled));
        if (cfg->peripherals.datasheet_dir)
            sc_json_object_set(&a, periph, "datasheet_dir",
                               sc_json_string_new(&a, cfg->peripherals.datasheet_dir,
                                                  strlen(cfg->peripherals.datasheet_dir)));
        sc_json_object_set(&a, root, "peripherals", periph);
    }

    /* hardware */
    if (cfg->hardware.enabled) {
        sc_json_value_t *hw = sc_json_object_new(&a);
        if (hw) {
            sc_json_object_set(&a, hw, "enabled", sc_json_bool_new(&a, cfg->hardware.enabled));
            if (cfg->hardware.transport)
                sc_json_object_set(&a, hw, "transport",
                                   sc_json_string_new(&a, cfg->hardware.transport,
                                                      strlen(cfg->hardware.transport)));
            if (cfg->hardware.serial_port)
                sc_json_object_set(&a, hw, "serial_port",
                                   sc_json_string_new(&a, cfg->hardware.serial_port,
                                                      strlen(cfg->hardware.serial_port)));
            if (cfg->hardware.baud_rate > 0)
                sc_json_object_set(&a, hw, "baud_rate",
                                   sc_json_number_new(&a, (double)cfg->hardware.baud_rate));
            sc_json_object_set(&a, root, "hardware", hw);
        }
    }

    /* browser */
    sc_json_object_set(&a, root, "browser", sc_json_bool_new(&a, cfg->browser.enabled));

    /* mcp_servers */
    if (cfg->mcp_servers_len > 0) {
        sc_json_value_t *mcp_arr = sc_json_array_new(&a);
        if (mcp_arr) {
            for (size_t i = 0; i < cfg->mcp_servers_len; i++) {
                sc_json_value_t *me = sc_json_object_new(&a);
                if (!me)
                    continue;
                if (cfg->mcp_servers[i].name)
                    sc_json_object_set(&a, me, "name",
                                       sc_json_string_new(&a, cfg->mcp_servers[i].name,
                                                          strlen(cfg->mcp_servers[i].name)));
                if (cfg->mcp_servers[i].command)
                    sc_json_object_set(&a, me, "command",
                                       sc_json_string_new(&a, cfg->mcp_servers[i].command,
                                                          strlen(cfg->mcp_servers[i].command)));
                if (cfg->mcp_servers[i].args_count > 0) {
                    sc_json_value_t *args_arr = sc_json_array_new(&a);
                    if (args_arr) {
                        for (size_t j = 0; j < cfg->mcp_servers[i].args_count; j++) {
                            if (cfg->mcp_servers[i].args[j])
                                sc_json_array_push(
                                    &a, args_arr,
                                    sc_json_string_new(&a, cfg->mcp_servers[i].args[j],
                                                       strlen(cfg->mcp_servers[i].args[j])));
                        }
                        sc_json_object_set(&a, me, "args", args_arr);
                    }
                }
                sc_json_array_push(&a, mcp_arr, me);
            }
            sc_json_object_set(&a, root, "mcp_servers", mcp_arr);
        }
    }

    /* providers array */
    if (cfg->providers_len > 0) {
        sc_json_value_t *parr = sc_json_array_new(&a);
        if (parr) {
            for (size_t i = 0; i < cfg->providers_len; i++) {
                sc_json_value_t *pe = sc_json_object_new(&a);
                if (!pe)
                    continue;
                if (cfg->providers[i].name)
                    sc_json_object_set(&a, pe, "name",
                                       sc_json_string_new(&a, cfg->providers[i].name,
                                                          strlen(cfg->providers[i].name)));
                if (cfg->providers[i].base_url)
                    sc_json_object_set(&a, pe, "base_url",
                                       sc_json_string_new(&a, cfg->providers[i].base_url,
                                                          strlen(cfg->providers[i].base_url)));
                if (cfg->providers[i].native_tools)
                    sc_json_object_set(&a, pe, "native_tools",
                                       sc_json_bool_new(&a, cfg->providers[i].native_tools));
                sc_json_array_push(&a, parr, pe);
            }
            sc_json_object_set(&a, root, "providers", parr);
        }
    }

    char *json_str = NULL;
    size_t json_len = 0;
    sc_error_t err = sc_json_stringify(&a, root, &json_str, &json_len);
    sc_json_free(&a, root);
    if (err != SC_OK)
        return err;
    if (!json_str)
        return SC_ERR_OUT_OF_MEMORY;

    FILE *f = fopen(cfg->config_path, "w");
    if (!f) {
        a.free(a.ctx, json_str, json_len + 1);
        return SC_ERR_IO;
    }
    fwrite(json_str, 1, json_len, f);
    fclose(f);
    a.free(a.ctx, json_str, json_len + 1);
    return SC_OK;
}
