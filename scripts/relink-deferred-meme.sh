#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin
cd /home/theretardedelon/dogedev-winbuild/src

# Sync sources
cp -f /mnt/c/dogedev/src/qt/memestreamrail.cpp qt/
cp -f /mnt/c/dogedev/src/qt/memestreamrail.h qt/
cp -f /mnt/c/dogedev/src/qt/memestreampage.cpp qt/
cp -f /mnt/c/dogedev/src/qt/memestreampage.h qt/
cp -f /mnt/c/dogedev/src/qt/memestreamclient.cpp qt/

# Objects should already exist from prior make; rebuild if missing timestamps wrong
make qt/libdogecoinqt_a-memestreamrail.o qt/libdogecoinqt_a-memestreampage.o qt/libdogecoinqt_a-memestreamclient.o 2>&1 | tail -15

patch_ar() {
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-arcadepage.o \
    qt/libdogecoinqt_a-arcadegamewidget.o \
    qt/libdogecoinqt_a-moc_arcadepage.o \
    qt/libdogecoinqt_a-moc_arcadegamewidget.o \
    qt/libdogecoinqt_a-peermapwidget.o \
    qt/libdogecoinqt_a-moc_peermapwidget.o \
    qt/libdogecoinqt_a-memestreamrail.o \
    qt/libdogecoinqt_a-memestreampage.o \
    qt/libdogecoinqt_a-memestreamclient.o \
    qt/libdogecoinqt_a-moc_memestreampage.o \
    qt/libdogecoinqt_a-moc_memestreamrail.o \
    qt/libdogecoinqt_a-moc_dogecoingui.o
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
}

patch_ar
rm -f qt/dogecoin-qt.exe
if ! make qt/dogecoin-qt.exe 2>&1 | tail -25; then
  echo "first link failed, patch and retry"
  patch_ar
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe 2>&1 | tail -25
fi

# Always re-patch after make AR and link once more if binary missing or arcade gone
if [ ! -f qt/dogecoin-qt.exe ] || ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q 'ArcadePageC1'; then
  patch_ar
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe 2>&1 | tail -20
fi

# Final safety patch + link
patch_ar
rm -f qt/dogecoin-qt.exe
make qt/dogecoin-qt.exe 2>&1 | tail -15

test -f qt/dogecoin-qt.exe
x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep 'ArcadePageC1' | head -1
strings qt/libdogecoinqt_a-memestreamrail.o | grep 'load feed' || true
cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
ls -la qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
echo BUILD_OK
