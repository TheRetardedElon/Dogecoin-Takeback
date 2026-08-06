#!/usr/bin/env bash
# Remoc DogecoinGUI (and PeerMap) WITH -DENABLE_WALLET so wallet slots exist.
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
cd "$BUILD/src"
DEP="$BUILD/depends/x86_64-w64-mingw32"
MOC="$DEP/native/bin/moc"

DEFS="-DHAVE_CONFIG_H -DENABLE_WALLET -DQT_NO_KEYWORDS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB"
INCS="-I. -I../src/config -I./obj -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include -I./qt -I./qt/forms -I${DEP}/include -I${DEP}/include/QtCore -I${DEP}/include/QtGui -I${DEP}/include/QtWidgets -I${DEP}/include/QtNetwork"

cp -f /mnt/c/dogedev/src/qt/dogecoingui.h qt/
cp -f /mnt/c/dogedev/src/qt/dogecoingui.cpp qt/
cp -f /mnt/c/dogedev/src/qt/peermapwidget.h qt/
cp -f /mnt/c/dogedev/src/qt/peermapwidget.cpp qt/

echo "MOC with ENABLE_WALLET..."
"$MOC" $DEFS $INCS -o qt/moc_dogecoingui.cpp qt/dogecoingui.h
"$MOC" $DEFS $INCS -o qt/moc_peermapwidget.cpp qt/peermapwidget.h

echo "gotoOverviewPage count: $(grep -c gotoOverviewPage qt/moc_dogecoingui.cpp || true)"
if ! grep -q gotoOverviewPage qt/moc_dogecoingui.cpp; then
  echo "FATAL: moc still missing wallet slots"
  exit 1
fi

CXXFLAGS="-std=c++11 -DHAVE_CONFIG_H -I. -I../src/config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads -I${DEP}/include -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include -I./qt -I./qt/forms -DQT_NO_KEYWORDS -I${DEP}/include/QtCore -I${DEP}/include/QtGui -I${DEP}/include/QtWidgets -I${DEP}/include/QtNetwork -I${DEP}/include/QtTest -I${DEP}/include/QtDBus -I${DEP}/include/QtPrintSupport -I${DEP}/include/ -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden"

x86_64-w64-mingw32-g++ $CXXFLAGS -c -o qt/libdogecoinqt_a-moc_dogecoingui.o qt/moc_dogecoingui.cpp
x86_64-w64-mingw32-g++ $CXXFLAGS -c -o qt/libdogecoinqt_a-moc_peermapwidget.o qt/moc_peermapwidget.cpp
x86_64-w64-mingw32-g++ $CXXFLAGS -c -o qt/libdogecoinqt_a-dogecoingui.o qt/dogecoingui.cpp
x86_64-w64-mingw32-g++ $CXXFLAGS -c -o qt/libdogecoinqt_a-peermapwidget.o qt/peermapwidget.cpp

x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-moc_dogecoingui.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o \
  qt/libdogecoinqt_a-dogecoingui.o \
  qt/libdogecoinqt_a-peermapwidget.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

rm -f qt/dogecoin-qt.exe
make qt/dogecoin-qt.exe 2>&1 | tail -20

if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q 'T _ZN11DogecoinGUI16gotoOverviewPageEv'; then
  echo "re-insert after make AR"
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-moc_dogecoingui.o \
    qt/libdogecoinqt_a-moc_peermapwidget.o \
    qt/libdogecoinqt_a-dogecoingui.o \
    qt/libdogecoinqt_a-peermapwidget.o
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe 2>&1 | tail -15
fi

echo "Symbol check:"
x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep 'T _ZN11DogecoinGUI16gotoOverviewPageEv' | head -1
ls -la qt/dogecoin-qt.exe
cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
echo REMOC_OK
