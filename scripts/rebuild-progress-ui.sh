#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"
cp -f "$SRC/src/qt/modaloverlay.cpp" "$SRC/src/qt/modaloverlay.h" "$SRC/src/qt/dogecoingui.cpp" qt/
make qt/libdogecoinqt_a-modaloverlay.o qt/libdogecoinqt_a-dogecoingui.o 2>&1 | tail -30
x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-modaloverlay.o \
  qt/libdogecoinqt_a-dogecoingui.o \
  qt/libdogecoinqt_a-themeswitcher.o \
  qt/libdogecoinqt_a-optionsdialog.o \
  qt/libdogecoinqt_a-arcadepage.o \
  qt/libdogecoinqt_a-arcadegamewidget.o \
  qt/libdogecoinqt_a-moc_arcadepage.o \
  qt/libdogecoinqt_a-moc_arcadegamewidget.o \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
rm -f qt/dogecoin-qt.exe
make qt/dogecoin-qt.exe 2>&1 | tail -25
if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q ArcadePageC1; then
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-arcadepage.o \
    qt/libdogecoinqt_a-arcadegamewidget.o \
    qt/libdogecoinqt_a-moc_arcadepage.o \
    qt/libdogecoinqt_a-moc_arcadegamewidget.o \
    qt/libdogecoinqt_a-modaloverlay.o \
    qt/libdogecoinqt_a-dogecoingui.o
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe 2>&1 | tail -15
fi
test -f qt/dogecoin-qt.exe
strings qt/dogecoin-qt.exe | grep -F 'Syncing headers first' | head -2
cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
ls -la qt/dogecoin-qt.exe
echo PROGRESS_UI_OK
