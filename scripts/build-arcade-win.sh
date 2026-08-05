#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"

cp -f "$SRC/src/qt/arcadepage.cpp" "$SRC/src/qt/arcadepage.h" qt/ 2>/dev/null || true
cp -f "$SRC/src/qt/arcadegamewidget.cpp" "$SRC/src/qt/arcadegamewidget.h" qt/
cp -f "$SRC/src/qt/walletview.cpp" "$SRC/src/qt/walletview.h" qt/
cp -f "$SRC/src/qt/walletframe.cpp" "$SRC/src/qt/walletframe.h" qt/
cp -f "$SRC/src/qt/dogecoingui.cpp" "$SRC/src/qt/dogecoingui.h" qt/
cp -f "$SRC/src/qt/dogecoin.qrc" qt/
mkdir -p qt/res/images
cp -f "$SRC/src/qt/res/images/retrdoge-title.png" qt/res/images/ 2>/dev/null || true
cp -f "$SRC/src/qt/res/images/worldmap-yellow.png" qt/res/images/ 2>/dev/null || true
cp -f "$SRC/src/Makefile.qt.include" ./

MOC=../depends/x86_64-w64-mingw32/native/bin/moc
RCC=../depends/x86_64-w64-mingw32/native/bin/rcc

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

"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_arcadepage.cpp qt/arcadepage.h
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_arcadegamewidget.cpp qt/arcadegamewidget.h
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_walletview.cpp qt/walletview.h
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_walletframe.cpp qt/walletframe.h
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_dogecoingui.cpp qt/dogecoingui.h
"$RCC" -name dogecoin -o qt/qrc_dogecoin.cpp qt/dogecoin.qrc

compile qt/libdogecoinqt_a-arcadepage.o qt/arcadepage.cpp
compile qt/libdogecoinqt_a-arcadegamewidget.o qt/arcadegamewidget.cpp
compile qt/libdogecoinqt_a-moc_arcadepage.o qt/moc_arcadepage.cpp
compile qt/libdogecoinqt_a-moc_arcadegamewidget.o qt/moc_arcadegamewidget.cpp
compile qt/libdogecoinqt_a-walletview.o qt/walletview.cpp
compile qt/libdogecoinqt_a-walletframe.o qt/walletframe.cpp
compile qt/libdogecoinqt_a-dogecoingui.o qt/dogecoingui.cpp
compile qt/libdogecoinqt_a-moc_walletview.o qt/moc_walletview.cpp
compile qt/libdogecoinqt_a-moc_walletframe.o qt/moc_walletframe.cpp
compile qt/libdogecoinqt_a-moc_dogecoingui.o qt/moc_dogecoingui.cpp
compile qt/libdogecoinqt_a-qrc_dogecoin.o qt/qrc_dogecoin.cpp

rm -f qt/libdogecoinqt.a qt/dogecoin-qt.exe
x86_64-w64-mingw32-ar cr qt/libdogecoinqt.a qt/libdogecoinqt_a-*.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

LINK=$(make -n qt/dogecoin-qt.exe 2>/dev/null | tr ';' '\n' | grep -E 'dogecoin-qt.exe' | grep x86_64 | tail -1)
echo "LINK=$LINK"
eval "$LINK"
file qt/dogecoin-qt.exe
x86_64-w64-mingw32-strings qt/dogecoin-qt.exe | grep -E 'Arcade|SHIBE|Shibe Blaster|RETR' | head -15

bash /mnt/c/dogedev/scripts/package-windows-release.sh || true
OUT=/mnt/c/dogedev/release
cp -f /home/theretardedelon/dogedev-winbuild/dogecoin-1.14.101-win64-setup.exe "$OUT/" 2>/dev/null || true
(
  cd "$OUT"
  sha256sum dogecoin-1.14.101-win64.zip dogecoin-1.14.101-win64-setup.exe 2>/dev/null | tee SHA256SUMS-win64.txt
  ls -lah dogecoin-1.14.101-win64*
)
echo ARCADE_DONE
