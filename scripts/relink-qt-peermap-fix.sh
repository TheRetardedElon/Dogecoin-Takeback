#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"
mkdir -p qt
cp -f "$SRC/src/qt/peermapwidget.h" "$SRC/src/qt/peermapwidget.cpp" qt/

MOC="$BUILD/depends/x86_64-w64-mingw32/native/bin/moc"
"$MOC" -o qt/moc_peermapwidget.cpp qt/peermapwidget.h

# Capture a known-good compile line for peermapwidget
rm -f qt/libdogecoinqt_a-peermapwidget.o
make qt/libdogecoinqt_a-peermapwidget.o V=1 > /tmp/peermap_build.log 2>&1 || true
CMD=$(grep 'x86_64-w64-mingw32-g++' /tmp/peermap_build.log | grep peermapwidget.cpp | head -1)
if [ -z "$CMD" ]; then
  echo "Failed to capture compile command"; cat /tmp/peermap_build.log | tail -30; exit 1
fi
# Rewrite for moc source
# Strip backtick path expansion and point at moc source
MOC_CMD=$(echo "$CMD" \
  | sed 's/libdogecoinqt_a-peermapwidget\.o/libdogecoinqt_a-moc_peermapwidget.o/g' \
  | sed 's/`[^`]*`//g' \
  | sed 's|qt/peermapwidget\.cpp|qt/moc_peermapwidget.cpp|g')
# ensure trailing source path present once
if ! echo "$MOC_CMD" | grep -q 'moc_peermapwidget.cpp'; then
  MOC_CMD="$MOC_CMD qt/moc_peermapwidget.cpp"
fi
echo "MOC_CMD=$MOC_CMD"
eval "$MOC_CMD"
ls -la qt/libdogecoinqt_a-moc_peermapwidget.o qt/libdogecoinqt_a-peermapwidget.o

# Rebuild archive with arcade + peermap (do not use make AR which may wipe)
x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o \
  qt/libdogecoinqt_a-arcadepage.o \
  qt/libdogecoinqt_a-arcadegamewidget.o \
  qt/libdogecoinqt_a-moc_arcadepage.o \
  qt/libdogecoinqt_a-moc_arcadegamewidget.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -E 'ArcadePageC|refreshFromPeers' | head -8

rm -f qt/dogecoin-qt.exe
# Link using make; if AR rule runs and drops objects, re-insert after
make qt/dogecoin-qt.exe 2>&1 | tee /tmp/qt_link.log | tail -40
if [ ! -f qt/dogecoin-qt.exe ]; then
  # re-add objects and retry link once
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-peermapwidget.o \
    qt/libdogecoinqt_a-moc_peermapwidget.o \
    qt/libdogecoinqt_a-arcadepage.o \
    qt/libdogecoinqt_a-arcadegamewidget.o \
    qt/libdogecoinqt_a-moc_arcadepage.o \
    qt/libdogecoinqt_a-moc_arcadegamewidget.o
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
  make qt/dogecoin-qt.exe 2>&1 | tail -30
fi
ls -la qt/dogecoin-qt.exe
file qt/dogecoin-qt.exe
cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
echo QT_REBUILD_OK
