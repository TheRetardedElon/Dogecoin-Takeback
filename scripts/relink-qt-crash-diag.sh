#!/usr/bin/env bash
# Rebuild dogecoin-qt pieces involved in post-load GUI init crash (0xc0000374).
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"
mkdir -p qt

# Sync sources
for f in \
  qt/dogecoin.cpp \
  qt/dogecoingui.cpp qt/dogecoingui.h \
  qt/clientmodel.cpp qt/clientmodel.h \
  qt/peermapwidget.cpp qt/peermapwidget.h \
  qt/networkpage.cpp qt/networkpage.h
do
  cp -f "$SRC/src/$f" "$f"
done

MOC="$BUILD/depends/x86_64-w64-mingw32/native/bin/moc"
"$MOC" -o qt/moc_peermapwidget.cpp qt/peermapwidget.h
"$MOC" -o qt/moc_clientmodel.cpp qt/clientmodel.h
"$MOC" -o qt/moc_dogecoingui.cpp qt/dogecoingui.h
"$MOC" -o qt/moc_networkpage.cpp qt/networkpage.h

# Force recompile of critical objects
rm -f \
  qt/dogecoin_qt-dogecoin.o \
  qt/libdogecoinqt_a-dogecoingui.o \
  qt/libdogecoinqt_a-clientmodel.o \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o \
  qt/libdogecoinqt_a-networkpage.o \
  qt/libdogecoinqt_a-moc_networkpage.o \
  qt/libdogecoinqt_a-moc_clientmodel.o \
  qt/libdogecoinqt_a-moc_dogecoingui.o

# Build objects via make (captures correct flags)
make \
  qt/dogecoin_qt-dogecoin.o \
  qt/libdogecoinqt_a-dogecoingui.o \
  qt/libdogecoinqt_a-clientmodel.o \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o \
  qt/libdogecoinqt_a-networkpage.o \
  qt/libdogecoinqt_a-moc_networkpage.o \
  qt/libdogecoinqt_a-moc_clientmodel.o \
  qt/libdogecoinqt_a-moc_dogecoingui.o \
  V=1 2>&1 | tee /tmp/qt_crash_diag_build.log | tail -60

# Ensure moc peermap object exists (make may not know moc source if not in Makefile deps)
if [ ! -f qt/libdogecoinqt_a-moc_peermapwidget.o ]; then
  CMD=$(grep 'x86_64-w64-mingw32-g++' /tmp/qt_crash_diag_build.log | grep peermapwidget.cpp | head -1)
  if [ -n "$CMD" ]; then
    MOC_CMD=$(echo "$CMD" \
      | sed 's/libdogecoinqt_a-peermapwidget\.o/libdogecoinqt_a-moc_peermapwidget.o/g' \
      | sed 's/`[^`]*`//g' \
      | sed 's|qt/peermapwidget\.cpp|qt/moc_peermapwidget.cpp|g')
    echo "MOC_CMD=$MOC_CMD"
    eval "$MOC_CMD"
  fi
fi

ls -la qt/dogecoin_qt-dogecoin.o \
  qt/libdogecoinqt_a-dogecoingui.o \
  qt/libdogecoinqt_a-clientmodel.o \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o \
  qt/libdogecoinqt_a-networkpage.o

# Patch archive members (do not wipe whole .a)
x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-dogecoingui.o \
  qt/libdogecoinqt_a-clientmodel.o \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o \
  qt/libdogecoinqt_a-networkpage.o \
  qt/libdogecoinqt_a-moc_networkpage.o \
  qt/libdogecoinqt_a-moc_clientmodel.o \
  qt/libdogecoinqt_a-moc_dogecoingui.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

rm -f qt/dogecoin-qt.exe
make qt/dogecoin-qt.exe 2>&1 | tee /tmp/qt_crash_diag_link.log | tail -40

# If make AR wiped objects, re-insert and relink once
if [ ! -f qt/dogecoin-qt.exe ] || ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q refreshFromPeers; then
  echo "Re-insert peermap/gui objects and retry link..."
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-dogecoingui.o \
    qt/libdogecoinqt_a-clientmodel.o \
    qt/libdogecoinqt_a-peermapwidget.o \
    qt/libdogecoinqt_a-moc_peermapwidget.o \
    qt/libdogecoinqt_a-networkpage.o \
    qt/libdogecoinqt_a-moc_networkpage.o \
    qt/libdogecoinqt_a-moc_clientmodel.o \
    qt/libdogecoinqt_a-moc_dogecoingui.o
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe 2>&1 | tail -30
fi

ls -la qt/dogecoin-qt.exe
file qt/dogecoin-qt.exe
x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -E 'refreshFromPeers|setClientModel' | head -10
# Confirm new log strings landed
strings qt/dogecoin-qt.exe | grep -F 'GUI init step' | head -10

cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
echo QT_CRASH_DIAG_OK
