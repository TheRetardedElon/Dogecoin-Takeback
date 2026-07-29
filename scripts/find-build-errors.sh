#!/usr/bin/env bash
export PATH=/usr/bin:/bin
LOG="${1:-/tmp/win-core-build4.log}"
echo "=== errors ==="
grep -n "error:" "$LOG" | grep -vi deprecated | tail -40
echo "=== make errors ==="
grep -n "Error " "$LOG" | tail -20
echo "=== last 50 lines ==="
tail -50 "$LOG"
