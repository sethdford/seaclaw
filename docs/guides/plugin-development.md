---
title: Plugin Development Guide
updated: 2026-03-23
---

# Plugin Development Guide

Build plugins for human that extend its capabilities at runtime — no recompilation needed. Plugins are shared libraries (`.so` on Linux, `.dylib` on macOS) that register tools, providers, and channels through the host API.

## Quick Start

```c
#include "human/plugin.h"

hu_error_t hu_plugin_init(hu_plugin_host_t *host, hu_plugin_info_t *info) {
    info->name = "my-plugin";
    info->version = "1.0.0";
    info->description = "A custom plugin for human";
    info->api_version = HU_PLUGIN_API_VERSION;

    /* Register your extensions here */
    return HU_OK;
}

void hu_plugin_deinit(void) {
    /* Clean up resources */
}
```

## ABI Contract

Every plugin must export exactly two symbols:

| Symbol | Signature | Required |
| --- | --- | --- |
| `hu_plugin_init` | `hu_error_t (hu_plugin_host_t *host, hu_plugin_info_t *info)` | Yes |
| `hu_plugin_deinit` | `void (void)` | Yes |

**API version**: Set `info->api_version = HU_PLUGIN_API_VERSION` (currently `1`). The loader rejects plugins with mismatched versions, preventing ABI breakage.

**Return HU_OK** from `hu_plugin_init` on success. Any other error code causes the loader to close the library and skip the plugin.

## Host API

The `hu_plugin_host_t` struct provides registration functions:

```c
typedef struct hu_plugin_host {
    hu_allocator_t *alloc;
    hu_error_t (*register_tool)(void *host_ctx, const char *name, void *tool_vtable);
    hu_error_t (*register_provider)(void *host_ctx, const char *name, void *provider_vtable);
    hu_error_t (*register_channel)(void *host_ctx, const hu_channel_t *channel);
    void *host_ctx;
} hu_plugin_host_t;
```

### Registering a Tool

```c
static hu_error_t my_tool_execute(void *ctx, hu_allocator_t *alloc,
                                   const char *args, size_t args_len,
                                   hu_tool_result_t *out) {
    /* Your tool logic here */
    out->content = "result";
    out->content_len = 6;
    return HU_OK;
}

hu_error_t hu_plugin_init(hu_plugin_host_t *host, hu_plugin_info_t *info) {
    info->name = "weather-plugin";
    info->version = "1.0.0";
    info->api_version = HU_PLUGIN_API_VERSION;

    static hu_tool_vtable_t vtable = {
        .execute = my_tool_execute,
        .name = my_tool_name,
        .description = my_tool_desc,
        .parameters_json = my_tool_params,
    };
    return host->register_tool(host->host_ctx, "weather", &vtable);
}
```

### Registering a Provider

Pass a `hu_provider_vtable_t *` to `register_provider`. The provider must implement `chat`, `get_name`, `supports_native_tools`, and `deinit`.

### Registering a Channel

Pass a fully initialized `hu_channel_t *` to `register_channel`. The channel must have its vtable set with at minimum `start`, `stop`, `send`, `name`, and `health_check`.

## Building a Plugin

```bash
# Compile as a shared library
gcc -shared -fPIC -o my_plugin.so my_plugin.c -I/path/to/human/include

# macOS
clang -shared -fPIC -o my_plugin.dylib my_plugin.c -I/path/to/human/include
```

## Installation

Place the compiled plugin in `~/.human/plugins/`:

```bash
cp my_plugin.so ~/.human/plugins/
```

The runtime auto-discovers plugins at startup. You can also specify plugin paths explicitly in `~/.human/config.json`:

```json
{
  "plugins": {
    "plugin_paths": [
      "/path/to/my_plugin.so"
    ]
  }
}
```

## Plugin Management CLI

```bash
human plugins list              # List discovered plugins
human plugins scan [directory]  # Scan a specific directory
human plugins dir               # Show the plugin directory path
```

## Testing

Guard side effects with `HU_IS_TEST`:

```c
hu_error_t my_tool_execute(void *ctx, ...) {
#if HU_IS_TEST
    /* Return mock data */
    out->content = "mock result";
    out->content_len = 11;
    return HU_OK;
#else
    /* Real implementation */
#endif
}
```

The plugin loader has a full mock path under `HU_IS_TEST` that validates arguments and returns test data without `dlopen`.

## Error Handling

- `HU_OK` — plugin loaded successfully
- `HU_ERR_NOT_FOUND` — shared library not found at path
- `HU_ERR_INVALID_ARGUMENT` — missing `hu_plugin_init` symbol or API version mismatch
- `HU_ERR_OUT_OF_MEMORY` — allocation failure during loading
- `HU_ERR_NOT_SUPPORTED` — plugin loading not supported on this platform (Windows)

## Lifecycle

1. **Discovery**: At bootstrap, human scans `~/.human/plugins/` for `.so`/`.dylib` files
2. **Loading**: Each library is opened with `dlopen(RTLD_NOW | RTLD_LOCAL)`
3. **Init**: `hu_plugin_init` is called with the host context
4. **Runtime**: Registered tools/providers/channels are available to the agent
5. **Shutdown**: `hu_plugin_deinit` is called, then the library is closed with `dlclose`

## Best Practices

- Keep plugins stateless where possible; use the allocator provided by the host
- Set `api_version` to `HU_PLUGIN_API_VERSION` — never hardcode a number
- Handle all error paths; return meaningful error codes
- Use `RTLD_LOCAL` (the loader does this) to avoid symbol collisions
- Test with the mock loader before testing with real `dlopen`
