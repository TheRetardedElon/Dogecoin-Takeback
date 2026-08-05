#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"
mkdir -p node
cp -f "$SRC/src/node/chainstate.cpp" "$SRC/src/node/chainstate.h" node/
cp -f "$SRC/src/init.cpp" .
cp -f "$SRC/src/rpc/blockchain.cpp" rpc/

FLAGS="-std=c++11 -DHAVE_CONFIG_H -I. -I../src/config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include"
FLAGS="$FLAGS -I./leveldb/include -I./secp256k1/include -I./univalue/include -DSTATICLIB -DMINIUPNP_STATICLIB"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/"
FLAGS="$FLAGS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB -pipe -O2 -fvisibility=hidden"

echo "CXX node/chainstate"
x86_64-w64-mingw32-g++ $FLAGS -c -o libdogecoin_server_a-chainstate.o node/chainstate.cpp
echo "make init + blockchain"
make libdogecoin_server_a-init.o rpc/libdogecoin_server_a-blockchain.o
echo "ar update"
x86_64-w64-mingw32-ar r libdogecoin_server.a \
  libdogecoin_server_a-chainstate.o \
  libdogecoin_server_a-init.o \
  rpc/libdogecoin_server_a-blockchain.o \
  libdogecoin_server_a-ibdstats.o
x86_64-w64-mingw32-ranlib libdogecoin_server.a
rm -f dogecoind.exe
make dogecoind.exe
echo "=== strings ==="
x86_64-w64-mingw32-strings dogecoind.exe | grep getchainstates | head -5
x86_64-w64-mingw32-strings dogecoind.exe | grep -F 'Chainstate: active' | head -3
x86_64-w64-mingw32-strings dogecoind.exe | grep -F 'A1-single-wrapper' | head -3
file dogecoind.exe
echo CHAINSTATE_A1_OK
