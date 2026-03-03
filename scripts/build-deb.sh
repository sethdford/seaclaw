#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$ROOT_DIR"

# Ensure packaging/debian exists
if [ ! -d packaging/debian ]; then
    echo "Error: packaging/debian not found"
    exit 1
fi

# Create debian symlink in project root (dpkg-buildpackage expects it)
ln -sfn packaging/debian debian

# Build the package
dpkg-buildpackage -us -uc -b

# Clean up symlink
rm -f debian

echo "Build complete. .deb files are in parent directory."
ls -la ../*.deb 2>/dev/null || echo "No .deb files found"
