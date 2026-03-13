# src/runtime/ — Runtime Adapters (High Risk)

Runtime adapters manage execution environments — where and how the agent runs. Each adapter implements the `hu_runtime_t` vtable.

## Vtable Contract

Every runtime implements `hu_runtime_vtable_t`:

- `name(ctx)` — runtime identifier (e.g., `"native"`, `"docker"`)
- `has_shell_access(ctx)` — whether shell commands are available
- `has_filesystem_access(ctx)` — whether filesystem is available
- `storage_path(ctx)` — persistent storage directory
- `supports_long_running(ctx)` — can run persistent processes
- `memory_budget(ctx)` — memory limit in bytes (0 = unlimited)
- `wrap_command(ctx, argv_in, argv_out)` — transform commands for the runtime

## Implementations

```
native.c             Native OS execution (default — macOS, Linux)
docker.c             Docker container execution
wasm_rt.c            WebAssembly runtime (memory-limited)
cloudflare.c         Cloudflare Workers execution
gce.c                Google Compute Engine execution
factory.c            Runtime registry and config-driven creation
```

## Runtime Kinds

```c
HU_RUNTIME_NATIVE      — direct OS execution, full access
HU_RUNTIME_DOCKER      — containerized, optional workspace mount, memory limit
HU_RUNTIME_WASM        — WebAssembly sandbox, strict memory limit
HU_RUNTIME_CLOUDFLARE  — edge execution, no shell, no filesystem
HU_RUNTIME_GCE         — remote VM execution
```

## Key Functions

- `hu_runtime_native()` — create native runtime (zero-config)
- `hu_runtime_docker(mount, mem_limit, image, workspace)` — Docker with config
- `hu_runtime_from_config(cfg, out)` — create runtime from config file
- `wrap_command` — transforms shell commands (e.g., wraps in `docker exec`)

## Rules (Mandatory)

- Non-supported platforms must return `HU_ERR_NOT_SUPPORTED` — never silent no-ops
- Use `#ifdef` guards for platform-specific code (POSIX, Linux, macOS, WASM)
- Docker runtime: validate image names, reject shell metacharacters
- WASM runtime: validate module paths, enforce memory limits
- Never weaken sandbox policy without explicit justification
- Clean up resources on error paths (file descriptors, child processes, temp files)
- Use `HU_IS_TEST` for operations that spawn real processes or containers
- Test on both macOS and Linux (CI covers both)
