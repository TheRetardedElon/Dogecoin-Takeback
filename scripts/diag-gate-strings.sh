#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild/src
cd "$B"

echo "=== nm exact symbols ==="
for s in \
  '_ZN11DogecoinGUI14gotoArcadePageEv' \
  '_ZN11DogecoinGUI18gotoMemeStreamPageEv' \
  'gotoArcadePage' \
  'ArcadePage' \
  'MemeStreamPage' \
  'DogeBusinessPage'
do
  c=$(x86_64-w64-mingw32-nm qt/dogecoin-qt.exe 2>/dev/null | grep -F "$s" | wc -l)
  echo "count=$c for $s"
  x86_64-w64-mingw32-nm qt/dogecoin-qt.exe 2>/dev/null | grep -F "$s" | head -3
done

echo "=== lib nm ==="
x86_64-w64-mingw32-nm qt/libdogecoinqt.a 2>/dev/null | grep -F 'ArcadePage' | head -5

echo "=== utf16 Meme/Arcade/Business ==="
strings -el qt/dogecoin-qt.exe 2>/dev/null | grep -iE 'meme|arcade|business|stream' | head -40

echo "=== dogecoind assume ==="
strings dogecoind.exe 2>/dev/null | grep -iE 'assume|txoutset|utxo' | head -30
strings dogecoind.exe 2>/dev/null | grep -i load | head -20

echo "=== rpc table grep ==="
# demangle samples
x86_64-w64-mingw32-nm dogecoind.exe 2>/dev/null | grep -i assume | head -10
x86_64-w64-mingw32-nm dogecoind.exe 2>/dev/null | grep -i txoutset | head -10
x86_64-w64-mingw32-nm dogecoind.exe 2>/dev/null | grep -i utxo | head -15
