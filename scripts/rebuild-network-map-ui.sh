#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"

cp -f "$SRC/src/qt/networkpage.cpp" qt/
cp -f "$SRC/src/qt/networkpage.h" qt/
cp -f "$SRC/src/qt/peermapwidget.cpp" qt/
cp -f "$SRC/src/qt/peermapwidget.h" qt/

FLAGS="-DHAVE_CONFIG_H -I. -I../src/config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include"
FLAGS="$FLAGS -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include"
FLAGS="$FLAGS -I./qt -I./qt/forms -DQT_NO_KEYWORDS"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtCore"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtGui"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtWidgets"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtNetwork"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtTest"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtDBus"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtPrintSupport"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/"
FLAGS="$FLAGS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB"
FLAGS="$FLAGS -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden"

compile() {
  echo "CXX $1"
  x86_64-w64-mingw32-g++ -std=c++11 $FLAGS -c -o "$1" "$2"
}

MOC=../depends/x86_64-w64-mingw32/native/bin/moc
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_networkpage.cpp qt/networkpage.h
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_peermapwidget.cpp qt/peermapwidget.h

compile qt/libdogecoinqt_a-networkpage.o qt/networkpage.cpp
compile qt/libdogecoinqt_a-moc_networkpage.o qt/moc_networkpage.cpp
compile qt/libdogecoinqt_a-peermapwidget.o qt/peermapwidget.cpp
compile qt/libdogecoinqt_a-moc_peermapwidget.o qt/moc_peermapwidget.cpp

rm -f qt/libdogecoinqt.a qt/dogecoin-qt.exe
x86_64-w64-mingw32-ar cr qt/libdogecoinqt.a qt/libdogecoinqt_a-*.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

LINK=$(make -n qt/dogecoin-qt.exe 2>/dev/null | tr ';' '\n' | grep -E 'dogecoin-qt.exe' | grep x86_64 | tail -1)
echo "LINK=$LINK"
eval "$LINK"
file qt/dogecoin-qt.exe

bash /mnt/c/dogedev/scripts/package-windows-release.sh || true
OUT=/mnt/c/dogedev/release
cp -f /home/theretardedelon/dogedev-winbuild/dogecoin-1.14.100-win64-setup.exe "$OUT/" 2>/dev/null || true
(
  cd "$OUT"
  sha256sum dogecoin-1.14.100-win64.zip dogecoin-1.14.100-win64-setup.exe 2>/dev/null | tee SHA256SUMS-win64.txt
  ls -lah dogecoin-1.14.100-win64*
)
echo DONE
