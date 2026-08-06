#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/bin:/bin
date
echo "=== processes ==="
pgrep -af 'make HOST=x86_64' || echo "no depends make"
pgrep -af 'wait-depends-then-release' || echo "no waiter"
pgrep -af 'reconf-and-build-win-release' || echo "no release build"
echo "=== config.site ==="
if [[ -f /home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/config.site ]]; then
  echo HAS_CONFIG_SITE
else
  echo NO_CONFIG_SITE
fi
du -sh /home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32 2>/dev/null || true
echo "=== work packages ==="
ls /home/theretardedelon/dogedev-winbuild/depends/work/build/x86_64-w64-mingw32/ 2>/dev/null || true
echo "=== built archives ==="
find /home/theretardedelon/dogedev-winbuild/depends/built/x86_64-w64-mingw32 -name '*.tar.gz' -printf '%f\n' 2>/dev/null || true
echo "=== depends log tail ==="
tail -10 /tmp/depends-rebuild.log 2>/dev/null || true
echo "=== full-release log tail ==="
tail -15 /tmp/full-release.log 2>/dev/null || true
echo "=== winbuild PE ==="
ls -la /home/theretardedelon/dogedev-winbuild/src/qt/dogecoin-qt.exe 2>/dev/null || echo "no qt exe yet"
ls -la /home/theretardedelon/dogedev-winbuild/release/*.exe 2>/dev/null || echo "no staged release yet"
echo "=== C release dir ==="
ls -la /mnt/c/dogedev/release/dogecoin-1.14.101* 2>/dev/null || true
grep -l FULL_RELEASE /mnt/c/dogedev/release/* 2>/dev/null || true
cat /home/theretardedelon/dogedev-winbuild/release/BUILD_STAMP.txt 2>/dev/null || true
