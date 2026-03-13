# Pre-flight Validation

Run this workflow before committing or declaring work complete. Catches missed tests, lint, and build errors.

## Quick Check (changed files only)

```bash
scripts/agent-preflight.sh
```

This automatically detects what changed and runs the appropriate checks:

- **C files changed**: build + targeted tests via `what-to-test.sh`
- **UI files changed**: `npm run check` in `ui/`
- **Design tokens changed**: `design-tokens/check-drift.sh`
- **Website files changed**: `npm run build` in `website/`

## Full Check (everything)

```bash
scripts/agent-preflight.sh --full
```

Runs all checks regardless of what changed.

## Manual Targeted Checks

### C build and test

```bash
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
./build/human_tests
```

### Targeted test suites

```bash
# Find which suites to run based on changed files
./build/human_tests $(scripts/what-to-test.sh)

# Or run a specific suite
./build/human_tests --suite=provider
./build/human_tests --filter=json_parse
```

### Format check

```bash
# Find clang-format
FMT=$(command -v clang-format 2>/dev/null || echo /opt/homebrew/opt/llvm/bin/clang-format)
$FMT --dry-run -Werror src/path/to/changed_file.c
```

### UI checks

```bash
cd ui && npm run check
```

## Verification Checklist

- [ ] Build succeeds with no warnings (`-Werror`)
- [ ] All relevant tests pass
- [ ] 0 ASan errors
- [ ] Changed C files are formatted (`clang-format`)
- [ ] No raw hex colors or hardcoded values in UI files
- [ ] One concern per commit
- [ ] Commit message follows conventional format
