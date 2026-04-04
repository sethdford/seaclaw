#include "human/onboard.h"
#include "human/core/string.h"
#include "human/daemon.h"
#include "human/interactions.h"
#include "human/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#define HU_CONFIG_DIR  ".human"
#define HU_CONFIG_FILE "config.json"
#define HU_MAX_PATH    1024
#define HU_ONBOARD_LINE 512

typedef enum hu_onboard_ch_kind {
    HU_ONBOARD_CH_NONE = 0,
    HU_ONBOARD_CH_TELEGRAM,
    HU_ONBOARD_CH_DISCORD,
    HU_ONBOARD_CH_SLACK,
    HU_ONBOARD_CH_WHATSAPP,
    HU_ONBOARD_CH_IMESSAGE,
    HU_ONBOARD_CH_INVALID,
} hu_onboard_ch_kind_t;

static char *get_config_path(char *buf, size_t buf_size) {
    const char *home = getenv("HOME");
    if (!home)
        home = ".";
    int n = snprintf(buf, buf_size, "%s/%s/%s", home, HU_CONFIG_DIR, HU_CONFIG_FILE);
    if (n <= 0 || (size_t)n >= buf_size)
        return NULL;
    return buf;
}

bool hu_onboard_check_first_run(void) {
    char path[HU_MAX_PATH];
    if (!get_config_path(path, sizeof(path)))
        return true;
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return false;
    }
    return true;
}

#ifdef HU_IS_TEST
hu_error_t hu_onboard_run_with_opts(hu_allocator_t *alloc, const hu_onboard_opts_t *opts) {
    (void)alloc;
    (void)opts;
    return HU_OK;
}

hu_error_t hu_onboard_run(hu_allocator_t *alloc) {
    return hu_onboard_run_with_opts(alloc, NULL);
}
#else

static const char *const HU_AGENTS_TEMPLATE = "# AGENTS.md — Project Agent Protocol\n"
                                              "## Build & Test\n"
                                              "- Build: `make` or `cmake .. && make`\n"
                                              "- Test: `make test`\n"
                                              "## Conventions\n"
                                              "- Follow existing code style\n"
                                              "- Write tests for new features\n"
                                              "- Keep commits focused\n";

static const char *const HU_USER_TEMPLATE = "# User Preferences\n"
                                            "## Communication\n"
                                            "- Be concise and direct\n"
                                            "- Show code examples when helpful\n"
                                            "## Expertise\n"
                                            "- Assume intermediate programming knowledge\n";

static const char *const HU_IDENTITY_TEMPLATE =
    "# Agent Identity\n"
    "name: Human\n"
    "description: Autonomous AI assistant running locally\n"
    "personality: Helpful, concise, security-conscious\n";

static const char *const HU_SOUL_TEMPLATE = "# SOUL.md — Presence & Boundaries\n"
                                            "## Tone\n"
                                            "- Warm and steady; no performative enthusiasm\n"
                                            "## Boundaries\n"
                                            "- Decline harmful or deceptive requests clearly\n"
                                            "- Prefer verified facts over speculation\n";

static bool write_template_if_missing(const char *path, const char *content) {
    FILE *check = fopen(path, "rb");
    if (check) {
        fclose(check);
        return false;
    }
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    size_t len = strlen(content);
    if (fwrite(content, 1, len, f) != len) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static char *read_line(char *buf, size_t buf_size) {
    if (!fgets(buf, (int)buf_size, stdin))
        return NULL;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return buf;
}

static void fprint_json_str(FILE *f, const char *s) {
    fputc('"', f);
    if (!s) {
        fputc('"', f);
        return;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc((int)c, f);
        } else if (c < 0x20) {
            fprintf(f, "\\u%04x", (unsigned)c);
        } else {
            fputc((int)c, f);
        }
    }
    fputc('"', f);
}

static hu_onboard_ch_kind_t parse_channel_kind(const char *name) {
    if (!name || !name[0])
        return HU_ONBOARD_CH_NONE;
    if (hu_util_strcasecmp(name, "none") == 0 || hu_util_strcasecmp(name, "skip") == 0)
        return HU_ONBOARD_CH_NONE;
    if (hu_util_strcasecmp(name, "telegram") == 0)
        return HU_ONBOARD_CH_TELEGRAM;
    if (hu_util_strcasecmp(name, "discord") == 0)
        return HU_ONBOARD_CH_DISCORD;
    if (hu_util_strcasecmp(name, "slack") == 0)
        return HU_ONBOARD_CH_SLACK;
    if (hu_util_strcasecmp(name, "whatsapp") == 0)
        return HU_ONBOARD_CH_WHATSAPP;
    if (hu_util_strcasecmp(name, "imessage") == 0)
        return HU_ONBOARD_CH_IMESSAGE;
    return HU_ONBOARD_CH_INVALID;
}

static const char *env_get(const char *key) {
    const char *v = getenv(key);
    return (v && v[0]) ? v : NULL;
}

static hu_error_t collect_channel_noninteractive(hu_onboard_ch_kind_t kind, const char **out_tg,
                                                 const char **out_dc_tok, const char **out_dc_guild,
                                                 const char **out_sl_tok, const char **out_wa_phone,
                                                 const char **out_wa_tok, const char **out_wa_verify,
                                                 const char **out_im_target) {
    *out_tg = *out_dc_tok = *out_dc_guild = *out_sl_tok = NULL;
    *out_wa_phone = *out_wa_tok = *out_wa_verify = *out_im_target = NULL;
    switch (kind) {
    case HU_ONBOARD_CH_NONE:
        return HU_OK;
    case HU_ONBOARD_CH_TELEGRAM:
        *out_tg = env_get("TELEGRAM_BOT_TOKEN");
        if (!*out_tg) {
            fprintf(stderr, "non-interactive telegram: set TELEGRAM_BOT_TOKEN\n");
            return HU_ERR_INVALID_ARGUMENT;
        }
        return HU_OK;
    case HU_ONBOARD_CH_DISCORD:
        *out_dc_tok = env_get("DISCORD_BOT_TOKEN");
        if (!*out_dc_tok) {
            fprintf(stderr, "non-interactive discord: set DISCORD_BOT_TOKEN\n");
            return HU_ERR_INVALID_ARGUMENT;
        }
        *out_dc_guild = env_get("DISCORD_GUILD_ID");
        return HU_OK;
    case HU_ONBOARD_CH_SLACK:
        *out_sl_tok = env_get("SLACK_BOT_TOKEN");
        if (!*out_sl_tok) {
            fprintf(stderr, "non-interactive slack: set SLACK_BOT_TOKEN\n");
            return HU_ERR_INVALID_ARGUMENT;
        }
        return HU_OK;
    case HU_ONBOARD_CH_WHATSAPP:
        *out_wa_phone = env_get("WHATSAPP_PHONE_NUMBER_ID");
        *out_wa_tok = env_get("WHATSAPP_TOKEN");
        *out_wa_verify = env_get("WHATSAPP_VERIFY_TOKEN");
        if (!*out_wa_phone || !*out_wa_tok || !*out_wa_verify) {
            fprintf(stderr,
                    "non-interactive whatsapp: set WHATSAPP_PHONE_NUMBER_ID, WHATSAPP_TOKEN, "
                    "WHATSAPP_VERIFY_TOKEN\n");
            return HU_ERR_INVALID_ARGUMENT;
        }
        return HU_OK;
    case HU_ONBOARD_CH_IMESSAGE:
        *out_im_target = env_get("IMESSAGE_DEFAULT_TARGET");
        return HU_OK;
    default:
        return HU_ERR_INVALID_ARGUMENT;
    }
}

static void print_channel_json(FILE *f, hu_onboard_ch_kind_t kind, const char *tg_tok,
                               const char *dc_tok, const char *dc_guild, const char *sl_tok,
                               const char *wa_phone, const char *wa_tok, const char *wa_verify,
                               const char *im_target) {
    fprintf(f, "  \"channels\": {\n");
    switch (kind) {
    case HU_ONBOARD_CH_TELEGRAM:
        fprintf(f, "    \"telegram\": { \"token\": ");
        fprint_json_str(f, tg_tok);
        fprintf(f, " }\n");
        break;
    case HU_ONBOARD_CH_DISCORD:
        fprintf(f, "    \"discord\": { \"token\": ");
        fprint_json_str(f, dc_tok);
        if (dc_guild && dc_guild[0]) {
            fprintf(f, ", \"guild_id\": ");
            fprint_json_str(f, dc_guild);
        }
        fprintf(f, " }\n");
        break;
    case HU_ONBOARD_CH_SLACK:
        fprintf(f, "    \"slack\": { \"token\": ");
        fprint_json_str(f, sl_tok);
        fprintf(f, " }\n");
        break;
    case HU_ONBOARD_CH_WHATSAPP:
        fprintf(f, "    \"whatsapp\": { \"phone_number_id\": ");
        fprint_json_str(f, wa_phone);
        fprintf(f, ", \"token\": ");
        fprint_json_str(f, wa_tok);
        fprintf(f, ", \"verify_token\": ");
        fprint_json_str(f, wa_verify);
        fprintf(f, " }\n");
        break;
    case HU_ONBOARD_CH_IMESSAGE:
        fprintf(f, "    \"imessage\": { ");
        if (im_target && im_target[0]) {
            fprintf(f, "\"default_target\": ");
            fprint_json_str(f, im_target);
        }
        fprintf(f, " }\n");
        break;
    case HU_ONBOARD_CH_NONE:
    case HU_ONBOARD_CH_INVALID:
        break;
    }
    fprintf(f, "  },\n");
}

static void prompt_channel_fields(hu_onboard_ch_kind_t kind, char *buf, char *buf2, char *buf3,
                                  const char **out_tg, const char **out_dc_tok,
                                  const char **out_dc_guild, const char **out_sl_tok,
                                  const char **out_wa_phone, const char **out_wa_tok,
                                  const char **out_wa_verify, const char **out_im_target) {
    char *line;
    *out_tg = *out_dc_tok = *out_dc_guild = *out_sl_tok = NULL;
    *out_wa_phone = *out_wa_tok = *out_wa_verify = *out_im_target = NULL;
    switch (kind) {
    case HU_ONBOARD_CH_NONE:
        return;
    case HU_ONBOARD_CH_TELEGRAM:
        printf("Telegram bot token: ");
        fflush(stdout);
        line = read_line(buf, HU_ONBOARD_LINE);
        *out_tg = (line && line[0]) ? line : "";
        return;
    case HU_ONBOARD_CH_DISCORD:
        printf("Discord bot token: ");
        fflush(stdout);
        line = read_line(buf, HU_ONBOARD_LINE);
        *out_dc_tok = (line && line[0]) ? line : "";
        printf("Discord guild ID (optional): ");
        fflush(stdout);
        line = read_line(buf2, HU_ONBOARD_LINE);
        *out_dc_guild = (line && line[0]) ? line : NULL;
        return;
    case HU_ONBOARD_CH_SLACK:
        printf("Slack bot token: ");
        fflush(stdout);
        line = read_line(buf, HU_ONBOARD_LINE);
        *out_sl_tok = (line && line[0]) ? line : "";
        return;
    case HU_ONBOARD_CH_WHATSAPP:
        printf("WhatsApp phone number ID: ");
        fflush(stdout);
        line = read_line(buf, HU_ONBOARD_LINE);
        *out_wa_phone = (line && line[0]) ? line : "";
        printf("WhatsApp access token: ");
        fflush(stdout);
        line = read_line(buf2, HU_ONBOARD_LINE);
        *out_wa_tok = (line && line[0]) ? line : "";
        printf("WhatsApp verify token: ");
        fflush(stdout);
        line = read_line(buf3, HU_ONBOARD_LINE);
        *out_wa_verify = (line && line[0]) ? line : "";
        return;
    case HU_ONBOARD_CH_IMESSAGE:
        printf("iMessage default target (optional, email or phone): ");
        fflush(stdout);
        line = read_line(buf, HU_ONBOARD_LINE);
        *out_im_target = (line && line[0]) ? line : NULL;
        return;
    default:
        return;
    }
}

static bool channel_has_required_credentials(hu_onboard_ch_kind_t kind, const char *tg_tok,
                                             const char *dc_tok, const char *sl_tok,
                                             const char *wa_phone, const char *wa_tok,
                                             const char *wa_verify) {
    switch (kind) {
    case HU_ONBOARD_CH_NONE:
    case HU_ONBOARD_CH_IMESSAGE:
        return true;
    case HU_ONBOARD_CH_TELEGRAM:
        return tg_tok && tg_tok[0];
    case HU_ONBOARD_CH_DISCORD:
        return dc_tok && dc_tok[0];
    case HU_ONBOARD_CH_SLACK:
        return sl_tok && sl_tok[0];
    case HU_ONBOARD_CH_WHATSAPP:
        return wa_phone && wa_phone[0] && wa_tok && wa_tok[0] && wa_verify && wa_verify[0];
    default:
        return false;
    }
}

hu_error_t hu_onboard_run_with_opts(hu_allocator_t *alloc, const hu_onboard_opts_t *opts) {
    if (!alloc)
        return HU_ERR_INVALID_ARGUMENT;

    const bool ni = opts && opts->non_interactive;

    if (!hu_onboard_check_first_run()) {
        printf("Config already exists. Run 'human doctor' to check status.\n");
        return HU_OK;
    }

    if (!ni) {
        printf("Human Setup Wizard\n");
        printf("===================\n\n");
    }

    char buf_api[HU_ONBOARD_LINE];
    char buf_model[HU_ONBOARD_LINE];
    char buf[HU_ONBOARD_LINE];
    char buf2[HU_ONBOARD_LINE];
    char buf3[HU_ONBOARD_LINE];
    char *line;

    const char *provider = NULL;
    if (opts && opts->provider && opts->provider[0])
        provider = opts->provider;
    else if (!ni) {
        static const hu_choice_t provider_choices[] = {
            {"OpenAI (GPT-4, etc.)", "openai", true},
            {"Anthropic (Claude)", "anthropic", false},
            {"Ollama (local)", "ollama", false},
            {"OpenRouter", "openrouter", false},
        };
        hu_choice_result_t provider_result;
        hu_error_t perr =
            hu_choices_prompt("Choose your default provider:", provider_choices,
                              sizeof(provider_choices) / sizeof(provider_choices[0]), &provider_result);
        provider = (perr == HU_OK && provider_result.selected_value) ? provider_result.selected_value
                                                                     : "openai";
    } else {
        provider = "openai";
    }

    const char *api_key = NULL;
    if (opts && opts->api_key)
        api_key = opts->api_key;
    else if (!ni) {
        printf("API key (or set %s env var): ", "OPENAI_API_KEY");
        fflush(stdout);
        line = read_line(buf_api, sizeof(buf_api));
        api_key = line && line[0] ? buf_api : "";
    } else {
        api_key = "";
    }

    const char *model = NULL;
    if (opts && opts->model && opts->model[0])
        model = opts->model;
    else if (!ni) {
        printf("Model (e.g. gpt-4o): ");
        fflush(stdout);
        line = read_line(buf_model, sizeof(buf_model));
        model = line && line[0] ? buf_model : "gpt-4o";
    } else {
        model = "gpt-4o";
    }

    hu_onboard_ch_kind_t ch_kind = HU_ONBOARD_CH_NONE;
    if (opts && opts->channel && opts->channel[0]) {
        hu_onboard_ch_kind_t p = parse_channel_kind(opts->channel);
        if (p == HU_ONBOARD_CH_INVALID) {
            fprintf(stderr, "Unknown --channel (use telegram, discord, slack, whatsapp, imessage)\n");
            return HU_ERR_INVALID_ARGUMENT;
        }
        ch_kind = p;
    } else if (!ni) {
        static const hu_choice_t ch_choices[] = {
            {"Telegram", "telegram", false},       {"Discord", "discord", false},
            {"Slack", "slack", false},             {"WhatsApp", "whatsapp", false},
            {"iMessage", "imessage", false},       {"Skip for now", "none", true},
        };
        hu_choice_result_t ch_res;
        hu_error_t cerr =
            hu_choices_prompt("Connect a messaging channel (optional):", ch_choices,
                              sizeof(ch_choices) / sizeof(ch_choices[0]), &ch_res);
        const char *cv = (cerr == HU_OK && ch_res.selected_value) ? ch_res.selected_value : "none";
        ch_kind = parse_channel_kind(cv);
    }

    const char *tg_tok = NULL, *dc_tok = NULL, *dc_guild = NULL, *sl_tok = NULL;
    const char *wa_phone = NULL, *wa_tok = NULL, *wa_verify = NULL, *im_target = NULL;

    if (ch_kind != HU_ONBOARD_CH_NONE) {
        if (ni) {
            hu_error_t cerr =
                collect_channel_noninteractive(ch_kind, &tg_tok, &dc_tok, &dc_guild, &sl_tok,
                                               &wa_phone, &wa_tok, &wa_verify, &im_target);
            if (cerr != HU_OK)
                return cerr;
        } else {
            prompt_channel_fields(ch_kind, buf, buf2, buf3, &tg_tok, &dc_tok, &dc_guild, &sl_tok,
                                  &wa_phone, &wa_tok, &wa_verify, &im_target);
        }
        if (!channel_has_required_credentials(ch_kind, tg_tok, dc_tok, sl_tok, wa_phone, wa_tok,
                                              wa_verify)) {
            if (!ni)
                printf("Channel credentials missing; skipping channel block.\n");
            ch_kind = HU_ONBOARD_CH_NONE;
        }
    }

    const char *home = getenv("HOME");
    if (!home)
        home = ".";
    char config_dir[HU_MAX_PATH];
    int n = snprintf(config_dir, sizeof(config_dir), "%s/%s", home, HU_CONFIG_DIR);
    if (n <= 0 || (size_t)n >= sizeof(config_dir))
        return HU_ERR_IO;

#ifdef _WIN32
    (void)_mkdir(config_dir);
#else
    (void)mkdir(config_dir, 0700);
#endif

    char config_path[HU_MAX_PATH];
    n = snprintf(config_path, sizeof(config_path), "%s/%s", config_dir, HU_CONFIG_FILE);
    if (n <= 0 || (size_t)n >= sizeof(config_path))
        return HU_ERR_IO;

    char *ws_dir = NULL;
    if (opts && opts->workspace && opts->workspace[0]) {
        ws_dir = hu_sprintf(alloc, "%s", opts->workspace);
        if (!ws_dir)
            return HU_ERR_OUT_OF_MEMORY;
    } else {
        ws_dir = hu_sprintf(alloc, "%s/%s/workspace", home, HU_CONFIG_DIR);
        if (!ws_dir)
            return HU_ERR_OUT_OF_MEMORY;
    }

#ifdef _WIN32
    (void)_mkdir(ws_dir);
#else
    (void)mkdir(ws_dir, 0700);
#endif

    {
        char tmpl_path[HU_MAX_PATH];
        int nr = snprintf(tmpl_path, sizeof(tmpl_path), "%s/AGENTS.md", ws_dir);
        if (nr > 0 && (size_t)nr < sizeof(tmpl_path) &&
            write_template_if_missing(tmpl_path, HU_AGENTS_TEMPLATE))
            printf("  Created %s\n", tmpl_path);
        nr = snprintf(tmpl_path, sizeof(tmpl_path), "%s/USER.md", ws_dir);
        if (nr > 0 && (size_t)nr < sizeof(tmpl_path) &&
            write_template_if_missing(tmpl_path, HU_USER_TEMPLATE))
            printf("  Created %s\n", tmpl_path);
        nr = snprintf(tmpl_path, sizeof(tmpl_path), "%s/IDENTITY.md", ws_dir);
        if (nr > 0 && (size_t)nr < sizeof(tmpl_path) &&
            write_template_if_missing(tmpl_path, HU_IDENTITY_TEMPLATE))
            printf("  Created %s\n", tmpl_path);
        nr = snprintf(tmpl_path, sizeof(tmpl_path), "%s/SOUL.md", ws_dir);
        if (nr > 0 && (size_t)nr < sizeof(tmpl_path) &&
            write_template_if_missing(tmpl_path, HU_SOUL_TEMPLATE))
            printf("  Created %s\n", tmpl_path);
    }

    FILE *f = fopen(config_path, "w");
    if (!f) {
        alloc->free(alloc->ctx, ws_dir, strlen(ws_dir) + 1);
        return HU_ERR_IO;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"workspace\": ");
    fprint_json_str(f, ws_dir);
    fprintf(f, ",\n");
    fprintf(f, "  \"default_provider\": ");
    fprint_json_str(f, provider);
    fprintf(f, ",\n");
    fprintf(f, "  \"default_model\": ");
    fprint_json_str(f, model);
    fprintf(f, ",\n");
    if (api_key && api_key[0]) {
        fprintf(f, "  \"providers\": [{\"name\": ");
        fprint_json_str(f, provider);
        fprintf(f, ", \"api_key\": ");
        fprint_json_str(f, api_key);
        fprintf(f, "}],\n");
    } else {
        fprintf(f, "  \"providers\": [],\n");
    }
    if (ch_kind != HU_ONBOARD_CH_NONE)
        print_channel_json(f, ch_kind, tg_tok, dc_tok, dc_guild, sl_tok, wa_phone, wa_tok,
                           wa_verify, im_target);
    fprintf(f, "  \"memory\": {\"backend\": \"sqlite\", \"auto_save\": true},\n");
    fprintf(f, "  \"gateway\": {\"port\": 3000, \"host\": \"127.0.0.1\"}\n");
    fprintf(f, "}\n");
    fclose(f);
    alloc->free(alloc->ctx, ws_dir, strlen(ws_dir) + 1);

    printf("\nSetup complete!\n");
    printf("Config written to %s\n", config_path);
    printf("Next steps:\n");
    printf("  Run `human doctor` to verify the install.\n");
    printf("  Start the gateway with `human service start` or use the daemon.\n");
    if (ch_kind == HU_ONBOARD_CH_NONE)
        printf("  Add channels under \"channels\" in config when you are ready.\n");

    if (opts && opts->start_gateway) {
        hu_error_t derr = hu_daemon_start();
        if (derr == HU_OK)
            printf("Gateway service start requested.\n");
        else if (derr == HU_ERR_NOT_SUPPORTED)
            fprintf(stderr, "Gateway auto-start is not supported on this platform.\n");
        else
            fprintf(stderr, "Could not start gateway service: %s\n", hu_error_string(derr));
    }

    return HU_OK;
}

hu_error_t hu_onboard_run(hu_allocator_t *alloc) {
    return hu_onboard_run_with_opts(alloc, NULL);
}
#endif
