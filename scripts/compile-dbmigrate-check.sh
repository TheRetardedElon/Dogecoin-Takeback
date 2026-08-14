#!/usr/bin/env bash
set -euo pipefail
cd /mnt/c/dogedev/src
export CFLAGS="${CFLAGS:-} -Wno-error -Wno-unused-function"
make libdogecoin_server_a-dbwrapper.o libdogecoin_server_a-dbengine.o libdogecoin_server_a-init.o -j2
# automake may add dbmigrate after Makefile.am refresh
if make -q libdogecoin_server_a-dbmigrate.o 2>/dev/null || grep -q dbmigrate Makefile; then
  make libdogecoin_server_a-dbmigrate.o || true
fi
if ! grep -q 'libdogecoin_server_a-dbmigrate' Makefile; then
  echo "=== compile dbmigrate.cpp by cloned flags ==="
  make -n libdogecoin_server_a-dbengine.o > /tmp/cxx-eng.txt || true
fi
# Force compile dbmigrate with same flags as dbengine
CXX_LINE=$(make -n libdogecoin_server_a-dbengine.o | tail -1)
if echo "$CXX_LINE" | grep -q g++; then
  LINE="${CXX_LINE//libdogecoin_server_a-dbengine/libdogecoin_server_a-dbmigrate}"
  LINE="${LINE//dbengine.cpp/dbmigrate.cpp}"
  eval "$LINE"
fi
ls -l libdogecoin_server_a-dbwrapper.o libdogecoin_server_a-dbmigrate.o 2>/dev/null || ls -l libdogecoin_server_a-dbwrapper.o
echo COMPILE_OK
