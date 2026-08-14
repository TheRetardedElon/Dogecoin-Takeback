#!/usr/bin/env bash
# Smoke-test dogecoin-pro-gui: launch briefly, confirm process stays up, capture stderr.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/dogecoin-pro-gui"
ASSETS="${ROOT}/build/assets"
LOG="${ROOT}/build/smoke-pro-gui.log"
SEC="${SMOKE_SEC:-8}"

echo "=== dogecoin-pro-gui smoke ==="
date -u +"UTC %Y-%m-%dT%H:%M:%SZ"

if [[ ! -x "$BIN" ]]; then
  echo "FAIL: missing binary $BIN"
  echo "Run: bash scripts/build-wsl.sh"
  exit 1
fi
if [[ ! -f "${ASSETS}/catalog.json" ]]; then
  echo "FAIL: missing assets at $ASSETS"
  exit 1
fi

echo "binary: $(ls -lh "$BIN" | awk '{print $5, $9}')"
echo "DISPLAY=${DISPLAY:-<unset>} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-<unset>}"

# Prefer system X11 libs at runtime if our extract prefix has shared objects
X11P="${HOME}/x11-prefix"
if [[ -d "${X11P}/usr/lib/x86_64-linux-gnu" ]]; then
  export LD_LIBRARY_PATH="${X11P}/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
fi

cd "${ROOT}/build"
rm -f "$LOG"
set +e
# Launch in background; kill after SEC
"$BIN" >"$LOG" 2>&1 &
PID=$!
echo "pid=$PID (will run ${SEC}s)"

# Wait a moment for crash-on-start
sleep 1
if ! kill -0 "$PID" 2>/dev/null; then
  echo "FAIL: process exited within 1s"
  echo "--- log ---"
  cat "$LOG" || true
  exit 1
fi
echo "OK: process alive after 1s"

# Optional: second check mid-run
sleep $((SEC > 2 ? SEC - 1 : 1))
if ! kill -0 "$PID" 2>/dev/null; then
  echo "FAIL: process died before ${SEC}s"
  echo "--- log ---"
  cat "$LOG" || true
  exit 1
fi
echo "OK: process still alive at ~${SEC}s"

kill "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true
set -e

echo "--- stderr/stdout (log) ---"
if [[ -s "$LOG" ]]; then
  cat "$LOG"
else
  echo "(empty — normal if no errors)"
fi

# Basic log sanity: assets path mentioned or no GLFW fatal
if grep -qiE 'GLFW error|Failed to|FATAL|segmentation' "$LOG" 2>/dev/null; then
  echo "FAIL: error patterns in log"
  exit 1
fi

echo "=== SMOKE_OK ==="
echo "You can also run interactively:"
echo "  cd ${ROOT}/build && ./dogecoin-pro-gui"
exit 0
