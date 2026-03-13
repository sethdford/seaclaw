#!/usr/bin/env bash
# Generates a module dependency graph from #include directives.
# Usage: scripts/gen-include-graph.sh [--json] [--mermaid]
#   --json     Output JSON adjacency list to stdout
#   --mermaid  Output Mermaid diagram to stdout (default)
set -euo pipefail

FORMAT="mermaid"
if [ "${1:-}" = "--json" ]; then
    FORMAT="json"
elif [ "${1:-}" = "--mermaid" ]; then
    FORMAT="mermaid"
fi

cd "$(git rev-parse --show-toplevel)"

MODULES="core agent providers channels tools memory security gateway runtime context persona feeds intelligence peripherals observability sse websocket"

EDGES=""

for mod in $MODULES; do
    dir="src/$mod"
    [ -d "$dir" ] || continue

    for dep in $MODULES; do
        [ "$dep" = "$mod" ] && continue
        if grep -rqh "\"human/${dep}/" "$dir" 2>/dev/null || \
           grep -rqh "\"human/${dep}\\.h\"" "$dir" 2>/dev/null; then
            EDGES="${EDGES}${mod} ${dep}\n"
        fi
    done
done

EDGES=$(printf '%b' "$EDGES" | sort -u)

if [ "$FORMAT" = "json" ]; then
    echo "{"
    first_mod=1
    for mod in $MODULES; do
        [ $first_mod -eq 0 ] && echo ","
        first_mod=0
        deps=$(echo "$EDGES" | awk -v m="$mod" '$1==m {printf "\"%s\",",$2}' | sed 's/,$//')
        printf '  "%s": [%s]' "$mod" "$deps"
    done
    echo ""
    echo "}"
elif [ "$FORMAT" = "mermaid" ]; then
    echo "graph LR"
    echo "$EDGES" | while read -r src dst; do
        [ -n "$src" ] && echo "  $src --> $dst"
    done
fi
