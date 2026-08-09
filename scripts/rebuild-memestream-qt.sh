#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild
S=/mnt/c/dogedev

echo "=== check winbuild host ==="
grep -E 'target os|host system|x86_64-w64-mingw' "$B/src/config/dogecoin-config.h" 2>/dev/null | head -10 || true
grep -E 'host system|target os|CXX' "$B/config.log" 2>/dev/null | head -15 || true
head -5 "$B/src/Makefile" 2>/dev/null || true
grep 'x86_64-w64-mingw32-g++' "$B/src/Makefile" 2>/dev/null | head -3 || echo "NO MINGW IN MAKEFILE"

cp -f "$S/src/qt/memestreamclient.cpp" "$S/src/qt/memestreamclient.h" \
      "$S/src/qt/memestreampage.cpp" "$S/src/qt/memestreampage.h" \
      "$S/src/qt/memestreamrail.cpp" "$S/src/qt/memestreamrail.h" \
      "$B/src/qt/"

cd "$B/src"

if grep -q 'x86_64-w64-mingw32-g++' Makefile 2>/dev/null; then
  echo "=== mingw make memestream objects ==="
  make qt/libdogecoinqt_a-memestreamclient.o \
       qt/libdogecoinqt_a-memestreampage.o \
       qt/libdogecoinqt_a-memestreamrail.o 2>&1 | tail -50
else
  echo "FATAL: winbuild Makefile is not MinGW — do not full-make until reconf"
  exit 2
fi

# Re-ar into libdogecoinqt.a
if [[ -f qt/libdogecoinqt.a ]]; then
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-memestreamclient.o \
    qt/libdogecoinqt_a-memestreampage.o \
    qt/libdogecoinqt_a-memestreamrail.o
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
  echo "Updated libdogecoinqt.a"
fi

# Relink qt if possible
if [[ -f qt/dogecoin-qt.exe ]] || true; then
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe 2>&1 | tail -30
fi

if [[ -f qt/dogecoin-qt.exe ]]; then
  file qt/dogecoin-qt.exe
  strings qt/dogecoin-qt.exe | grep -F 'MemeStream: SSL available' | head -2
  strings qt/dogecoin-qt.exe | grep -F 'system CA count' | head -2 || true
  cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/ 2>/dev/null || true
  echo MEMESTREAM_QT_REBUILD_OK
else
  echo MEMESTREAM_QT_REBUILD_NO_EXE
  exit 1
fi
