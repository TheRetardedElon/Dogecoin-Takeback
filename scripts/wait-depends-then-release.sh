#!/usr/bin/env bash
# Wait for depends rebuild, then full PE release + installer.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
BUILD=/home/theretardedelon/dogedev-winbuild
HOST=x86_64-w64-mingw32
LOG=/tmp/depends-rebuild.log

log() { echo "[wait $(date +%H:%M:%S)] $*"; }

# Wait until config.site exists and make finished
for i in $(seq 1 500); do
  if [[ -f $BUILD/depends/$HOST/share/config.site ]]; then
    # ensure make process not still running
    if ! pgrep -f "make HOST=$HOST" >/dev/null 2>&1; then
      log "depends config.site ready and make idle"
      break
    fi
    log "config.site exists but make still running..."
  fi
  if ! pgrep -f "make HOST=$HOST" >/dev/null 2>&1; then
    # make died without config.site
    if [[ ! -f $BUILD/depends/$HOST/share/config.site ]]; then
      log "depends make exited without config.site — check $LOG"
      tail -50 "$LOG" || true
      exit 1
    fi
  fi
  if (( i % 5 == 0 )); then
    log "still waiting (cycle $i) last log:"
    tail -3 "$LOG" 2>/dev/null || true
    du -sh "$BUILD/depends/$HOST" 2>/dev/null || true
  fi
  sleep 60
done

test -f "$BUILD/depends/$HOST/share/config.site"
log "Starting full release build"
exec bash /mnt/c/dogedev/scripts/reconf-and-build-win-release.sh
