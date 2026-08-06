#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/bin:/bin
B=/home/theretardedelon/dogedev-winbuild/src

echo "=== file ==="
file "$B/qt/dogecoin-qt.exe"
ls -la "$B/qt/dogecoin-qt.exe"
echo "=== strings count ==="
strings "$B/qt/dogecoin-qt.exe" | wc -l
echo "=== sample dogecoin strings ==="
strings "$B/qt/dogecoin-qt.exe" | grep -i dogecoin | head -10
echo "=== utf16 search ==="
# Qt sometimes stores as UTF-16LE
strings -el "$B/qt/dogecoin-qt.exe" 2>/dev/null | grep -E 'Meme Stream|Doge Business|Arcade|Network' | head -20
echo "=== rg binary ==="
rg -a -F 'Meme Stream' "$B/qt/dogecoin-qt.exe" && echo found_meme || echo no_meme
rg -a -F 'Doge Business' "$B/qt/dogecoin-qt.exe" && echo found_biz || echo no_biz
rg -a -F 'gotoArcadePage' "$B/qt/dogecoin-qt.exe" && echo found_goto || echo no_goto
rg -a -F 'assumeutxo' "$B/qt/dogecoind.exe" && echo found_au_d || echo no_au_d
rg -a -F 'assumeutxo' "$B/qt/dogecoin-qt.exe" && echo found_au_qt || echo no_au_qt
echo "=== nm exe for gotoArcade ==="
x86_64-w64-mingw32-nm "$B/qt/dogecoin-qt.exe" 2>/dev/null | grep -i Arcade | head -10 || true
x86_64-w64-mingw32-nm "$B/qt/dogecoin-qt.exe" 2>/dev/null | grep -i Meme | head -10 || true
x86_64-w64-mingw32-objdump -t "$B/qt/dogecoin-qt.exe" 2>/dev/null | grep -i Arcade | head -10 || true
echo "=== is dogecoingui.o in archive table of contents ==="
x86_64-w64-mingw32-ar t "$B/qt/libdogecoinqt.a" | grep -E 'arcade|meme|business|peermap|dogecoingui' | sort
echo "=== link line from build log ==="
grep -E 'dogecoin-qt|OBJCXXLD|libdogecoinqt' /tmp/win-full-build.log 2>/dev/null | tail -30
echo "=== walletframe has pro methods? ==="
rg -n 'gotoArcade|gotoMeme|gotoDogeBusiness' "$B/qt/walletframe.cpp" "$B/qt/walletview.cpp" 2>/dev/null | head
rg -n 'gotoArcade|gotoMeme|gotoDogeBusiness' /mnt/c/dogedev/src/qt/walletframe.cpp /mnt/c/dogedev/src/qt/walletview.cpp 2>/dev/null | head
