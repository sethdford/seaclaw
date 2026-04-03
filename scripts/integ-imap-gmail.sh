#!/usr/bin/env bash
set -euo pipefail

# Run human_integration_tests live IMAP/SMTP check against Gmail.
#
# Google does not allow normal account passwords for IMAP/SMTP when 2FA is on.
# Create an App Password: Google Account → Security → 2-Step Verification → App passwords.
#
# Required environment:
#   HU_INTEG_GMAIL_USER       full address, e.g. you@gmail.com
#   HU_INTEG_GMAIL_APP_PASS   16-character app password (spaces optional)
#
# Optional:
#   HU_INTEG_IMAP_TO          recipient (default: same as HU_INTEG_GMAIL_USER)
#
# Usage:
#   export HU_INTEG_GMAIL_USER='you@gmail.com'
#   export HU_INTEG_GMAIL_APP_PASS='xxxx xxxx xxxx xxxx'
#   bash scripts/integ-imap-gmail.sh
#
# Does not read .env or print secrets.

if [[ -z "${HU_INTEG_GMAIL_USER:-}" || -z "${HU_INTEG_GMAIL_APP_PASS:-}" ]]; then
  echo "Set HU_INTEG_GMAIL_USER and HU_INTEG_GMAIL_APP_PASS (Gmail app password)." >&2
  exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

cmake -B build-integration \
  -DCMAKE_BUILD_TYPE=Debug \
  -DHU_ENABLE_INTEGRATION_TESTS=ON \
  -DHU_ENABLE_CURL=ON \
  -DHU_ENABLE_SQLITE=ON \
  -DHU_ENABLE_CLI=ON \
  -DHU_ENABLE_IMAP=ON >/dev/null
cmake --build build-integration -j >/dev/null

export HU_INTEG_IMAP=1
export HU_INTEG_IMAP_HOST=imap.gmail.com
export HU_INTEG_IMAP_PORT=993
export HU_INTEG_IMAP_USER="$HU_INTEG_GMAIL_USER"
export HU_INTEG_IMAP_PASS="$HU_INTEG_GMAIL_APP_PASS"
export HU_INTEG_SMTP_HOST=smtp.gmail.com
export HU_INTEG_SMTP_PORT=587
export HU_INTEG_IMAP_TLS=1
export HU_INTEG_IMAP_FOLDER=INBOX
export HU_INTEG_IMAP_FROM="$HU_INTEG_GMAIL_USER"
export HU_INTEG_IMAP_TO="${HU_INTEG_IMAP_TO:-$HU_INTEG_GMAIL_USER}"

echo "Running Gmail IMAP integration test (sends a short test message to HU_INTEG_IMAP_TO)..."
exec ./build-integration/human_integration_tests --filter=integ_imap_send_and_poll_live
