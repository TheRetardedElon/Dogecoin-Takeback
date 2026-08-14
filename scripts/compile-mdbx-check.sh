#!/usr/bin/env bash
set -euo pipefail
cd /mnt/c/dogedev/src
export CFLAGS="${CFLAGS:-} -Wno-error -Wno-unused-function"
echo "=== mdbx.c (automake target) ==="
make mdbx/libdogecoin_server_a-mdbx.o
echo "=== wrapper / engine / init ==="
make libdogecoin_server_a-dbwrapper.o libdogecoin_server_a-dbengine.o libdogecoin_server_a-init.o -j2
ls -l mdbx/libdogecoin_server_a-mdbx.o libdogecoin_server_a-dbwrapper.o
echo COMPILE_OK
