# Add a New Tool

Step-by-step workflow for adding a new tool to the human runtime. **Tools are high-risk** — they execute real-world actions.

## Pre-flight

1. Read `src/tools/CLAUDE.md` for module context and security rules
2. Read `include/human/tools.h` for the `hu_tool_t` vtable definition
3. Review `src/security/policy.c` for risk level classification

## Steps

### 1. Create the tool source

Create `src/tools/<name>.c` implementing `hu_tool_t`:

- `execute(ctx, params_json)` — parse params, validate inputs, perform action, return `hu_tool_result_t`
- `name()` — stable lowercase name (e.g., `"my_tool"`)
- `description()` — human-readable description for the LLM
- `parameters_json()` — JSON Schema describing accepted parameters

Use `src/tools/file_read.c` (simple) or `src/tools/shell.c` (complex with security) as references.

### 2. Input validation (CRITICAL)

In `execute`, before doing anything:

- Parse and validate ALL JSON parameters
- Reject path traversal (`../`), null bytes, command injection
- Enforce allowlists where applicable
- Length-limit all string inputs
- Return error result (don't crash) on invalid input

### 3. Create the header

Create `include/human/tools/<name>.h` with the factory function.

### 4. Register in factory

Add the tool to `src/tools/factory.c`:

- Include the header
- Add the factory entry

### 5. Add to CMakeLists.txt

Add the source file. If the tool requires optional dependencies, add a feature flag.

### 6. Add tests

Add tests for:

- Valid input producing expected output
- Invalid/missing parameters (boundary test)
- Security: path traversal, injection, unauthorized access
- Use `HU_IS_TEST` mock paths — no real network, no real filesystem writes

### 7. Build and verify

```bash
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
./build/human_tests --suite=tool
```

## Checklist

- [ ] Implements all required vtable methods
- [ ] ALL inputs validated and sanitized
- [ ] `HU_IS_TEST` guards for process spawning, network, filesystem writes
- [ ] Registered in `factory.c`
- [ ] Added to CMakeLists.txt
- [ ] Tests: valid input, boundary/failure, security
- [ ] Tests pass with 0 ASan errors
- [ ] URL tools reject non-HTTPS schemes
- [ ] File tools validate paths are within allowed directories
- [ ] Parameters JSON matches actual validation logic
