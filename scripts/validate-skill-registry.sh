#!/usr/bin/env bash
# Validate skill-registry/registry.json against on-disk skills/ trees.
# Mirrors skill-registry/.github/workflows/validate.yml (Python inline checks).
# Usage: from repo root — bash scripts/validate-skill-registry.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
REG_DIR="$ROOT/skill-registry"

if [[ ! -f "$REG_DIR/registry.json" ]]; then
  echo "error: missing $REG_DIR/registry.json" >&2
  exit 1
fi

cd "$REG_DIR"

python3 -c "import json; json.load(open('registry.json'))"

python3 -c "
import json
import os
with open('registry.json') as f:
    registry = json.load(f)
for entry in registry:
    name = entry.get('name')
    if not name:
        raise SystemExit('registry entry missing name')
    for field in ['description', 'version', 'author', 'url']:
        if not entry.get(field):
            raise SystemExit('registry entry %r missing %r' % (name, field))
    skill_json = os.path.join('skills', name, name + '.skill.json')
    if not os.path.isfile(skill_json):
        raise SystemExit('missing %s' % skill_json)
    with open(skill_json) as f:
        meta = json.load(f)
    if not meta.get('name') or not meta.get('description'):
        raise SystemExit('invalid skill json: %s' % skill_json)
print('skill-registry OK (%d entries)' % len(registry))
"
