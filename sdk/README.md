# seaclaw Plugin SDK

This directory provides templates and examples for building third-party plugins that extend seaclaw: custom AI providers, messaging channels, and tools.

## Overview

seaclaw uses a **vtable-driven** architecture. Plugins implement struct vtables and register instances with the runtime. All extension points follow the same pattern: `(void *ctx, const struct sc_*_vtable *vtable)`.

## Prerequisites

- seaclaw headers in `include/seaclaw/`
- C11 compiler
- CMake 3.20+

## Quick Start

1. Copy a template from `sdk/templates/` (provider, channel, or tool).
2. Implement the vtable methods.
3. Build against seaclaw (link or compile into the main binary).
4. Register your plugin in the appropriate factory or manager.

## Templates

| Template | Path                      | Purpose                             |
| -------- | ------------------------- | ----------------------------------- |
| Provider | `sdk/templates/provider/` | Custom LLM backend                  |
| Channel  | `sdk/templates/channel/`  | Custom messaging channel            |
| Tool     | `sdk/templates/tool/`     | Custom tool (e.g. API call, script) |

## Examples

| Example      | Path                    | Description                            |
| ------------ | ----------------------- | -------------------------------------- |
| Weather Tool | `sdk/examples/weather/` | Complete working tool with JSON params |

## Registration

### Provider

Add your provider creation to `src/providers/factory.c` in `sc_provider_create()`:

```c
if (name_len == 10 && memcmp(name, "my_provider", 10) == 0) {
    return sc_my_provider_create(alloc, api_key, api_key_len,
        base_url, base_url_len, out);
}
```

Alternatively, build seaclaw with your provider as a linked object and extend the factory.

### Channel

Create your channel, then register with the channel manager:

```c
sc_channel_t my_ch;
sc_my_channel_create(&alloc, &my_ch);
sc_channel_manager_register(&mgr, "my_channel", "default", &my_ch,
    SC_CHANNEL_LISTENER_SEND_ONLY);
```

### Tool

Tools are created explicitly and passed to the agent. Either:

1. Add your tool to `sc_tools_create_default` in `src/tools/factory.c`, or
2. Build your own tool array and pass it to `sc_agent_from_config`:

```c
sc_tool_t tools[2];
sc_web_fetch_create(&alloc, 50000, &tools[0]);
sc_weather_create(&alloc, &tools[1]);
sc_agent_from_config(&agent, &alloc, provider, tools, 2, ...);
```

## SC_IS_TEST

Use `#if SC_IS_TEST` to bypass side effects in tests (network, process spawn, browser, etc.). Return stub data instead. This ensures tests are deterministic and do not require credentials or external services.

```c
#if SC_IS_TEST
    *out = sc_tool_result_ok("(stub)", 6);
    return SC_OK;
#else
    /* real implementation */
#endif
```

## Naming

- Types: `sc_<name>_t`
- Functions: `sc_<module>_<action>`
- Constants: `SC_SCREAMING_SNAKE_CASE`
- Factory registration keys: lowercase, user-facing (e.g. `"weather"`, `"my_channel"`)

## See Also

- [API Reference](../docs/api/README.md) for full type and function documentation
- [AGENTS.md](../AGENTS.md) for engineering protocol and change playbooks
