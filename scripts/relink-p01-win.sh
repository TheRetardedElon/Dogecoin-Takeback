#!/usr/bin/env bash
# Relink dogecoind/qt after P0.1 object compile (expects objects already built).
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
cd "$BUILD/src"

SERVER_OBJS=(
  libdogecoin_server_a-addrman.o
  libdogecoin_server_a-addrdb.o
  libdogecoin_server_a-bloom.o
  libdogecoin_server_a-blockencodings.o
  libdogecoin_server_a-chain.o
  libdogecoin_server_a-checkpoints.o
  libdogecoin_server_a-httprpc.o
  libdogecoin_server_a-httpserver.o
  libdogecoin_server_a-ibdstats.o
  libdogecoin_server_a-init.o
  libdogecoin_server_a-dbwrapper.o
  libdogecoin_server_a-merkleblock.o
  libdogecoin_server_a-miner.o
  libdogecoin_server_a-net.o
  libdogecoin_server_a-net_processing.o
  libdogecoin_server_a-noui.o
  policy/libdogecoin_server_a-fees.o
  policy/libdogecoin_server_a-policy.o
  libdogecoin_server_a-pow.o
  libdogecoin_server_a-rest.o
  rpc/libdogecoin_server_a-auxcache.o
  rpc/libdogecoin_server_a-auxpow.o
  rpc/libdogecoin_server_a-blockchain.o
  rpc/libdogecoin_server_a-mining.o
  rpc/libdogecoin_server_a-misc.o
  rpc/libdogecoin_server_a-net.o
  rpc/libdogecoin_server_a-rawtransaction.o
  rpc/libdogecoin_server_a-server.o
  script/libdogecoin_server_a-sigcache.o
  script/libdogecoin_server_a-ismine.o
  libdogecoin_server_a-timedata.o
  libdogecoin_server_a-torcontrol.o
  libdogecoin_server_a-txdb.o
  libdogecoin_server_a-txmempool.o
  libdogecoin_server_a-txrequest.o
  libdogecoin_server_a-ui_interface.o
  libdogecoin_server_a-validation.o
  libdogecoin_server_a-validationinterface.o
  libdogecoin_server_a-versionbits.o
)
for extra in libdogecoin_server_a-dogecoin.o libdogecoin_server_a-dogecoin-fees.o; do
  [[ -f "$extra" ]] && SERVER_OBJS+=("$extra")
done

for o in "${SERVER_OBJS[@]}"; do
  [[ -f "$o" ]] || { echo "MISSING $o"; exit 1; }
done

rm -f libdogecoin_server.a
STAGE=$(mktemp -d)
i=0
STAGED=()
for o in "${SERVER_OBJS[@]}"; do
  dest="$STAGE/${i}_$(basename "$o")"
  cp -f "$o" "$dest"
  STAGED+=("$dest")
  i=$((i+1))
done
x86_64-w64-mingw32-ar cr libdogecoin_server.a "${STAGED[@]}"
x86_64-w64-mingw32-ranlib libdogecoin_server.a
rm -rf "$STAGE"

x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-networkpage.o \
  qt/libdogecoinqt_a-moc_networkpage.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

rm -f dogecoind.exe qt/dogecoin-qt.exe
make dogecoind.exe 2>&1 | tail -8
make qt/dogecoin-qt.exe 2>&1 | tail -8

x86_64-w64-mingw32-strings dogecoind.exe | grep -E 'comfortable mainnet|raise -dbcache|getibdinfo' | head -8
x86_64-w64-mingw32-strings qt/dogecoin-qt.exe | grep -E 'raise -dbcache|getibdinfo' | head -5
file dogecoind.exe qt/dogecoin-qt.exe
echo P01_RELINK_OK
