#!/usr/bin/env bash
# Component quality audit against the 14-point checklist.
# Checks each component for token usage, state handling, accessibility, and motion.
# Usage: scripts/audit-component-quality.sh [--verbose]
set -euo pipefail

cd "$(dirname "$0")/.."

VERBOSE=false
if [ "${1:-}" = "--verbose" ]; then VERBOSE=true; fi

COMP_DIR="ui/src/components"
VIEW_DIR="ui/src/views"

total_issues=0
total_files=0
declare -a summary_lines

audit_file() {
  local file="$1"
  local name
  name=$(basename "$file" .ts)
  local issues=0
  local notes=""

  total_files=$((total_files + 1))

  # Skip test files
  if echo "$name" | grep -q '\.test$'; then return; fi

  local content
  content=$(cat "$file")

  # 1. Raw hex colors
  local hex_count
  hex_count=$(echo "$content" | grep -cE '#[0-9a-fA-F]{3,8}' 2>/dev/null || true)
  hex_count=${hex_count:-0}
  hex_count=$(echo "$hex_count" | tr -d '[:space:]')
  if [ "$hex_count" -gt 0 ] 2>/dev/null; then
    issues=$((issues + 1))
    notes="${notes} raw-hex($hex_count)"
  fi

  # 2. Raw px values (more than 5 is suspicious)
  local px_count
  px_count=$(echo "$content" | grep -cE ':\s*\d+px' 2>/dev/null || echo 0)
  if [ "$px_count" -gt 5 ]; then
    issues=$((issues + 1))
    notes="${notes} raw-px($px_count)"
  fi

  # 3. Missing dark/light awareness (no prefers-color-scheme or data-theme)
  if ! echo "$content" | grep -qE 'prefers-color-scheme|data-theme|\-\-hu\-' 2>/dev/null; then
    issues=$((issues + 1))
    notes="${notes} no-tokens"
  fi

  # 4. Missing loading/empty/error states (for views)
  if echo "$file" | grep -q 'views/'; then
    if ! echo "$content" | grep -qE 'hu-skeleton|loading|isLoading' 2>/dev/null; then
      issues=$((issues + 1))
      notes="${notes} no-loading-state"
    fi
    if ! echo "$content" | grep -qE 'hu-empty-state|empty' 2>/dev/null; then
      issues=$((issues + 1))
      notes="${notes} no-empty-state"
    fi
    if ! echo "$content" | grep -qE 'error|Error|hu-toast' 2>/dev/null; then
      issues=$((issues + 1))
      notes="${notes} no-error-state"
    fi
  fi

  # 5. Missing ARIA (interactive components)
  if echo "$content" | grep -qE '@click|click\(' 2>/dev/null; then
    if ! echo "$content" | grep -qE 'role=|aria-|tabindex' 2>/dev/null; then
      issues=$((issues + 1))
      notes="${notes} missing-aria"
    fi
  fi

  # 6. Missing keyboard support (interactive components)
  if echo "$content" | grep -qE '@click|click\(' 2>/dev/null; then
    if ! echo "$content" | grep -qE '@keydown|@keyup|keydown|keyup|Enter|Escape' 2>/dev/null; then
      issues=$((issues + 1))
      notes="${notes} no-keyboard"
    fi
  fi

  # 7. Missing reduced-motion support (animated components)
  if echo "$content" | grep -qE 'animation|transition|@keyframes|spring' 2>/dev/null; then
    if ! echo "$content" | grep -qE 'prefers-reduced-motion|reducedMotion' 2>/dev/null; then
      issues=$((issues + 1))
      notes="${notes} no-reduced-motion"
    fi
  fi

  # 8. rgba() usage (should use color-mix)
  local rgba_count
  rgba_count=$(echo "$content" | grep -cE 'rgba?\(' 2>/dev/null || echo 0)
  if [ "$rgba_count" -gt 0 ]; then
    issues=$((issues + 1))
    notes="${notes} rgba($rgba_count)"
  fi

  # 9. Glass usage check (overlays should use glass)
  if echo "$name" | grep -qE 'modal|sheet|dialog|dropdown|popover|tooltip|menu|palette'; then
    if ! echo "$content" | grep -qE 'glass|backdrop-filter' 2>/dev/null; then
      notes="${notes} could-use-glass"
    fi
  fi

  # 10. Tonal surface check (cards/panels should use tonal)
  if echo "$name" | grep -qE 'card|panel|sidebar'; then
    if ! echo "$content" | grep -qE 'surface-container' 2>/dev/null; then
      notes="${notes} could-use-tonal"
    fi
  fi

  total_issues=$((total_issues + issues))

  local grade="A"
  if [ "$issues" -ge 4 ]; then grade="D"
  elif [ "$issues" -ge 3 ]; then grade="C"
  elif [ "$issues" -ge 2 ]; then grade="B"
  elif [ "$issues" -ge 1 ]; then grade="B+"
  fi

  if [ "$VERBOSE" = true ] || [ "$issues" -gt 0 ]; then
    printf "%-40s %s  (%d issues)%s\n" "$name" "$grade" "$issues" "$notes"
  fi

  summary_lines+=("$grade|$name|$issues|$notes")
}

echo "=== Component Quality Audit ==="
echo "Checking against: tokens, states, accessibility, motion, surfaces"
echo ""

echo "--- Components ($COMP_DIR) ---"
for f in "$COMP_DIR"/*.ts; do
  audit_file "$f"
done

echo ""
echo "--- Views ($VIEW_DIR) ---"
for f in "$VIEW_DIR"/*.ts; do
  audit_file "$f"
done

echo ""
echo "=== Summary ==="

# Count grades
a_count=0; b_count=0; c_count=0; d_count=0
for line in "${summary_lines[@]}"; do
  grade=$(echo "$line" | cut -d'|' -f1)
  case "$grade" in
    A) a_count=$((a_count + 1)) ;;
    B|B+) b_count=$((b_count + 1)) ;;
    C) c_count=$((c_count + 1)) ;;
    D) d_count=$((d_count + 1)) ;;
  esac
done

echo "Files audited: $total_files"
echo "Total issues: $total_issues"
echo "Grades: A=$a_count  B=$b_count  C=$c_count  D=$d_count"
echo ""

if [ "$d_count" -gt 0 ]; then
  echo "Priority fixes (grade D):"
  for line in "${summary_lines[@]}"; do
    grade=$(echo "$line" | cut -d'|' -f1)
    if [ "$grade" = "D" ]; then
      name=$(echo "$line" | cut -d'|' -f2)
      notes=$(echo "$line" | cut -d'|' -f4)
      echo "  $name:$notes"
    fi
  done
fi
