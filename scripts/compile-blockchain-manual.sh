#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
cd /home/theretardedelon/dogedev-winbuild/src
# Reuse the exact recipe pattern from snapshot_fetch (known working)
LINE=$(sed -n '/libdogecoin_server_a-snapshot_fetch.o: node\/snapshot_fetch.cpp/,+1p' Makefile | tail -1)
# strip leading tab
LINE=${LINE#$'\t'}
# substitute paths
CMD=${LINE//node\/snapshot_fetch/rpc\/blockchain}
CMD=${CMD//libdogecoin_server_a-snapshot_fetch/libdogecoin_server_a-blockchain}
echo "CMD=$CMD"
eval "$CMD"
ls -la rpc/libdogecoin_server_a-blockchain.o
echo OK
