#!/usr/bin/env bash
set -euo pipefail
QT=${1:-/home/theretardedelon/dogedev-winbuild/src/qt/dogecoin-qt.exe}
R=${2:-/home/theretardedelon/dogedev-winbuild/release/dogecoin-qt.exe}
echo "=== sizes ==="
ls -la "$QT" "$R" 2>/dev/null || true
echo "=== unstripped ==="
for s in "Meme Stream" "Doge Business" "Arcade" "gotoArcadePage" "MemeStreamPage" "PeerMap" "assumeutxo"; do
  if strings "$QT" 2>/dev/null | grep -Fq "$s"; then echo "OK  $s"; else echo "MISS $s"; fi
done
echo "=== stripped release ==="
for s in "Meme Stream" "Doge Business" "Arcade" "gotoArcadePage" "MemeStreamPage"; do
  if strings "$R" 2>/dev/null | grep -Fq "$s"; then echo "OK  $s"; else echo "MISS $s"; fi
done
echo "=== archive symbols ==="
A=/home/theretardedelon/dogedev-winbuild/src/qt/libdogecoinqt.a
x86_64-w64-mingw32-nm "$A" 2>/dev/null | grep -E 'ArcadePageC1|MemeStreamPageC1|DogeBusinessPageC1|PeerMapWidgetC1' | head -20
echo "=== sample arcade strings ==="
strings "$QT" 2>/dev/null | grep -i arcade | head -15
echo "=== sample meme strings ==="
strings "$QT" 2>/dev/null | grep -i meme | head -15
