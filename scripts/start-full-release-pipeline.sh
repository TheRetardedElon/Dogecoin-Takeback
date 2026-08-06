#!/usr/bin/env bash
# Kick depends rebuild (if needed) + wait-then-full-release.
set -uo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
BUILD=/home/theretardedelon/dogedev-winbuild
HOST=x86_64-w64-mingw32

# Stop any prior waiters (not depends if healthy)
pkill -f wait-depends-then-release 2>/dev/null || true
pkill -f reconf-and-build-win-release 2>/dev/null || true
sleep 1

if [[ -f "$BUILD/depends/$HOST/share/config.site" ]]; then
  echo "depends already complete — starting release build only"
  nohup env JOBS=6 bash /mnt/c/dogedev/scripts/reconf-and-build-win-release.sh \
    > /tmp/full-release.log 2>&1 &
  echo "RELEASE_PID=$!"
  exit 0
fi

if pgrep -f "make HOST=$HOST" >/dev/null 2>&1; then
  echo "depends make already running"
else
  echo "starting depends rebuild"
  cd "$BUILD/depends"
  nohup make HOST=$HOST -j2 > /tmp/depends-rebuild.log 2>&1 &
  echo "DEPENDS_PID=$!"
  sleep 2
fi

echo "starting waiter"
nohup env JOBS=6 bash /mnt/c/dogedev/scripts/wait-depends-then-release.sh \
  > /tmp/full-release.log 2>&1 &
echo "WAIT_PID=$!"
sleep 2
echo "--- processes ---"
pgrep -af "make HOST=$HOST|wait-depends|reconf-and-build" || true
echo "--- depends log tail ---"
tail -15 /tmp/depends-rebuild.log 2>/dev/null || true
echo PIPELINE_STARTED
