#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"
cp -f "$SRC/src/qt/themeswitcher.cpp" "$SRC/src/qt/themeswitcher.h" qt/
cp -f "$SRC/src/qt/optionsdialog.cpp" qt/
# only these objects + link
make qt/libdogecoinqt_a-themeswitcher.o qt/libdogecoinqt_a-optionsdialog.o 2>&1 | tail -20
# ensure arcade/peermap still in archive if make re-ars
x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-themeswitcher.o \
  qt/libdogecoinqt_a-optionsdialog.o \
  qt/libdogecoinqt_a-arcadepage.o \
  qt/libdogecoinqt_a-arcadegamewidget.o \
  qt/libdogecoinqt_a-moc_arcadepage.o \
  qt/libdogecoinqt_a-moc_arcadegamewidget.o \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o 2>/dev/null || true
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
rm -f qt/dogecoin-qt.exe
make qt/dogecoin-qt.exe 2>&1 | tail -25
# if arcade missing, patch and relink
if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q 'ArcadePageC1'; then
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
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
  make qt/dogecoin-qt.exe 2>&1 | tail -15
fi
test -f qt/dogecoin-qt.exe
cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
ls -la qt/dogecoin-qt.exe
echo THEME_OPTIONS_OK
