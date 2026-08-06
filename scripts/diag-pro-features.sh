#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/bin:/bin
B=/home/theretardedelon/dogedev-winbuild
S=/mnt/c/dogedev

echo "=== winbuild pro sources ==="
for f in \
  src/qt/arcadepage.cpp src/qt/arcadegamewidget.cpp \
  src/qt/memestreampage.cpp src/qt/memestreamclient.cpp \
  src/qt/dogebusinesspage.cpp src/qt/peermapwidget.cpp \
  src/qt/dogecoingui.cpp src/node/utxo_snapshot.cpp
do
  if [[ -f "$B/$f" ]]; then
    echo " OK $f ($(wc -c < "$B/$f") bytes)"
  else
    echo " MISSING $f"
  fi
done

echo "=== Makefile.am / Makefile.qt.include mentions ==="
for f in src/Makefile.am src/Makefile.qt.include src/qt/Makefile.am.include; do
  if [[ -f "$B/$f" ]]; then
    echo "-- $f --"
    grep -E 'arcade|memestream|dogebusiness|peermap|utxo_snapshot' "$B/$f" | head -20 || echo "(none)"
  else
    echo "no $f"
  fi
done

echo "=== generated src/Makefile mentions (count) ==="
if [[ -f "$B/src/Makefile" ]]; then
  for s in arcadepage memestreampage dogebusinesspage peermapwidget utxo_snapshot; do
    c=$(grep -c "$s" "$B/src/Makefile" || true)
    echo "  $s: $c"
  done
else
  echo "no src/Makefile"
fi

echo "=== object files ==="
ls -la "$B"/src/qt/*arcade* "$B"/src/qt/*meme* "$B"/src/qt/*business* "$B"/src/qt/*peermap* 2>&1 | head -50
ls -la "$B"/src/node/*utxo* 2>&1 | head -20

echo "=== nm libdogecoinqt.a pro symbols ==="
if [[ -f "$B/src/qt/libdogecoinqt.a" ]]; then
  x86_64-w64-mingw32-nm "$B/src/qt/libdogecoinqt.a" 2>/dev/null | grep -iE 'Arcade|MemeStream|DogeBusiness|PeerMap' | head -30
else
  echo "no libdogecoinqt.a"
fi

echo "=== strings on dogecoin-qt.exe ==="
if [[ -f "$B/src/qt/dogecoin-qt.exe" ]]; then
  for s in "Meme Stream" "Doge Business" "gotoArcadePage" "gotoMemeStreamPage" "assumeutxo" "Arcade" "Network"; do
    if strings "$B/src/qt/dogecoin-qt.exe" | grep -Fq "$s"; then
      echo " OK $s"
    else
      echo " FAIL $s"
    fi
  done
fi

echo "=== dogecoingui.cpp has pro menus? ==="
grep -nE 'Meme Stream|Arcade|Doge Business|gotoArcade|gotoMeme' "$B/src/qt/dogecoingui.cpp" 2>/dev/null | head -20
grep -nE 'Meme Stream|Arcade|Doge Business|gotoArcade|gotoMeme' "$S/src/qt/dogecoingui.cpp" 2>/dev/null | head -20

echo "=== compare Makefile.am arcade lines ==="
echo "SRC:"
grep -E 'arcade|peermap' "$S/src/Makefile.qt.include" 2>/dev/null | head -20
echo "WIN:"
grep -E 'arcade|peermap' "$B/src/Makefile.qt.include" 2>/dev/null | head -20
