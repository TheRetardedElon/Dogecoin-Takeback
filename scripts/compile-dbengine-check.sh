#!/usr/bin/env bash
set -euo pipefail
cd /mnt/c/dogedev/src
make libdogecoin_server_a-dbengine.o libdogecoin_server_a-init.o -j2
ls -l libdogecoin_server_a-dbengine.o libdogecoin_server_a-dbwrapper.o libdogecoin_server_a-init.o
echo COMPILE_OK
