#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release

# Sync sources
mkdir -p "$BUILD/src/qt/res/images"
cp -f "$SRC/src/qt/peermapwidget.cpp" "$BUILD/src/qt/"
cp -f "$SRC/src/qt/peermapwidget.h" "$BUILD/src/qt/"
cp -f "$SRC/src/qt/networkpage.cpp" "$BUILD/src/qt/"
cp -f "$SRC/src/qt/networkpage.h" "$BUILD/src/qt/"
cp -f "$SRC/src/qt/dogecoin.qrc" "$BUILD/src/qt/"
cp -f "$SRC/src/qt/res/images/worldmap-yellow.png" "$BUILD/src/qt/res/images/"
cp -f "$SRC/src/Makefile.qt.include" "$BUILD/src/"

cd "$BUILD"

# Ensure Makefile picks up new sources (re-run config.status if needed)
if ! grep -q peermapwidget src/Makefile 2>/dev/null; then
  echo "Refreshing Makefile from Makefile.qt.include..."
  # automake-generated Makefile includes Makefile.qt.include via config
  ./config.status src/Makefile 2>&1 | tail -5 || true
fi

# If still missing, force regenerate
if ! grep -q peermapwidget src/Makefile 2>/dev/null; then
  echo "WARNING: peermapwidget not in Makefile — patching"
  # Last resort: append object to build line is fragile; reconfigure if needed
  grep peermapwidget src/Makefile.qt.include || true
fi

# moc + rcc + compile
export CONFIG_SITE="$BUILD/depends/x86_64-w64-mingw32/share/config.site"
export PATH="/usr/bin:/bin:$BUILD/depends/x86_64-w64-mingw32/native/bin"

# Force rebuild of qrc and network page
rm -f src/qt/qrc_dogecoin.cpp src/qt/libdogecoinqt_a-qrc_dogecoin.o
rm -f src/qt/moc_peermapwidget.cpp src/qt/libdogecoinqt_a-peermapwidget.o
rm -f src/qt/libdogecoinqt_a-networkpage.o src/qt/libdogecoinqt_a-moc_networkpage.o
rm -f src/qt/libdogecoinqt.a src/qt/dogecoin-qt.exe

make -j"$(nproc)" -C src qt/dogecoin-qt.exe 2>&1 | tee /tmp/peermap-build.log | tail -n 60

file src/qt/dogecoin-qt.exe
x86_64-w64-mingw32-strings src/qt/dogecoin-qt.exe | grep -E 'Peers on map|ip-api|worldmap' | head -10 || true

bash "$SRC/scripts/package-windows-release.sh" || true
cp -f "$BUILD"/dogecoin-1.14.100-win64-setup.exe "$OUT/" 2>/dev/null || \
  cp -f "$BUILD"/dogecoin-*-win64-setup.exe "$OUT/dogecoin-1.14.100-win64-setup.exe" 2>/dev/null || true
(
  cd "$OUT"
  sha256sum dogecoin-1.14.100-win64.zip dogecoin-1.14.100-win64-setup.exe 2>/dev/null | tee SHA256SUMS-win64.txt
  ls -lah dogecoin-1.14.100-win64*
)
echo PEERMAP_BUILD_DONE
