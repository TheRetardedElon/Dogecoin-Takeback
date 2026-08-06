#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
SRC=/home/theretardedelon/dogedev-winbuild/depends/sources
BUILD=/home/theretardedelon/dogedev-winbuild
HOST=x86_64-w64-mingw32

# Kill waiters carefully by exact script path in pgrep, kill PIDs only
for pid in $(pgrep -f '/mnt/c/dogedev/scripts/wait-depends-then-release.sh' || true); do
  kill "$pid" 2>/dev/null || true
done
for pid in $(pgrep -f '/mnt/c/dogedev/scripts/reconf-and-build-win-release.sh' || true); do
  kill "$pid" 2>/dev/null || true
done
# kill stale make HOST if hung after error
for pid in $(pgrep -f "make HOST=${HOST}" || true); do
  kill "$pid" 2>/dev/null || true
done
sleep 1

# Remove empty/corrupt stamp that triggers source deletion
rm -f "$SRC/download-stamps/.stamp_fetched-zlib-zlib-1.3.tar.gz.hash"

# Restore zlib if missing
if [[ ! -f "$SRC/zlib-1.3.tar.gz" ]]; then
  echo "Downloading zlib-1.3.tar.gz"
  curl -fL --retry 3 -o "$SRC/zlib-1.3.tar.gz" \
    "https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz"
fi
echo "ff0ba4c292013dbc27530b3a81e1f9a813cd39de01ca5e0f8bf355702efa593e  $SRC/zlib-1.3.tar.gz" | sha256sum -c -

# Pre-create a valid stamp so check-sources won't delete it
# Stamp format is output of sha256sum for each source file
{
  sha256sum "$SRC/zlib-1.3.tar.gz"
} > "$SRC/download-stamps/.stamp_fetched-zlib-zlib-1.3.tar.gz.hash"
echo "stamp ready:"
cat "$SRC/download-stamps/.stamp_fetched-zlib-zlib-1.3.tar.gz.hash"

# Ensure other prefetched sources still present
ls -lah "$SRC"/*.tar.gz "$SRC"/*.tar.bz2 2>/dev/null | awk '{print $5,$9}'

echo "Starting depends make..."
cd "$BUILD/depends"
nohup make HOST=$HOST -j2 > /tmp/depends-rebuild.log 2>&1 &
echo "DEPENDS_PID=$!"
sleep 4
echo "=== depends log ==="
tail -40 /tmp/depends-rebuild.log

echo "Starting waiter..."
nohup env JOBS=6 bash /mnt/c/dogedev/scripts/wait-depends-then-release.sh \
  > /tmp/full-release.log 2>&1 &
echo "WAIT_PID=$!"
sleep 1
pgrep -af "make HOST=${HOST}|wait-depends-then-release" || true
echo RESTART_OK
