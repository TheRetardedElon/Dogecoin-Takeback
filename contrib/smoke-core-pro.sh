#!/usr/bin/env bash
# Smoke checks for Dogecoin Core Pro build (pure DOGE client).
# Usage (WSL): bash contrib/smoke-core-pro.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== binary =="
test -x src/qt/dogecoin-qt
test -x src/dogecoind || echo "(dogecoind optional for this smoke)"
ls -lh src/qt/dogecoin-qt

echo "== version string =="
# -version exits after printing
src/qt/dogecoin-qt -version 2>&1 | head -5 || true

echo "== meme feed API (network) =="
if curl -fsS --max-time 15 "https://gopastearth.com/api/public/memestream/feed?limit=2" | head -c 200 | grep -q '"items"'; then
  echo "feed OK"
else
  echo "feed unreachable or unexpected (non-fatal for offline smoke)"
fi

echo "== media sample =="
if curl -fsSI --max-time 15 "https://gopastearth.com/media/memestream/-iolmTBEnbRh7ASQ.png" | head -1 | grep -q 200; then
  echo "media OK"
else
  echo "media check skipped/fail (non-fatal)"
fi

echo "== smoke complete =="
echo "Launch GUI: ./src/qt/dogecoin-qt"
echo "Check: Home quick bar, Network peers/bans, Meme Stream images, Business invoices"
