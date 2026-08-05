#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"
cp -f "$SRC/src/validation.cpp" .
cp -f "$SRC/src/init.cpp" .
cp -f "$SRC/src/net.cpp" .
cp -f "$SRC/src/rpc/blockchain.cpp" rpc/
cp -f "$SRC/src/qt/networkpage.cpp" "$SRC/src/qt/networkpage.h" qt/

DEP_INC="$BUILD/depends/x86_64-w64-mingw32/share/../include"
FLAGS="-DHAVE_CONFIG_H -I. -I../src/config -I./config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads"
FLAGS="$FLAGS -I$DEP_INC -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include"
FLAGS="$FLAGS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB"
FLAGS="$FLAGS -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden -std=c++11"
QTFLAGS="$FLAGS -I./qt -I./qt/forms -DQT_NO_KEYWORDS"
QTFLAGS="$QTFLAGS -I$DEP_INC/QtCore -I$DEP_INC/QtGui -I$DEP_INC/QtWidgets -I$DEP_INC/QtNetwork -I$DEP_INC/QtPrintSupport -I$DEP_INC/"

compile() {
  echo "CXX $1"
  x86_64-w64-mingw32-g++ $3 -c -o "$1" "$2"
}

compile libdogecoin_server_a-validation.o validation.cpp "$FLAGS"
compile libdogecoin_server_a-init.o init.cpp "$FLAGS"
compile libdogecoin_server_a-net.o net.cpp "$FLAGS"
compile rpc/libdogecoin_server_a-blockchain.o rpc/blockchain.cpp "$FLAGS"

MOC="$BUILD/depends/x86_64-w64-mingw32/native/bin/moc"
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_networkpage.cpp qt/networkpage.h
compile qt/libdogecoinqt_a-networkpage.o qt/networkpage.cpp "$QTFLAGS"
compile qt/libdogecoinqt_a-moc_networkpage.o qt/moc_networkpage.cpp "$QTFLAGS"

ls -la libdogecoin_server_a-net.o libdogecoin_server_a-init.o libdogecoin_server_a-validation.o rpc/libdogecoin_server_a-blockchain.o
echo P01_COMPILE_OK
