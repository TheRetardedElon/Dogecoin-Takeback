#!/usr/bin/env bash
set -euo pipefail
cd /mnt/c/dogedev/src
export CXXFLAGS="${CXXFLAGS:-} -Wno-error"
echo "=== blockarchive + init + validation + rpc/blockchain ==="
make libdogecoin_server_a-blockarchive.o libdogecoin_server_a-init.o libdogecoin_server_a-validation.o rpc/libdogecoin_server_a-blockchain.o -j2
ls -l libdogecoin_server_a-blockarchive.o rpc/libdogecoin_server_a-blockchain.o
echo COMPILE_OK
