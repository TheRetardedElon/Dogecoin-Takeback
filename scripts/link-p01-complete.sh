#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"

cp -f "$SRC/src/ibdstats.cpp" "$SRC/src/ibdstats.h" .
cp -f "$SRC/src/validation.cpp" .
cp -f "$SRC/src/init.cpp" .
cp -f "$SRC/src/net.cpp" .
cp -f "$SRC/src/net_processing.cpp" .
cp -f "$SRC/src/rpc/blockchain.cpp" rpc/
cp -f "$SRC/src/qt/networkpage.cpp" "$SRC/src/qt/networkpage.h" qt/
cp -f "$SRC/src/qt/clientmodel.cpp" "$SRC/src/qt/clientmodel.h" qt/

# Rebuild objects with correct make flags
make libdogecoin_server_a-net.o \
     libdogecoin_server_a-init.o \
     libdogecoin_server_a-validation.o \
     libdogecoin_server_a-net_processing.o \
     rpc/libdogecoin_server_a-blockchain.o

# ibdstats is not in the generated Makefile yet — compile with same flags as init.o
x86_64-w64-mingw32-g++ -std=c++11 -DHAVE_CONFIG_H -I. -I../src/config \
  -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads \
  -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include \
  -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include \
  -DSTATICLIB -DMINIUPNP_STATICLIB \
  -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/ \
  -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB \
  -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden \
  -c -o libdogecoin_server_a-ibdstats.o ibdstats.cpp

make libdogecoin_server.a
x86_64-w64-mingw32-ar r libdogecoin_server.a libdogecoin_server_a-ibdstats.o
x86_64-w64-mingw32-ranlib libdogecoin_server.a

make qt/libdogecoinqt_a-networkpage.o qt/libdogecoinqt_a-clientmodel.o || true
# moc if needed
MOC=/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/native/bin/moc
if [[ -f qt/networkpage.h ]]; then
  "$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_networkpage.cpp qt/networkpage.h
  make qt/libdogecoinqt_a-moc_networkpage.o 2>/dev/null || \
  x86_64-w64-mingw32-g++ -std=c++11 -DHAVE_CONFIG_H -I. -I../src/config -I./qt -I./qt/forms -DQT_NO_KEYWORDS \
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads \
    -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include \
    -I./leveldb/include -I./secp256k1/include -I./univalue/include -DSTATICLIB -DMINIUPNP_STATICLIB \
    -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/ \
    -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtCore \
    -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtGui \
    -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtWidgets \
    -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtNetwork \
    -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtPrintSupport \
    -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB \
    -pipe -O2 -fvisibility=hidden -c -o qt/libdogecoinqt_a-moc_networkpage.o qt/moc_networkpage.cpp
fi

x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-networkpage.o \
  qt/libdogecoinqt_a-clientmodel.o \
  qt/libdogecoinqt_a-moc_networkpage.o 2>/dev/null || true
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

rm -f dogecoind.exe qt/dogecoin-qt.exe
make dogecoind.exe
make qt/dogecoin-qt.exe

echo "=== verify ==="
x86_64-w64-mingw32-nm libdogecoin_server.a | grep NoteFlush | head -2
x86_64-w64-mingw32-strings dogecoind.exe | grep -F "comfortable mainnet" | head -2
x86_64-w64-mingw32-strings dogecoind.exe | grep -F "raise -dbcache" | head -2
x86_64-w64-mingw32-strings dogecoind.exe | grep getibdinfo | head -2
file dogecoind.exe qt/dogecoin-qt.exe
echo P01_LINK_OK
