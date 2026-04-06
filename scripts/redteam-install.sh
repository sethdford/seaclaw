#!/usr/bin/env bash
# redteam-install.sh — Documentation and installation red team checks.
# Validates that install paths, CLI flags, and docs are consistent.
# Run as part of release gate or manually: bash scripts/redteam-install.sh
set -euo pipefail

PASS=0
FAIL=0
WARN=0

pass() { PASS=$((PASS + 1)); printf "  \033[32m✓\033[0m %s\n" "$1"; }
fail() { FAIL=$((FAIL + 1)); printf "  \033[31m✗\033[0m %s\n" "$1"; }
warn() { WARN=$((WARN + 1)); printf "  \033[33m!\033[0m %s\n" "$1"; }
section() { printf "\n\033[1m%s\033[0m\n" "$1"; }

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# ── Section 1: Clone directory name in docs ──────────────────────────────
section "Clone directory consistency"

bad_cd=$(grep -rn 'cd human$\|cd human ' "$ROOT/docs" "$ROOT/website/src" "$ROOT/README.md" \
    --include='*.md' --include='*.mdx' 2>/dev/null \
    | grep -v 'cd h-uman' \
    | grep -v 'cd human_tests' \
    | grep -v 'cd human-' \
    | grep -v '# cd' \
    | grep -v 'human/workspace' || true)

if [ -z "$bad_cd" ]; then
    pass "No 'cd human' (should be 'cd h-uman') found in docs"
else
    fail "Found 'cd human' instead of 'cd h-uman':"
    echo "$bad_cd" | head -10
fi

# ── Section 2: CLI flags mentioned in docs exist in code ─────────────────
section "Onboard CLI flags"

for flag in "--apple" "--provider" "--api-key"; do
    if grep -q "$flag" "$ROOT/src/main.c" 2>/dev/null || grep -q "\"$flag\"" "$ROOT/src/main.c" 2>/dev/null; then
        pass "Flag $flag is implemented in main.c"
    else
        # Check for the flag pattern (e.g. strcmp(argv[i], "--apple"))
        flag_bare="${flag#--}"
        if grep -q "$flag_bare" "$ROOT/src/main.c" 2>/dev/null; then
            pass "Flag $flag is implemented in main.c"
        else
            fail "Flag $flag is referenced in docs but not found in main.c"
        fi
    fi
done

# Check --interactive is NOT promised in docs (removed)
interactive_refs=$(grep -rn '\-\-interactive' "$ROOT/README.md" "$ROOT/docs/guides/getting-started.md" \
    "$ROOT/website/src/content/docs/getting-started" 2>/dev/null || true)
if [ -z "$interactive_refs" ]; then
    pass "No --interactive references in primary install docs"
else
    warn "--interactive still referenced in docs (not implemented):"
    echo "$interactive_refs" | head -5
fi

# ── Section 3: Install methods vs release artifacts ──────────────────────
section "Install method consistency"

if grep -q 'install\.sh' "$ROOT/README.md" 2>/dev/null; then
    pass "install.sh referenced in README"
else
    fail "install.sh not mentioned in README"
fi

if [ -f "$ROOT/install.sh" ]; then
    pass "install.sh exists in repo root"
    if grep -q 'apfel\|Apple Intelligence' "$ROOT/install.sh" 2>/dev/null; then
        pass "install.sh includes Apple Intelligence setup"
    else
        fail "install.sh missing Apple Intelligence setup for macOS"
    fi
else
    fail "install.sh missing from repo root"
fi

# Check that install.sh handles both platforms from README
if grep -q 'linux' "$ROOT/install.sh" 2>/dev/null && grep -q 'macos' "$ROOT/install.sh" 2>/dev/null; then
    pass "install.sh handles both Linux and macOS"
else
    fail "install.sh missing platform support"
fi

# ── Section 4: Apple Intelligence documentation ──────────────────────────
section "Apple Intelligence documentation"

if [ -f "$ROOT/website/src/content/docs/providers/apple.mdx" ]; then
    pass "Apple Intelligence provider page exists"
else
    fail "Missing website/src/content/docs/providers/apple.mdx"
fi

if grep -q 'apple\|Apple Intelligence' "$ROOT/website/astro.config.mjs" 2>/dev/null; then
    pass "Apple Intelligence in website sidebar"
else
    fail "Apple Intelligence not in website sidebar config"
fi

apple_in_onboard=$(grep -c 'apple\|Apple' "$ROOT/src/onboard.c" 2>/dev/null || echo "0")
if [ "$apple_in_onboard" -gt 0 ]; then
    pass "Apple option present in onboard wizard"
else
    fail "Apple option missing from onboard wizard"
fi

# ── Section 5: Config defaults ───────────────────────────────────────────
section "Platform defaults"

if grep -q 'HU_ENABLE_APPLE_INTELLIGENCE' "$ROOT/src/config_merge.c" 2>/dev/null; then
    pass "config_merge.c has Apple platform defaults"
else
    fail "config_merge.c missing Apple platform defaults"
fi

if grep -q 'on_device_available' "$ROOT/src/daemon.c" 2>/dev/null; then
    pass "daemon.c probes on-device availability"
else
    fail "daemon.c not probing on-device availability"
fi

if grep -q 'on_device_model' "$ROOT/src/config_parse_agent.c" 2>/dev/null; then
    pass "config_parse_agent.c parses on-device fields"
else
    fail "config_parse_agent.c missing on-device field parsing"
fi

# ── Section 6: First-run experience ──────────────────────────────────────
section "First-run experience"

if grep -q 'hu_onboard_check_first_run' "$ROOT/src/main.c" 2>/dev/null; then
    pass "main.c has first-run detection nudge"
else
    fail "main.c missing first-run detection"
fi

if grep -q 'hu_onboard_run_with_args' "$ROOT/include/human/onboard.h" 2>/dev/null; then
    pass "onboard.h declares hu_onboard_run_with_args"
else
    fail "onboard.h missing hu_onboard_run_with_args declaration"
fi

# ── Section 7: Build flags ───────────────────────────────────────────────
section "Build configuration"

if grep -q 'if(APPLE)' "$ROOT/CMakeLists.txt" 2>/dev/null && \
   grep -A1 'if(APPLE)' "$ROOT/CMakeLists.txt" 2>/dev/null | grep -q 'APPLE_INTELLIGENCE.*ON'; then
    pass "CMakeLists.txt auto-enables Apple Intelligence on macOS"
else
    fail "CMakeLists.txt not auto-enabling Apple Intelligence on macOS"
fi

# ── Summary ──────────────────────────────────────────────────────────────
section "Summary"
printf "  Passed: %d  Failed: %d  Warnings: %d\n" "$PASS" "$FAIL" "$WARN"

if [ "$FAIL" -gt 0 ]; then
    printf "\n\033[31mInstall red team: %d failures found.\033[0m\n" "$FAIL"
    exit 1
fi

if [ "$WARN" -gt 0 ]; then
    printf "\n\033[33mInstall red team: passed with %d warnings.\033[0m\n" "$WARN"
else
    printf "\n\033[32mInstall red team: all checks passed.\033[0m\n"
fi
