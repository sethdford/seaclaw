#!/usr/bin/env bash
set -euo pipefail

# Local real IMAP/SMTP validation via GreenMail Docker.
# Requires: docker, cmake toolchain, libcurl + sqlite dev libs installed.
#
# This script:
# 1) launches GreenMail with IMAP+SMTP
# 2) builds integration tests with HU_ENABLE_INTEGRATION_TESTS=ON
# 3) runs only the live IMAP test using env vars
#
# Usage:
#   bash scripts/integ-imap-local.sh
#
# For real Gmail (app password), use scripts/integ-imap-gmail.sh — see docs/guides/imap-gmail.md

if ! command -v docker >/dev/null 2>&1; then
  echo "docker is required for local IMAP validation"
  exit 1
fi

CONTAINER="human-greenmail-integ"
trap 'docker rm -f "$CONTAINER" >/dev/null 2>&1 || true' EXIT

docker rm -f "$CONTAINER" >/dev/null 2>&1 || true
docker run -d --name "$CONTAINER" \
  -p 3143:3143 -p 3025:3025 \
  -e GREENMAIL_OPTS="-Dgreenmail.setup.test.imap -Dgreenmail.setup.test.smtp -Dgreenmail.users=test:secret" \
  greenmail/standalone:2.1.1 >/dev/null

echo "Waiting for GreenMail..."
sleep 4

cmake -B build-integration \
  -DCMAKE_BUILD_TYPE=Debug \
  -DHU_ENABLE_INTEGRATION_TESTS=ON \
  -DHU_ENABLE_CURL=ON \
  -DHU_ENABLE_SQLITE=ON \
  -DHU_ENABLE_CLI=ON \
  -DHU_ENABLE_IMAP=ON >/dev/null
cmake --build build-integration -j >/dev/null

export HU_INTEG_IMAP=1
export HU_INTEG_IMAP_HOST=127.0.0.1
export HU_INTEG_IMAP_PORT=3143
export HU_INTEG_IMAP_USER=test
export HU_INTEG_IMAP_PASS=secret
export HU_INTEG_SMTP_HOST=127.0.0.1
export HU_INTEG_SMTP_PORT=3025
export HU_INTEG_IMAP_TLS=0
export HU_INTEG_IMAP_FOLDER=INBOX
export HU_INTEG_IMAP_TO=test
export HU_INTEG_IMAP_FROM=test

echo "Running live IMAP integration test..."
./build-integration/human_integration_tests --filter=integ_imap_send_and_poll_live
