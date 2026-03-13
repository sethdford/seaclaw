#!/usr/bin/env bash
# Check for blacklisted terminology in code and docs.
# Source of truth: docs/standards/brand/terminology.md
#
# Usage: ./scripts/check-terminology.sh [--fix]
#   --fix: suggest corrections (does not auto-fix)
#
# Scans: src/**/*.c, include/**/*.h, docs/**/*.md, ui/src/**/*.ts
# Skips: vendor/, build/, node_modules/, *.min.*, CHANGELOG, brand/terminology.md itself
set -euo pipefail

EXIT_CODE=0
VIOLATIONS=0

# Directories to scan
SCAN_PATHS=(
    "src"
    "include"
    "docs"
    "ui/src"
)

# File extensions to check
EXTENSIONS=("*.c" "*.h" "*.md" "*.ts")

# Files to skip (patterns)
SKIP_PATTERNS=(
    "*/node_modules/*"
    "*/build/*"
    "*/vendor/*"
    "*.min.*"
    "*/CHANGELOG*"
    "*/terminology.md"
    "*/design-tokens-reference.json"
)

# Build find exclusions
FIND_EXCLUDES=""
for pat in "${SKIP_PATTERNS[@]}"; do
    FIND_EXCLUDES="$FIND_EXCLUDES ! -path '$pat'"
done

# Blacklisted terms: "pattern" -> "preferred term"
# These are checked as whole words (word boundaries) to reduce false positives.
# Format: REGEX|PREFERRED
RULES=(
    # Structural terms -- check in docs and comments
    '\bknowledge base\b|memory (not "knowledge base")'
    '\bbump allocator\b|arena (not "bump allocator")'
    '\bthe connector\b|channel (not "connector")'
    '\bthe integration\b|channel (not "integration" when referring to a messaging transport)'
)

# More targeted rules for .md and .ts files (user-facing content)
DOC_RULES=(
    '\bIoT\b|peripheral (not "IoT")'
    '\bthe addon\b|plugin (not "addon")'
    '\bthe jail\b|sandbox (not "jail")'
)

echo "Checking terminology compliance..."
echo ""

check_pattern() {
    local pattern="$1"
    local message="$2"
    local paths=("${@:3}")

    for scan_path in "${paths[@]}"; do
        if [ ! -d "$scan_path" ]; then
            continue
        fi

        # Use rg (ripgrep) for speed, fall back to grep
        if command -v rg &>/dev/null; then
            matches=$(rg -i -n --no-heading \
                --glob '*.c' --glob '*.h' --glob '*.md' --glob '*.ts' \
                --glob '!**/node_modules/**' --glob '!**/build/**' \
                --glob '!**/vendor/**' --glob '!**/*.min.*' \
                --glob '!**/CHANGELOG*' \
                --glob '!**/terminology.md' \
                --glob '!**/plans/**' \
                --glob '!**/design-tokens-reference.json' \
                --glob '!**/brand/**' \
                "$pattern" "$scan_path" 2>/dev/null || true)
        else
            matches=$(grep -rni --include='*.c' --include='*.h' --include='*.md' --include='*.ts' \
                --exclude-dir=node_modules --exclude-dir=build --exclude-dir=vendor \
                -E "$pattern" "$scan_path" 2>/dev/null || true)
        fi

        if [ -n "$matches" ]; then
            while IFS= read -r line; do
                # Skip self-references in the terminology definition doc and plan docs
                case "$line" in
                    *terminology.md*|*plans/*) continue ;;
                esac
                echo "  TERM: $line"
                echo "        -> $message"
                VIOLATIONS=$((VIOLATIONS + 1))
            done <<< "$matches"
        fi
    done
}

# Run general rules
for rule in "${RULES[@]}"; do
    pattern="${rule%%|*}"
    message="${rule#*|}"
    check_pattern "$pattern" "$message" "${SCAN_PATHS[@]}"
done

# Run doc-specific rules (only on docs and UI)
for rule in "${DOC_RULES[@]}"; do
    pattern="${rule%%|*}"
    message="${rule#*|}"
    check_pattern "$pattern" "$message" "docs" "ui/src"
done

echo ""
if [ "$VIOLATIONS" -gt 0 ]; then
    echo "Found $VIOLATIONS terminology violation(s)."
    echo "See docs/standards/brand/terminology.md for canonical terms."
    # Don't fail the build yet -- this is advisory until terms stabilize
    # EXIT_CODE=1
else
    echo "  No terminology violations found."
fi

exit $EXIT_CODE
