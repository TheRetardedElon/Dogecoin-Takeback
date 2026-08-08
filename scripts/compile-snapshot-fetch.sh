#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild
S=/mnt/c/dogedev
mkdir -p "$B/src/node" "$B/src/rpc" "$B/src/test"
cp -f "$S/src/node/snapshot_fetch.cpp" "$S/src/node/snapshot_fetch.h" "$B/src/node/"
cp -f "$S/src/rpc/blockchain.cpp" "$B/src/rpc/"
cp -f "$S/src/test/snapshot_fetch_tests.cpp" "$B/src/test/"

# Ensure Makefile lists the new source (generated Makefile)
if ! grep -q 'snapshot_fetch.cpp' "$B/src/Makefile"; then
  # Insert next to utxo_snapshot.cpp in libdogecoin_server sources
  sed -i 's|node/utxo_snapshot\.cpp|node/snapshot_fetch.cpp \\\n\tnode/utxo_snapshot.cpp|g' "$B/src/Makefile"
fi
if ! grep -q 'snapshot_fetch.h' "$B/src/Makefile"; then
  sed -i 's|node/utxo_snapshot\.h|node/snapshot_fetch.h \\\n\tnode/utxo_snapshot.h|g' "$B/src/Makefile" || true
fi

cd "$B"
make -C src node/libdogecoin_server_a-snapshot_fetch.o 2>&1 | tail -50
echo "---"
make -C src rpc/libdogecoin_server_a-blockchain.o 2>&1 | tail -40
echo COMPILE_TRY_DONE
