#include "human/cli_commands.h"
#include "human/config.h"
#include "human/core/error.h"
#include "human/eval.h"
#include "human/eval_dashboard.h"
#include "human/memory.h"
#include "human/memory/factory.h"
#include "human/providers/factory.h"
#include "human/security/sandbox.h"
#ifdef HU_ENABLE_FEEDS
#include "human/feeds/processor.h"
#include "human/feeds/findings.h"
#include "human/feeds/trends.h"
#include "human/intelligence/cycle.h"
#endif
#ifdef HU_HAS_UPDATE
#include "human/update.h"
#endif
#include "human/version.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#endif

#define HU_INIT_CONFIG_DIR  ".human"
#define HU_INIT_CONFIG_FILE "config.json"
#define HU_INIT_MAX_PATH    1024

#ifndef HU_IS_TEST
static const char HU_INIT_DEFAULT_JSON[] =
    "{\n"
    "  \"default_provider\": \"gemini\",\n"
    "  \"default_model\": \"gemini-3.1-flash-lite-preview\",\n"
    "  \"max_tokens\": 4096,\n"
    "  \"memory\": {\n"
    "    \"backend\": \"sqlite\"\n"
    "  },\n"
    "  \"gateway\": {\n"
    "    \"enabled\": false,\n"
    "    \"port\": 3000\n"
    "  }\n"
    "}\n";
#endif

/* ── init ────────────────────────────────────────────────────────────────── */
hu_error_t cmd_init(hu_allocator_t *alloc, int argc, char **argv) {
    (void)alloc;
    (void)argc;
    (void)argv;

#ifdef HU_IS_TEST
    /* In test mode: skip filesystem and stdin, succeed immediately. */
    return HU_OK;
#else
    const char *home = getenv("HOME");
    if (!home)
        home = ".";

    char config_path[HU_INIT_MAX_PATH];
    int n = snprintf(config_path, sizeof(config_path), "%s/%s/%s", home, HU_INIT_CONFIG_DIR,
                     HU_INIT_CONFIG_FILE);
    if (n <= 0 || (size_t)n >= sizeof(config_path))
        return HU_ERR_INVALID_ARGUMENT;

    if (access(config_path, F_OK) == 0) {
        printf("Config already exists. Overwrite? [y/N] ");
        fflush(stdout);
        int c = getchar();
        if (c != 'y' && c != 'Y') {
            printf("Aborted.\n");
            return HU_ERR_CANCELLED;
        }
        while (c != '\n' && c != EOF)
            c = getchar();
    }

    char dir_path[HU_INIT_MAX_PATH];
    n = snprintf(dir_path, sizeof(dir_path), "%s/%s", home, HU_INIT_CONFIG_DIR);
    if (n <= 0 || (size_t)n >= sizeof(dir_path))
        return HU_ERR_INVALID_ARGUMENT;

    if (mkdir(dir_path, 0700) != 0 && errno != EEXIST)
        return HU_ERR_IO;

    FILE *f = fopen(config_path, "w");
    if (!f)
        return HU_ERR_IO;

    size_t len = sizeof(HU_INIT_DEFAULT_JSON) - 1;
    if (fwrite(HU_INIT_DEFAULT_JSON, 1, len, f) != len) {
        fclose(f);
        return HU_ERR_IO;
    }
    fclose(f);

    printf("Created ~/.human/config.json\n");
    printf("Set your API key: export OPENAI_API_KEY=sk-...\n");
    printf("Start chatting: human agent\n");
    return HU_OK;
#endif /* !HU_IS_TEST */
}

/* ── channel ─────────────────────────────────────────────────────────────── */
hu_error_t cmd_channel(hu_allocator_t *alloc, int argc, char **argv) {
    if (argc < 3 || strcmp(argv[2], "list") == 0) {
        hu_config_t cfg;
        hu_error_t err = hu_config_load(alloc, &cfg);
        printf("Configured channels:\n");
        printf("  cli: active\n");
        if (err == HU_OK && cfg.channels.default_channel &&
            strcmp(cfg.channels.default_channel, "cli") != 0)
            printf("  %s: configured\n", cfg.channels.default_channel);
        if (err == HU_OK)
            hu_config_deinit(&cfg);
        return HU_OK;
    }
    if (strcmp(argv[2], "status") == 0) {
        printf("Channel health:\n");
        printf("  cli: ok\n");
        return HU_OK;
    }
    fprintf(stderr, "Unknown channel subcommand: %s\n", argv[2]);
    return HU_ERR_INVALID_ARGUMENT;
}

/* ── hardware ────────────────────────────────────────────────────────────── */
hu_error_t cmd_hardware(hu_allocator_t *alloc, int argc, char **argv) {
    hu_config_t cfg;
    hu_error_t err = hu_config_load(alloc, &cfg);

    if (argc < 3 || strcmp(argv[2], "list") == 0) {
        if (err == HU_OK) {
            printf("Peripherals: %s\n", cfg.peripherals.enabled ? "enabled" : "disabled");
            if (cfg.hardware.enabled) {
                printf("Hardware transport: %s\n",
                       cfg.hardware.transport ? cfg.hardware.transport : "auto");
                if (cfg.hardware.serial_port)
                    printf("Serial port: %s @ %u baud\n", cfg.hardware.serial_port,
                           cfg.hardware.baud_rate);
                if (cfg.hardware.probe_target)
                    printf("Probe target: %s\n", cfg.hardware.probe_target);
            } else {
                printf("Detected hardware: none\n");
            }
        } else {
            printf("Detected hardware: none\n");
        }
        printf("Supported boards: arduino-uno, nucleo-f401re, stm32f411, esp32, rpi-pico\n");
        if (err == HU_OK)
            hu_config_deinit(&cfg);
        return HU_OK;
    }
    if (strcmp(argv[2], "info") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: human hardware info <board>\n");
            if (err == HU_OK)
                hu_config_deinit(&cfg);
            return HU_ERR_INVALID_ARGUMENT;
        }
        printf("Board: %s\nStatus: not connected\n", argv[3]);
        if (err == HU_OK)
            hu_config_deinit(&cfg);
        return HU_OK;
    }
    if (err == HU_OK)
        hu_config_deinit(&cfg);
    fprintf(stderr, "Unknown hardware subcommand: %s\n", argv[2]);
    return HU_ERR_INVALID_ARGUMENT;
}

/* ── memory ─────────────────────────────────────────────────────────────── */

hu_error_t cmd_memory(hu_allocator_t *alloc, int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: human memory <stats|count|list|search|get|forget>\n");
        return HU_OK;
    }
    hu_config_t cfg;
    hu_error_t err = hu_config_load(alloc, &cfg);
    if (err != HU_OK) {
        fprintf(stderr, "Config error: %s\n", hu_error_string(err));
        return err;
    }
    const char *ws = cfg.workspace_dir ? cfg.workspace_dir : ".";
    hu_memory_t mem = hu_memory_create_from_config(alloc, &cfg, ws);
    if (!mem.vtable) {
        printf("Memory backend: none (not configured)\n");
        hu_config_deinit(&cfg);
        return HU_OK;
    }
    const char *name = mem.vtable->name ? mem.vtable->name(mem.ctx) : "unknown";
    const char *sub = argv[2];

    if (strcmp(sub, "stats") == 0 || strcmp(sub, "status") == 0) {
        size_t count = 0;
        if (mem.vtable->count)
            mem.vtable->count(mem.ctx, &count);
        bool healthy = mem.vtable->health_check ? mem.vtable->health_check(mem.ctx) : true;
        printf("Memory backend: %s\nEntry count: %zu\nHealth: %s\n", name, count,
               healthy ? "ok" : "error");
    } else if (strcmp(sub, "count") == 0) {
        size_t count = 0;
        if (mem.vtable->count)
            mem.vtable->count(mem.ctx, &count);
        printf("%zu\n", count);
    } else if (strcmp(sub, "list") == 0) {
        hu_memory_entry_t *entries = NULL;
        size_t count = 0;
        err = mem.vtable->list(mem.ctx, alloc, NULL, NULL, 0, &entries, &count);
        if (err != HU_OK || count == 0) {
            printf("No memory entries.\n");
        } else {
            for (size_t i = 0; i < count; i++) {
                printf("  [%zu] %.*s: %.*s\n", i + 1, (int)entries[i].key_len,
                       entries[i].key ? entries[i].key : "",
                       (int)(entries[i].content_len > 80 ? 80 : entries[i].content_len),
                       entries[i].content ? entries[i].content : "");
                hu_memory_entry_free_fields(alloc, &entries[i]);
            }
            alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
        }
    } else if (strcmp(sub, "search") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: human memory search <query>\n");
            err = HU_ERR_INVALID_ARGUMENT;
            goto done;
        }
        hu_memory_entry_t *entries = NULL;
        size_t count = 0;
        err = mem.vtable->recall(mem.ctx, alloc, argv[3], strlen(argv[3]), 10, NULL, 0, &entries,
                                 &count);
        if (err != HU_OK || count == 0) {
            printf("No results for: %s\n", argv[3]);
        } else {
            for (size_t i = 0; i < count; i++) {
                printf("  [%zu] %.*s: %.*s\n", i + 1, (int)entries[i].key_len,
                       entries[i].key ? entries[i].key : "",
                       (int)(entries[i].content_len > 80 ? 80 : entries[i].content_len),
                       entries[i].content ? entries[i].content : "");
                hu_memory_entry_free_fields(alloc, &entries[i]);
            }
            alloc->free(alloc->ctx, entries, count * sizeof(hu_memory_entry_t));
        }
    } else if (strcmp(sub, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: human memory get <key>\n");
            err = HU_ERR_INVALID_ARGUMENT;
            goto done;
        }
        hu_memory_entry_t entry;
        bool found = false;
        err = mem.vtable->get(mem.ctx, alloc, argv[3], strlen(argv[3]), &entry, &found);
        if (err == HU_OK && found) {
            printf("%.*s\n", (int)entry.content_len, entry.content ? entry.content : "");
            hu_memory_entry_free_fields(alloc, &entry);
        } else {
            printf("Not found: %s\n", argv[3]);
        }
    } else if (strcmp(sub, "forget") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: human memory forget <key>\n");
            err = HU_ERR_INVALID_ARGUMENT;
            goto done;
        }
        bool deleted = false;
        err = mem.vtable->forget(mem.ctx, argv[3], strlen(argv[3]), &deleted);
        printf("%s: %s\n", argv[3], deleted ? "forgotten" : "not found");
    } else {
        fprintf(stderr, "Unknown memory subcommand: %s\n", sub);
        err = HU_ERR_INVALID_ARGUMENT;
    }
done:
    if (mem.vtable && mem.vtable->deinit)
        mem.vtable->deinit(mem.ctx);
    hu_config_deinit(&cfg);
    return err;
}

/* ── workspace ───────────────────────────────────────────────────────────── */
hu_error_t cmd_workspace(hu_allocator_t *alloc, int argc, char **argv) {
    hu_config_t cfg;
    hu_error_t err = hu_config_load(alloc, &cfg);
    const char *ws = (err == HU_OK && cfg.workspace_dir) ? cfg.workspace_dir : ".";

    if (argc < 3 || strcmp(argv[2], "show") == 0) {
        printf("Current workspace: %s\n", ws);
        if (err == HU_OK)
            hu_config_deinit(&cfg);
        return HU_OK;
    }
    if (strcmp(argv[2], "set") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: human workspace set <path>\n");
            if (err == HU_OK)
                hu_config_deinit(&cfg);
            return HU_ERR_INVALID_ARGUMENT;
        }
        if (err == HU_OK) {
            char json_buf[1024];
            size_t jp = 0;
            jp += (size_t)snprintf(json_buf + jp, sizeof(json_buf) - jp, "{\"workspace\":\"");
            const char *s = argv[3];
            for (; *s && jp + 4 < sizeof(json_buf); s++) {
                if (*s == '"' || *s == '\\')
                    json_buf[jp++] = '\\';
                json_buf[jp++] = *s;
            }
            jp += (size_t)snprintf(json_buf + jp, sizeof(json_buf) - jp, "\"}");
            hu_error_t pe = hu_config_parse_json(&cfg, json_buf, jp);
            if (pe == HU_OK) {
                hu_error_t se = hu_config_save(&cfg);
                if (se == HU_OK)
                    printf("Workspace set to: %s\n", argv[3]);
                else
                    fprintf(stderr, "Failed to save config: %s\n", hu_error_string(se));
            }
            hu_config_deinit(&cfg);
        }
        return HU_OK;
    }
    if (err == HU_OK)
        hu_config_deinit(&cfg);
    fprintf(stderr, "Unknown workspace subcommand: %s\n", argv[2]);
    return HU_ERR_INVALID_ARGUMENT;
}

/* ── capabilities ────────────────────────────────────────────────────────── */
hu_error_t cmd_capabilities(hu_allocator_t *alloc, int argc, char **argv) {
    hu_config_t cfg;
    hu_error_t err = hu_config_load(alloc, &cfg);
    bool json_mode = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0 || strcmp(argv[i], "json") == 0)
            json_mode = true;
    }
    const char *prov = (err == HU_OK && cfg.default_provider) ? cfg.default_provider : "gemini";
    const char *backend = (err == HU_OK && cfg.memory.backend) ? cfg.memory.backend : "none";
    if (json_mode) {
        printf("{\"channels\":[\"cli\"],\"tools\":[\"shell\",\"file_read\",\"file_write\",\"file_"
               "edit\","
               "\"git\",\"web_search\",\"web_fetch\",\"memory_store\",\"memory_recall\"],"
               "\"providers\":[\"%s\"],\"memory\":\"%s\"}\n",
               prov, backend);
    } else {
        printf("Capabilities:\n");
        printf("  Channels: cli\n");
        printf("  Tools: shell, file_read, file_write, file_edit, file_append, git,\n");
        printf("         web_search, web_fetch, http_request, memory_store, memory_recall,\n");
        printf("         memory_list, memory_forget, browser, image, screenshot,\n");
        printf("         message, delegate, spawn, cron, composio, pushover\n");
        printf("  Providers: %s\n", prov);
        printf("  Memory: %s\n", backend);
    }
    if (err == HU_OK)
        hu_config_deinit(&cfg);
    return HU_OK;
}

/* ── models ─────────────────────────────────────────────────────────────── */
hu_error_t cmd_models(hu_allocator_t *alloc, int argc, char **argv) {
    hu_config_t cfg;
    hu_error_t err = hu_config_load(alloc, &cfg);

    if (argc < 3 || strcmp(argv[2], "list") == 0) {
        if (err == HU_OK) {
            const char *prov = cfg.default_provider ? cfg.default_provider : "gemini";
            const char *model = cfg.default_model ? cfg.default_model : "(provider default)";
            printf("Active: provider=%s model=%s\n\n", prov, model);
            if (cfg.providers_len > 0) {
                printf("Configured providers:\n");
                for (size_t i = 0; i < cfg.providers_len; i++) {
                    printf("  %-16s\n", cfg.providers[i].name ? cfg.providers[i].name : "?");
                }
                printf("\n");
            }
        }
        printf("Known providers and default models:\n");
        printf("  %-16s %s\n", "Provider", "Default Model");
        printf("  %-16s %s\n", "--------", "-------------");
        printf("  %-16s %s\n", "openai", "gpt-4o");
        printf("  %-16s %s\n", "anthropic", "claude-sonnet-4-20250514");
        printf("  %-16s %s\n", "google", "gemini-2.0-flash");
        printf("  %-16s %s\n", "groq", "llama-3.3-70b-versatile");
        printf("  %-16s %s\n", "deepseek", "deepseek-chat");
        printf("  %-16s %s\n", "ollama", "(local)");
        printf("  %-16s %s\n", "openrouter", "(varies)");
        if (err == HU_OK)
            hu_config_deinit(&cfg);
        return HU_OK;
    }
    if (strcmp(argv[2], "info") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: human models info <model>\n");
            if (err == HU_OK)
                hu_config_deinit(&cfg);
            return HU_ERR_INVALID_ARGUMENT;
        }
        printf("Model: %s\n", argv[3]);
        if (err == HU_OK) {
            const char *prov = cfg.default_provider ? cfg.default_provider : "unknown";
            printf("Provider: %s\n", prov);
            hu_config_deinit(&cfg);
        } else {
            printf("Provider: unknown\n");
        }
        return HU_OK;
    }
    if (err == HU_OK)
        hu_config_deinit(&cfg);
    fprintf(stderr, "Unknown models subcommand: %s\n", argv[2]);
    return HU_ERR_INVALID_ARGUMENT;
}

/* ── auth ───────────────────────────────────────────────────────────────── */
hu_error_t cmd_auth(hu_allocator_t *alloc, int argc, char **argv) {
    (void)alloc;
    if (argc < 3) {
        printf("Usage: human auth <login|status|logout> <provider>\n");
        return HU_OK;
    }
    if (strcmp(argv[2], "status") == 0) {
        const char *provider = (argc >= 4) ? argv[3] : "default";
        printf("%s: not authenticated\n", provider);
        return HU_OK;
    }
    if (strcmp(argv[2], "login") == 0) {
        const char *provider = (argc >= 4) ? argv[3] : "default";
        printf("Login for %s: configure API key in ~/.human/config.json\n", provider);
        return HU_OK;
    }
    if (strcmp(argv[2], "logout") == 0) {
        const char *provider = (argc >= 4) ? argv[3] : "default";
        printf("%s: no credentials found\n", provider);
        return HU_OK;
    }
    fprintf(stderr, "Unknown auth subcommand: %s\n", argv[2]);
    return HU_ERR_INVALID_ARGUMENT;
}

/* ── sandbox ────────────────────────────────────────────────────────────── */
hu_error_t cmd_sandbox(hu_allocator_t *alloc, int argc, char **argv) {
    (void)argc;
    (void)argv;

    hu_config_t cfg;
    hu_error_t err = hu_config_load(alloc, &cfg);
    if (err != HU_OK) {
        fprintf(stderr, "Failed to load config: %s\n", hu_error_string(err));
        return err;
    }

    const char *ws = cfg.workspace_dir ? cfg.workspace_dir : ".";
    hu_sandbox_backend_t backend = cfg.security.sandbox_config.backend;

    printf("Sandbox Configuration\n");
    printf("  backend:       %s\n", cfg.security.sandbox ? cfg.security.sandbox : "auto");
    printf("  configured:    %s\n",
           cfg.security.sandbox_config.enabled ? "yes (explicit)" : "auto-detect");

    /* Detect available backends */
    hu_sandbox_alloc_t sa = {.ctx = alloc->ctx, .alloc = alloc->alloc, .free = alloc->free};
    hu_available_backends_t avail = hu_sandbox_detect_available(ws, &sa);

    printf("\nAvailable Backends\n");
    struct {
        const char *name;
        bool available;
        const char *desc;
    } backends[] = {
#ifdef __APPLE__
        {"seatbelt", avail.seatbelt, "macOS kernel sandbox (SBPL)"},
#endif
#ifdef __linux__
        {"landlock", avail.landlock, "Linux kernel FS ACLs"},
        {"seccomp", avail.seccomp, "Linux syscall filtering"},
        {"landlock+seccomp", avail.landlock_seccomp, "Combined FS + syscall isolation"},
        {"firejail", avail.firejail, "User-space namespace sandbox"},
        {"bubblewrap", avail.bubblewrap, "User-space container sandbox"},
        {"firecracker", avail.firecracker, "KVM microVM isolation"},
#endif
#ifdef _WIN32
        {"appcontainer", avail.appcontainer, "Windows AppContainer + Job Object"},
#endif
        {"docker", avail.docker, "Docker container isolation"},
        {"wasi", avail.wasi, "WebAssembly capability sandbox"},
    };
    size_t n = sizeof(backends) / sizeof(backends[0]);
    for (size_t i = 0; i < n; i++) {
        printf("  %-20s %s  %s\n", backends[i].name,
               backends[i].available ? "[available]  " : "[unavailable]", backends[i].desc);
    }

    /* Show active sandbox */
    hu_sandbox_storage_t *st = hu_sandbox_storage_create(&sa);
    if (st) {
        hu_sandbox_t sb = hu_sandbox_create(backend, ws, st, &sa);
        printf("\nActive Sandbox\n");
        printf("  name:        %s\n", hu_sandbox_name(&sb));
        printf("  available:   %s\n", hu_sandbox_is_available(&sb) ? "yes" : "no");
        printf("  description: %s\n", hu_sandbox_description(&sb));
        printf("  apply:       %s\n",
               (sb.vtable && sb.vtable->apply) ? "kernel-level" : "argv-wrapping");
        hu_sandbox_storage_destroy(st, &sa);
    }

    /* Network proxy */
    printf("\nNetwork Proxy\n");
    if (cfg.security.sandbox_config.net_proxy.enabled) {
        printf("  enabled:  yes\n");
        printf("  deny_all: %s\n", cfg.security.sandbox_config.net_proxy.deny_all ? "yes" : "no");
        if (cfg.security.sandbox_config.net_proxy.proxy_addr)
            printf("  proxy:    %s\n", cfg.security.sandbox_config.net_proxy.proxy_addr);
        if (cfg.security.sandbox_config.net_proxy.allowed_domains_len > 0) {
            printf("  allowed:  ");
            for (size_t i = 0; i < cfg.security.sandbox_config.net_proxy.allowed_domains_len; i++) {
                if (i > 0)
                    printf(", ");
                printf("%s", cfg.security.sandbox_config.net_proxy.allowed_domains[i]);
            }
            printf("\n");
        }
    } else {
        printf("  enabled:  no\n");
    }

    hu_config_deinit(&cfg);
    return HU_OK;
}

/* ── update ─────────────────────────────────────────────────────────────── */
hu_error_t cmd_update(hu_allocator_t *alloc, int argc, char **argv) {
#ifdef HU_HAS_UPDATE
    (void)alloc;
    bool check_only = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0)
            check_only = true;
    }
    const char *ver = hu_version_string();
    printf("Human v%s\n", ver ? ver : "0.4.0");

    char latest[64];
    hu_error_t err = hu_update_check(latest, sizeof(latest));
    if (err != HU_OK) {
        printf("Could not check for updates. Check https://github.com/sethdford/h-uman/releases\n");
        return HU_OK;
    }

    const char *current = ver ? ver : "0.0.0";
    const char *remote = latest;
    if (remote[0] == 'v')
        remote++;
    if (strcmp(current, remote) == 0) {
        printf("Already up to date.\n");
        return HU_OK;
    }
    printf("Update available: %s -> %s\n", current, latest);
    if (check_only)
        return HU_OK;

    printf("Downloading update...\n");
    err = hu_update_apply();
    if (err != HU_OK)
        printf(
            "Update failed. Download manually from https://github.com/sethdford/h-uman/releases\n");
    return HU_OK;
#else
    (void)alloc;
    (void)argc;
    (void)argv;
    fprintf(stderr, "[human] update support not built (compile with HU_ENABLE_UPDATE=ON)\n");
    return HU_ERR_NOT_SUPPORTED;
#endif
}

/* ── eval (run/list/compare) ────────────────────────────────────────────── */
hu_error_t cmd_eval(hu_allocator_t *alloc, int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: human eval <run|list|compare|dashboard|history> [args]\n");
        printf("  run <suite.json>     Load and run an eval suite, print report JSON\n");
        printf("  list                 List available eval suites in eval_suites/\n");
        printf("  compare <r1> <r2>    Compare two run report JSON files\n");
        printf("  dashboard [r1.json]  Render terminal dashboard from run report(s)\n");
        printf("  history [--last N] [--benchmark X]  Show eval history from SQLite\n");
        return HU_OK;
    }
    const char *sub = argv[2];

    if (strcmp(sub, "run") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: human eval run <suite.json>\n");
            return HU_ERR_INVALID_ARGUMENT;
        }
        const char *path = argv[3];
        char *json = NULL;
        size_t json_len = 0;

#ifdef HU_IS_TEST
        (void)path;
        static const char MOCK_SUITE[] =
            "{\"name\":\"mock-eval\",\"tasks\":["
            "{\"id\":\"m1\",\"prompt\":\"2+2?\",\"expected\":\"4\",\"category\":\"math\",\"difficulty\":1,\"timeout_ms\":5000}"
            "]}\n";
        json_len = sizeof(MOCK_SUITE) - 1;
        json = alloc->alloc(alloc->ctx, json_len + 1);
        if (!json)
            return HU_ERR_OUT_OF_MEMORY;
        memcpy(json, MOCK_SUITE, json_len + 1);
#else
        {
            FILE *f = fopen(path, "rb");
            if (!f) {
                fprintf(stderr, "eval: cannot open %s: %s\n", path, strerror(errno));
                return HU_ERR_IO;
            }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz <= 0 || sz > 1024 * 1024) {
                fclose(f);
                fprintf(stderr, "eval: invalid file size\n");
                return HU_ERR_INVALID_ARGUMENT;
            }
            json_len = (size_t)sz;
            json = alloc->alloc(alloc->ctx, json_len + 1);
            if (!json) {
                fclose(f);
                return HU_ERR_OUT_OF_MEMORY;
            }
            size_t n = fread(json, 1, json_len, f);
            fclose(f);
            if (n != json_len) {
                alloc->free(alloc->ctx, json, json_len + 1);
                return HU_ERR_IO;
            }
            json[json_len] = '\0';
        }
#endif

        hu_eval_suite_t suite = {0};
        hu_error_t err = hu_eval_suite_load_json(alloc, json, json_len, &suite);
#ifndef HU_IS_TEST
        alloc->free(alloc->ctx, json, json_len + 1);
#endif
        if (err != HU_OK) {
#ifdef HU_IS_TEST
            alloc->free(alloc->ctx, json, json_len + 1);
#endif
            fprintf(stderr, "eval: failed to load suite: %s\n", hu_error_string(err));
            return err;
        }
#ifdef HU_IS_TEST
        alloc->free(alloc->ctx, json, json_len + 1);
#endif

        hu_eval_run_t run = {0};
#ifdef HU_IS_TEST
        err = hu_eval_run_suite(alloc, NULL, "mock", 4, &suite, HU_EVAL_CONTAINS, &run);
#else
        {
            hu_config_t cfg;
            hu_error_t cfg_err = hu_config_load(alloc, &cfg);
            if (cfg_err != HU_OK) {
                hu_eval_suite_free(alloc, &suite);
                fprintf(stderr, "eval: config error: %s\n", hu_error_string(cfg_err));
                return cfg_err;
            }
            const char *prov = cfg.default_provider ? cfg.default_provider : "openai";
            size_t prov_len = strlen(prov);
            const char *model = cfg.default_model ? cfg.default_model : "";
            size_t model_len = model ? strlen(model) : 0;
            hu_provider_t provider = {0};
            err = hu_provider_create_from_config(alloc, &cfg, prov, prov_len, &provider);
            hu_config_deinit(&cfg);
            if (err != HU_OK) {
                hu_eval_suite_free(alloc, &suite);
                fprintf(stderr, "eval: provider error: %s\n", hu_error_string(err));
                return err;
            }
            err = hu_eval_run_suite(alloc, &provider, model, model_len, &suite,
                                   HU_EVAL_CONTAINS, &run);
            if (provider.vtable && provider.vtable->deinit)
                provider.vtable->deinit(provider.ctx, alloc);
        }
#endif
        hu_eval_suite_free(alloc, &suite);
        if (err != HU_OK) {
            fprintf(stderr, "eval: run failed: %s\n", hu_error_string(err));
            return err;
        }

        char *report = NULL;
        size_t report_len = 0;
        err = hu_eval_report_json(alloc, &run, &report, &report_len);
        if (err == HU_OK && report) {
            printf("%.*s\n", (int)report_len, report);
            alloc->free(alloc->ctx, report, report_len + 1);
        }

#ifdef HU_ENABLE_SQLITE
        {
            hu_config_t store_cfg;
            if (hu_config_load(alloc, &store_cfg) == HU_OK) {
                const char *ws = store_cfg.workspace_dir ? store_cfg.workspace_dir : ".";
                hu_memory_t mem = hu_memory_create_from_config(alloc, &store_cfg, ws);
                sqlite3 *db = mem.vtable ? hu_sqlite_memory_get_db(&mem) : NULL;
                if (db) {
                    hu_error_t tbl_err = hu_eval_init_tables(db);
                    if (tbl_err != HU_OK)
                        fprintf(stderr, "eval: table init failed\n");
                    if (hu_eval_store_run(alloc, db, &run) == HU_OK)
                        fprintf(stderr, "eval: stored run to history\n");

                    hu_eval_regression_t reg = {0};
                    if (hu_eval_detect_regression(db, run.suite_name,
                            run.pass_rate, 0.05, &reg) == HU_OK && reg.regressed) {
                        fprintf(stderr,
                            "WARNING: regression detected! pass_rate %.1f%% vs baseline %.1f%% (delta %.1f%%)\n",
                            reg.current_pass_rate * 100.0,
                            reg.baseline_pass_rate * 100.0,
                            reg.delta * 100.0);
                    }
                }
                if (mem.vtable && mem.vtable->deinit)
                    mem.vtable->deinit(mem.ctx);
                hu_config_deinit(&store_cfg);
            }
        }
#endif

        hu_eval_run_free(alloc, &run);
        return err;
    }

    if (strcmp(sub, "list") == 0) {
#if defined(HU_IS_TEST) || !(defined(__unix__) || defined(__APPLE__))
        printf("eval_suites/reasoning_basic.json\n");
        printf("eval_suites/tool_use_basic.json\n");
        printf("eval_suites/coding_basic.json\n");
        return HU_OK;
#else
        {
            const char *dir_path = "eval_suites";
            DIR *d = opendir(dir_path);
            if (!d) {
                fprintf(stderr, "eval: cannot open %s: %s\n", dir_path, strerror(errno));
                return HU_ERR_IO;
            }
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (e->d_name[0] == '.')
                    continue;
                size_t len = strlen(e->d_name);
                if (len >= 5 && strcmp(e->d_name + len - 5, ".json") == 0)
                    printf("%s/%s\n", dir_path, e->d_name);
            }
            closedir(d);
        }
        return HU_OK;
#endif
    }

    if (strcmp(sub, "compare") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: human eval compare <run1.json> <run2.json>\n");
            return HU_ERR_INVALID_ARGUMENT;
        }
        const char *path1 = argv[3];
        const char *path2 = argv[4];
        char *json1 = NULL, *json2 = NULL;
        size_t len1 = 0, len2 = 0;

#ifndef HU_IS_TEST
        {
            FILE *f1 = fopen(path1, "rb");
            if (!f1) {
                fprintf(stderr, "eval: cannot open %s: %s\n", path1, strerror(errno));
                return HU_ERR_IO;
            }
            fseek(f1, 0, SEEK_END);
            long sz1 = ftell(f1);
            fseek(f1, 0, SEEK_SET);
            if (sz1 <= 0 || sz1 > 65536) {
                fclose(f1);
                return HU_ERR_INVALID_ARGUMENT;
            }
            len1 = (size_t)sz1;
            json1 = alloc->alloc(alloc->ctx, len1 + 1);
            if (!json1) {
                fclose(f1);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (fread(json1, 1, len1, f1) != len1) {
                fclose(f1);
                alloc->free(alloc->ctx, json1, len1 + 1);
                return HU_ERR_IO;
            }
            json1[len1] = '\0';
            fclose(f1);

            FILE *f2 = fopen(path2, "rb");
            if (!f2) {
                alloc->free(alloc->ctx, json1, len1 + 1);
                fprintf(stderr, "eval: cannot open %s: %s\n", path2, strerror(errno));
                return HU_ERR_IO;
            }
            fseek(f2, 0, SEEK_END);
            long sz2 = ftell(f2);
            fseek(f2, 0, SEEK_SET);
            if (sz2 <= 0 || sz2 > 65536) {
                fclose(f2);
                alloc->free(alloc->ctx, json1, len1 + 1);
                return HU_ERR_INVALID_ARGUMENT;
            }
            len2 = (size_t)sz2;
            json2 = alloc->alloc(alloc->ctx, len2 + 1);
            if (!json2) {
                fclose(f2);
                alloc->free(alloc->ctx, json1, len1 + 1);
                return HU_ERR_OUT_OF_MEMORY;
            }
            if (fread(json2, 1, len2, f2) != len2) {
                fclose(f2);
                alloc->free(alloc->ctx, json1, len1 + 1);
                alloc->free(alloc->ctx, json2, len2 + 1);
                return HU_ERR_IO;
            }
            json2[len2] = '\0';
            fclose(f2);
        }
#else
        {
            static const char MOCK_R1[] = "{\"suite\":\"s1\",\"passed\":8,\"failed\":2,\"pass_rate\":0.80,\"elapsed_ms\":1000}";
            static const char MOCK_R2[] = "{\"suite\":\"s1\",\"passed\":9,\"failed\":1,\"pass_rate\":0.90,\"elapsed_ms\":900}";
            (void)path1;
            (void)path2;
            len1 = sizeof(MOCK_R1) - 1;
            len2 = sizeof(MOCK_R2) - 1;
            json1 = alloc->alloc(alloc->ctx, len1 + 1);
            json2 = alloc->alloc(alloc->ctx, len2 + 1);
            if (json1)
                memcpy(json1, MOCK_R1, len1 + 1);
            if (json2)
                memcpy(json2, MOCK_R2, len2 + 1);
        }
#endif

        if (!json1 || !json2) {
            if (json1) alloc->free(alloc->ctx, json1, len1 + 1);
            if (json2) alloc->free(alloc->ctx, json2, len2 + 1);
            return HU_ERR_OUT_OF_MEMORY;
        }

        hu_eval_run_t baseline = {0}, current = {0};
        hu_error_t err = hu_eval_run_load_json(alloc, json1, len1, &baseline);
        if (err == HU_OK)
            err = hu_eval_run_load_json(alloc, json2, len2, &current);
        alloc->free(alloc->ctx, json1, len1 + 1);
        alloc->free(alloc->ctx, json2, len2 + 1);
        if (err != HU_OK) {
            hu_eval_run_free(alloc, &baseline);
            fprintf(stderr, "eval: failed to parse run reports\n");
            return err;
        }

        char *report = NULL;
        size_t report_len = 0;
        err = hu_eval_compare(alloc, &baseline, &current, &report, &report_len);
        hu_eval_run_free(alloc, &baseline);
        hu_eval_run_free(alloc, &current);
        if (err == HU_OK && report) {
            printf("%.*s\n", (int)report_len, report);
            alloc->free(alloc->ctx, report, report_len + 1);
        }
        return err;
    }

    if (strcmp(sub, "dashboard") == 0) {
        hu_eval_run_t *runs = NULL;
        size_t runs_count = 0;
        hu_error_t err = HU_OK;

        if (argc >= 4) {
            runs_count = (size_t)(argc - 3);
            runs = alloc->alloc(alloc->ctx, runs_count * sizeof(hu_eval_run_t));
            if (!runs) return HU_ERR_OUT_OF_MEMORY;
            memset(runs, 0, runs_count * sizeof(hu_eval_run_t));

            for (size_t i = 0; i < runs_count; i++) {
                const char *path = argv[3 + (int)i];
#ifdef HU_IS_TEST
                (void)path;
                static const char MOCK[] = "{\"suite\":\"reasoning-basic\",\"passed\":8,\"failed\":2,\"pass_rate\":0.80,\"elapsed_ms\":120}";
                size_t len = sizeof(MOCK) - 1;
                err = hu_eval_run_load_json(alloc, MOCK, len, &runs[i]);
#else
                FILE *f = fopen(path, "rb");
                if (!f) {
                    fprintf(stderr, "eval: cannot open %s: %s\n", path, strerror(errno));
                    err = HU_ERR_IO;
                    break;
                }
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                if (sz <= 0 || sz > 65536) {
                    fclose(f);
                    err = HU_ERR_INVALID_ARGUMENT;
                    break;
                }
                size_t len = (size_t)sz;
                char *json = alloc->alloc(alloc->ctx, len + 1);
                if (!json) {
                    fclose(f);
                    err = HU_ERR_OUT_OF_MEMORY;
                    break;
                }
                if (fread(json, 1, len, f) != len) {
                    fclose(f);
                    alloc->free(alloc->ctx, json, len + 1);
                    err = HU_ERR_IO;
                    break;
                }
                json[len] = '\0';
                fclose(f);
                err = hu_eval_run_load_json(alloc, json, len, &runs[i]);
                alloc->free(alloc->ctx, json, len + 1);
#endif
                if (err != HU_OK) break;
            }

            if (err == HU_OK)
                err = hu_eval_dashboard_render(alloc, stdout, runs, runs_count);

            for (size_t i = 0; i < runs_count; i++)
                hu_eval_run_free(alloc, &runs[i]);
            alloc->free(alloc->ctx, runs, runs_count * sizeof(hu_eval_run_t));
        } else {
            err = hu_eval_dashboard_render(alloc, stdout, NULL, 0);
        }
        return err;
    }

    if (strcmp(sub, "history") == 0) {
#ifdef HU_ENABLE_SQLITE
        size_t max_runs = 10;
        const char *filter_suite = NULL;
        for (int a = 3; a < argc; a++) {
            if (strcmp(argv[a], "--last") == 0 && a + 1 < argc) {
                max_runs = (size_t)atoi(argv[++a]);
                if (max_runs == 0 || max_runs > 100)
                    max_runs = 10;
            } else if (strcmp(argv[a], "--benchmark") == 0 && a + 1 < argc) {
                filter_suite = argv[++a];
            }
        }
        hu_config_t cfg;
        hu_error_t cfg_err = hu_config_load(alloc, &cfg);
        if (cfg_err != HU_OK) {
            fprintf(stderr, "eval history: config error\n");
            return cfg_err;
        }
        const char *ws = cfg.workspace_dir ? cfg.workspace_dir : ".";
        hu_memory_t mem = hu_memory_create_from_config(alloc, &cfg, ws);
        sqlite3 *db = mem.vtable ? hu_sqlite_memory_get_db(&mem) : NULL;
        if (!db) {
            fprintf(stderr, "eval history: no SQLite backend\n");
            if (mem.vtable && mem.vtable->deinit)
                mem.vtable->deinit(mem.ctx);
            hu_config_deinit(&cfg);
            return HU_ERR_NOT_FOUND;
        }
        hu_error_t tbl_err = hu_eval_init_tables(db);
        if (tbl_err != HU_OK)
            fprintf(stderr, "eval: table init failed\n");
        hu_eval_run_t *runs = alloc->alloc(alloc->ctx, max_runs * sizeof(hu_eval_run_t));
        if (!runs) {
            if (mem.vtable && mem.vtable->deinit)
                mem.vtable->deinit(mem.ctx);
            hu_config_deinit(&cfg);
            return HU_ERR_OUT_OF_MEMORY;
        }
        memset(runs, 0, max_runs * sizeof(hu_eval_run_t));
        size_t count = 0;
        hu_error_t err = hu_eval_load_history(alloc, db, runs, max_runs, &count);
        if (err == HU_OK && count > 0) {
            printf("%-20s %-12s %-10s %6s %6s %8s\n",
                   "Suite", "Provider", "Model", "Pass", "Fail", "Rate");
            printf("%-20s %-12s %-10s %6s %6s %8s\n",
                   "----", "--------", "-----", "----", "----", "----");
            for (size_t i = 0; i < count; i++) {
                if (filter_suite && runs[i].suite_name &&
                    strcmp(runs[i].suite_name, filter_suite) != 0)
                    continue;
                printf("%-20s %-12s %-10s %6zu %6zu %7.1f%%\n",
                       runs[i].suite_name ? runs[i].suite_name : "-",
                       runs[i].provider ? runs[i].provider : "-",
                       runs[i].model ? runs[i].model : "-",
                       runs[i].passed, runs[i].failed,
                       runs[i].pass_rate * 100.0);
            }
        } else {
            printf("No eval history found.\n");
        }
        for (size_t i = 0; i < count; i++)
            hu_eval_run_free(alloc, &runs[i]);
        alloc->free(alloc->ctx, runs, max_runs * sizeof(hu_eval_run_t));
        if (mem.vtable && mem.vtable->deinit)
            mem.vtable->deinit(mem.ctx);
        hu_config_deinit(&cfg);
        return err;
#else
        fprintf(stderr, "eval history: SQLite not enabled\n");
        return HU_ERR_NOT_SUPPORTED;
#endif
    }

    fprintf(stderr, "Unknown eval subcommand: %s\n", sub);
    return HU_ERR_INVALID_ARGUMENT;
}

/* ── feed ──────────────────────────────────────────────────────────────── */
#if defined(HU_ENABLE_FEEDS) && defined(HU_ENABLE_SQLITE)
hu_error_t cmd_feed(hu_allocator_t *alloc, int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: human feed <poll|status|list|health|findings|search|digest|correlate|cleanup|learn>\n");
        return HU_OK;
    }
    hu_config_t cfg;
    hu_error_t err = hu_config_load(alloc, &cfg);
    if (err != HU_OK) {
        fprintf(stderr, "Config error: %s\n", hu_error_string(err));
        return err;
    }
    const char *ws = cfg.workspace_dir ? cfg.workspace_dir : ".";
    hu_memory_t mem = hu_memory_create_from_config(alloc, &cfg, ws);
    if (!mem.vtable) {
        fprintf(stderr, "[feed] No memory backend configured\n");
        hu_config_deinit(&cfg);
        return HU_ERR_NOT_FOUND;
    }
    sqlite3 *db = hu_sqlite_memory_get_db(&mem);
    if (!db) {
        fprintf(stderr, "[feed] SQLite database not available\n");
        if (mem.vtable->deinit) mem.vtable->deinit(mem.ctx);
        hu_config_deinit(&cfg);
        return HU_ERR_NOT_FOUND;
    }
    const char *sub = argv[2];

    if (strcmp(sub, "poll") == 0) {
        hu_feed_processor_t fp = {.alloc = alloc, .db = db};
        hu_feed_config_t fconf;
        memset(&fconf, 0, sizeof(fconf));
        fconf.enabled[HU_FEED_NEWS_RSS]    = true;
        fconf.enabled[HU_FEED_FILE_INGEST] = true;
        fconf.enabled[HU_FEED_GMAIL]       = true;
        fconf.enabled[HU_FEED_IMESSAGE]    = true;
        fconf.enabled[HU_FEED_TWITTER]     = true;
        fconf.poll_interval_minutes[HU_FEED_FILE_INGEST] = 0;
        fconf.poll_interval_minutes[HU_FEED_NEWS_RSS]    = 0;
        fconf.poll_interval_minutes[HU_FEED_GMAIL]       = 0;
        fconf.poll_interval_minutes[HU_FEED_IMESSAGE]    = 0;
        fconf.poll_interval_minutes[HU_FEED_TWITTER]     = 0;
        fconf.max_items_per_poll = 50;
        uint64_t last_poll[HU_FEED_COUNT] = {0};
        uint64_t now_ms = (uint64_t)time(NULL) * 1000ULL;
        size_t ingested = 0;
        err = hu_feed_processor_poll(&fp, &fconf, last_poll, now_ms, &ingested);
        if (err == HU_OK)
            printf("[feed] Poll complete: %zu items ingested\n", ingested);
        else
            fprintf(stderr, "[feed] Poll error: %s\n", hu_error_string(err));
    } else if (strcmp(sub, "status") == 0) {
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(db,
            "SELECT source, COUNT(*), MAX(ingested_at) FROM feed_items GROUP BY source",
            -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[feed] Query error: %s\n", sqlite3_errmsg(db));
        } else {
            printf("%-20s  %6s  %s\n", "SOURCE", "COUNT", "LAST INGESTED");
            printf("%-20s  %6s  %s\n", "--------------------", "------", "-------------------");
            int total = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *src = (const char *)sqlite3_column_text(stmt, 0);
                int cnt = sqlite3_column_int(stmt, 1);
                int64_t ts = sqlite3_column_int64(stmt, 2);
                total += cnt;
                char timebuf[32] = "unknown";
                if (ts > 0) {
                    time_t tt = (time_t)ts;
                    struct tm *lt = localtime(&tt);
                    if (lt)
                        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", lt);
                }
                printf("%-20s  %6d  %s\n", src ? src : "(null)", cnt, timebuf);
            }
            printf("%-20s  %6d\n", "TOTAL", total);
            sqlite3_finalize(stmt);
        }
        /* Intelligence stats */
        printf("\nIntelligence:\n");
        sqlite3_stmt *istmt = NULL;
        int irc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM research_findings WHERE status = 'actioned'", -1, &istmt, NULL);
        if (irc == SQLITE_OK && sqlite3_step(istmt) == SQLITE_ROW)
            printf("  Findings actioned: %d\n", sqlite3_column_int(istmt, 0));
        if (istmt) sqlite3_finalize(istmt);

        irc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM general_lessons", -1, &istmt, NULL);
        if (irc == SQLITE_OK && sqlite3_step(istmt) == SQLITE_ROW)
            printf("  General lessons:   %d\n", sqlite3_column_int(istmt, 0));
        if (istmt) sqlite3_finalize(istmt);

        irc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM current_events", -1, &istmt, NULL);
        if (irc == SQLITE_OK && sqlite3_step(istmt) == SQLITE_ROW)
            printf("  Current events:    %d\n", sqlite3_column_int(istmt, 0));
        if (istmt) sqlite3_finalize(istmt);

        irc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM inferred_values", -1, &istmt, NULL);
        if (irc == SQLITE_OK && sqlite3_step(istmt) == SQLITE_ROW)
            printf("  Inferred values:   %d\n", sqlite3_column_int(istmt, 0));
        if (istmt) sqlite3_finalize(istmt);

        irc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM growth_milestones", -1, &istmt, NULL);
        if (irc == SQLITE_OK && sqlite3_step(istmt) == SQLITE_ROW)
            printf("  Growth milestones: %d\n", sqlite3_column_int(istmt, 0));
        if (istmt) sqlite3_finalize(istmt);
    } else if (strcmp(sub, "list") == 0) {
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(db,
            "SELECT source, content_type, substr(content, 1, 80), ingested_at "
            "FROM feed_items ORDER BY ingested_at DESC LIMIT 10",
            -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[feed] Query error: %s\n", sqlite3_errmsg(db));
        } else {
            printf("Last 10 feed items:\n\n");
            int idx = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *src = (const char *)sqlite3_column_text(stmt, 0);
                const char *ctype = (const char *)sqlite3_column_text(stmt, 1);
                const char *content = (const char *)sqlite3_column_text(stmt, 2);
                int64_t ts = sqlite3_column_int64(stmt, 3);
                char timebuf[32] = "unknown";
                if (ts > 0) {
                    time_t tt = (time_t)ts;
                    struct tm *lt = localtime(&tt);
                    if (lt)
                        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", lt);
                }
                printf("[%d] %s (%s) @ %s\n    %s\n\n",
                       ++idx, src ? src : "?", ctype ? ctype : "?",
                       timebuf, content ? content : "(empty)");
            }
            if (idx == 0)
                printf("  (no feed items yet)\n");
            sqlite3_finalize(stmt);
        }
    } else if (strcmp(sub, "health") == 0) {
        printf("Feed Health Report\n");
        printf("==================\n\n");
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(db,
            "SELECT source, COUNT(*), MAX(ingested_at) FROM feed_items GROUP BY source",
            -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            time_t now = time(NULL);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *src = (const char *)sqlite3_column_text(stmt, 0);
                int cnt = sqlite3_column_int(stmt, 1);
                int64_t last_ts = sqlite3_column_int64(stmt, 2);
                double hours_ago = (last_ts > 0) ? difftime(now, (time_t)last_ts) / 3600.0 : -1;
                const char *status_icon = (hours_ago >= 0 && hours_ago < 24) ? "OK" : "STALE";
                if (hours_ago < 0) status_icon = "NEVER";
                printf("  %-16s  %4d items  last=%.1fh ago  [%s]\n",
                       src ? src : "?", cnt, hours_ago >= 0 ? hours_ago : 0.0, status_icon);
            }
            sqlite3_finalize(stmt);
        }
        const char *home = getenv("HOME");
        if (home) {
            char ingest_dir[512];
            snprintf(ingest_dir, sizeof(ingest_dir), "%s/.human/feeds/ingest", home);
            printf("\nIngest directory: %s\n", ingest_dir);
            struct stat st;
            if (stat(ingest_dir, &st) == 0)
                printf("  Status: exists\n");
            else
                printf("  Status: NOT FOUND (create with: mkdir -p %s)\n", ingest_dir);
        }
        printf("\n");
    } else if (strcmp(sub, "findings") == 0) {
        hu_research_finding_t *findings = NULL;
        size_t fcount = 0;
        const char *filter = (argc >= 4) ? argv[3] : "all";
        hu_error_t ferr;
        if (strcmp(filter, "pending") == 0)
            ferr = hu_findings_get_pending(alloc, db, 50, &findings, &fcount);
        else
            ferr = hu_findings_get_all(alloc, db, 50, &findings, &fcount);
        if (ferr != HU_OK) {
            fprintf(stderr, "[feed] Findings query error: %s\n", hu_error_string(ferr));
        } else if (fcount == 0) {
            printf("No research findings yet. Run the research agent first.\n");
        } else {
            printf("Research Findings (%zu %s):\n\n", fcount,
                   strcmp(filter, "pending") == 0 ? "pending" : "total");
            for (size_t i = 0; i < fcount; i++) {
                hu_research_finding_t *f = &findings[i];
                char timebuf[32] = "unknown";
                if (f->created_at > 0) {
                    time_t tt = (time_t)f->created_at;
                    struct tm *lt = localtime(&tt);
                    if (lt) strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", lt);
                }
                printf("[%lld] [%s] %s @ %s\n", (long long)f->id,
                       f->priority[0] ? f->priority : "?",
                       f->status[0] ? f->status : "?", timebuf);
                printf("  Finding: %.120s%s\n",
                       f->finding, strlen(f->finding) > 120 ? "..." : "");
                if (f->source[0])
                    printf("  Source: %s\n", f->source);
                if (f->suggested_action[0])
                    printf("  Action: %.100s%s\n",
                           f->suggested_action, strlen(f->suggested_action) > 100 ? "..." : "");
                printf("\n");
            }
            hu_findings_free(alloc, findings, fcount);
        }
    } else if (strcmp(sub, "search") == 0) {
        if (argc < 4) {
            printf("Usage: human feed search <query>\n");
        } else {
            const char *query = argv[3];
            hu_feed_item_stored_t *results = NULL;
            size_t rcount = 0;
            hu_error_t serr = hu_feed_search(alloc, db, query, strlen(query), 20,
                                             &results, &rcount);
            if (serr != HU_OK) {
                fprintf(stderr, "[feed] Search error: %s\n", hu_error_string(serr));
            } else if (rcount == 0) {
                printf("No results for '%s'\n", query);
            } else {
                printf("Feed search results for '%s' (%zu matches):\n\n", query, rcount);
                for (size_t i = 0; i < rcount; i++) {
                    hu_feed_item_stored_t *r = &results[i];
                    char timebuf[32] = "unknown";
                    if (r->ingested_at > 0) {
                        time_t tt = (time_t)r->ingested_at;
                        struct tm *lt = localtime(&tt);
                        if (lt) strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M", lt);
                    }
                    printf("[%zu] %s (%s) @ %s\n    %.100s%s\n",
                           i + 1, r->source, r->content_type, timebuf,
                           r->content, r->content_len > 100 ? "..." : "");
                    if (r->url[0]) printf("    %s\n", r->url);
                    printf("\n");
                }
                hu_feed_items_free(alloc, results, rcount);
            }
        }
    } else if (strcmp(sub, "digest") == 0) {
        char *digest_text = NULL;
        size_t digest_text_len = 0;
        int64_t since = (int64_t)time(NULL) - 86400;
        hu_error_t derr = hu_feed_build_daily_digest(alloc, db, since, 8000,
                                                     &digest_text, &digest_text_len);
        if (derr != HU_OK) {
            fprintf(stderr, "[feed] Digest error: %s\n", hu_error_string(derr));
        } else {
            printf("%.*s", (int)digest_text_len, digest_text);
            alloc->free(alloc->ctx, digest_text, digest_text_len + 1);
        }
    } else if (strcmp(sub, "correlate") == 0) {
        int64_t since = (int64_t)time(NULL) - 86400;
        hu_error_t cerr = hu_feed_correlate_recent(alloc, db, since, 0.3);
        if (cerr == HU_OK) printf("[feed] Correlation complete\n");
        else fprintf(stderr, "[feed] Correlation error: %s\n", hu_error_string(cerr));
    } else if (strcmp(sub, "cleanup") == 0) {
        uint32_t days = 30;
        if (argc >= 4) { int d = atoi(argv[3]); if (d > 0 && d <= 365) days = (uint32_t)d; }
        hu_feed_processor_t fp = {.alloc = alloc, .db = db};
        hu_error_t cerr = hu_feed_processor_cleanup(&fp, days);
        if (cerr == HU_OK) printf("[feed] Cleanup complete (removed items older than %u days)\n", days);
        else fprintf(stderr, "[feed] Cleanup error: %s\n", hu_error_string(cerr));
    } else if (strcmp(sub, "learn") == 0) {
#if defined(HU_HAS_SKILLS)
        hu_intelligence_cycle_result_t learn_result;
        memset(&learn_result, 0, sizeof(learn_result));
        hu_error_t lerr = hu_intelligence_run_cycle(alloc, db, &learn_result);
        if (lerr == HU_OK) {
            printf("[feed] Intelligence cycle complete:\n");
            printf("  Findings actioned: %zu\n", learn_result.findings_actioned);
            printf("  Lessons extracted: %zu\n", learn_result.lessons_extracted);
            printf("  Events recorded:   %zu\n", learn_result.events_recorded);
            printf("  Values learned:    %zu\n", learn_result.values_learned);
            printf("  Causal recorded:   %zu\n", learn_result.causal_recorded);
            printf("  Skills updated:    %zu\n", learn_result.skills_updated);
        } else {
            fprintf(stderr, "[feed] Intelligence cycle error: %s\n", hu_error_string(lerr));
        }
#else
        fprintf(stderr, "[feed] Intelligence cycle requires HU_HAS_SKILLS\n");
#endif
    } else {
        fprintf(stderr, "Unknown feed subcommand: %s\n", sub);
        printf("Usage: human feed <poll|status|list|health|findings|search|digest|correlate|cleanup|learn>\n");
        err = HU_ERR_INVALID_ARGUMENT;
    }

    if (mem.vtable->deinit) mem.vtable->deinit(mem.ctx);
    hu_config_deinit(&cfg);
    return err;
}
#endif /* HU_ENABLE_FEEDS && HU_ENABLE_SQLITE */
