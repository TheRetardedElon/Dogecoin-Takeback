#!/usr/bin/env bash
# Print a crypto-random URL-safe-ish password (no shared defaults).
set -euo pipefail
if command -v openssl >/dev/null 2>&1; then
  openssl rand -base64 24 | tr -d '/+=' | head -c 32
  echo
  exit 0
fi
tr -dc 'A-Za-z0-9' </dev/urandom | head -c 28
echo
