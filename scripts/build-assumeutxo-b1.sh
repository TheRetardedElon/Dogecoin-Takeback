#!/usr/bin/env bash
# Compile AssumeUTXO Phase B1/B2 (utxo_snapshot + chainstate + RPCs) into dogecoind.exe
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"
mkdir -p node rpc

cp -f "$SRC/src/node/chainstate.cpp" "$SRC/src/node/chainstate.h" node/
cp -f "$SRC/src/node/utxo_snapshot.cpp" "$SRC/src/node/utxo_snapshot.h" node/
cp -f "$SRC/src/init.cpp" .
cp -f "$SRC/src/rpc/blockchain.cpp" rpc/
cp -f "$SRC/src/txdb.cpp" "$SRC/src/txdb.h" .
cp -f "$SRC/src/validation.cpp" .
cp -f "$SRC/src/net_processing.cpp" .
cp -f "$SRC/src/chainparams.cpp" "$SRC/src/chainparams.h" .
cp -f "$SRC/src/qt/clientmodel.cpp" "$SRC/src/qt/clientmodel.h" qt/ 2>/dev/null || true
mkdir -p qt
cp -f "$SRC/src/qt/clientmodel.cpp" "$SRC/src/qt/clientmodel.h" qt/
cp -f "$SRC/src/qt/dogecoingui.cpp" qt/
if ! grep -q 'utxo_snapshot.cpp' Makefile 2>/dev/null; then
  echo "Note: Makefile may not list utxo_snapshot yet; compiling object manually"
fi

FLAGS="-std=c++11 -DHAVE_CONFIG_H -I. -I../src/config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include"
FLAGS="$FLAGS -I./leveldb/include -I./secp256k1/include -I./univalue/include -DSTATICLIB -DMINIUPNP_STATICLIB"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/"
FLAGS="$FLAGS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB -pipe -O2 -fvisibility=hidden"

echo "CXX node/chainstate"
x86_64-w64-mingw32-g++ $FLAGS -c -o libdogecoin_server_a-chainstate.o node/chainstate.cpp
echo "CXX node/utxo_snapshot"
x86_64-w64-mingw32-g++ $FLAGS -c -o libdogecoin_server_a-utxo_snapshot.o node/utxo_snapshot.cpp
echo "CXX txdb"
x86_64-w64-mingw32-g++ $FLAGS -c -o libdogecoin_server_a-txdb.o txdb.cpp
echo "CXX chainparams"
x86_64-w64-mingw32-g++ $FLAGS -c -o libdogecoin_common_a-chainparams.o chainparams.cpp || \
  make libdogecoin_common_a-chainparams.o
echo "make init + blockchain + validation + net_processing"
make libdogecoin_server_a-init.o rpc/libdogecoin_server_a-blockchain.o libdogecoin_server_a-validation.o libdogecoin_server_a-net_processing.o
echo "ar update server"
x86_64-w64-mingw32-ar r libdogecoin_server.a \
  libdogecoin_server_a-chainstate.o \
  libdogecoin_server_a-utxo_snapshot.o \
  libdogecoin_server_a-txdb.o \
  libdogecoin_server_a-init.o \
  libdogecoin_server_a-validation.o \
  libdogecoin_server_a-net_processing.o \
  rpc/libdogecoin_server_a-blockchain.o
x86_64-w64-mingw32-ranlib libdogecoin_server.a
# common lib for chainparams if object built
if [ -f libdogecoin_common_a-chainparams.o ]; then
  x86_64-w64-mingw32-ar r libdogecoin_common.a libdogecoin_common_a-chainparams.o
  x86_64-w64-mingw32-ranlib libdogecoin_common.a
fi
rm -f dogecoind.exe
make dogecoind.exe
echo "=== strings ==="
x86_64-w64-mingw32-strings dogecoind.exe | grep -E 'mapAssumeutxo|attested AssumeUTXO|assumeutxodev|stepbackgroundvalidation' | head -20
file dogecoind.exe
echo ASSUMEUTXO_D1_OK
# Qt optional (slow)
if [ "${BUILD_QT:-0}" = "1" ]; then
  make -C . qt/libdogecoinqt_a-clientmodel.o qt/libdogecoinqt_a-dogecoingui.o 2>/dev/null || true
  make qt/dogecoin-qt.exe || true
fi
