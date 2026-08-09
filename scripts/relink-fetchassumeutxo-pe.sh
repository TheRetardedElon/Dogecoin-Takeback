#!/usr/bin/env bash
# Sync P1 snapshot_fetch sources into winbuild, re-ar server lib, relink dogecoind/cli, copy smoke-run.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild
S=/mnt/c/dogedev
SMOKE=/mnt/c/dogedev/smoke-run

log() { echo "[relink $(date +%H:%M:%S)] $*"; }

mkdir -p "$B/src/node" "$B/src/rpc" "$SMOKE"
cp -f "$S/src/node/snapshot_fetch.cpp" "$S/src/node/snapshot_fetch.h" "$B/src/node/"
cp -f "$S/src/rpc/blockchain.cpp" "$B/src/rpc/"
cp -f "$S/src/node/utxo_snapshot.cpp" "$S/src/node/utxo_snapshot.h" "$B/src/node/" 2>/dev/null || true
cp -f "$S/src/node/chainstate.cpp" "$S/src/node/chainstate.h" "$B/src/node/" 2>/dev/null || true

cd "$B/src"

# Ensure snapshot_fetch object exists
if [[ ! -f node/libdogecoin_server_a-snapshot_fetch.o ]]; then
  log "Building snapshot_fetch.o"
  make node/libdogecoin_server_a-snapshot_fetch.o
fi

# Force rebuild blockchain.o with fetchassumeutxo RPC
log "Building blockchain.o"
cat > /tmp/build-blockchain.mk <<'EOF'
include Makefile
.PHONY: force_blockchain
force_blockchain:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -c -o rpc/libdogecoin_server_a-blockchain.o `test -f 'rpc/blockchain.cpp' || echo '$(srcdir)/'`rpc/blockchain.cpp
EOF
make -f /tmp/build-blockchain.mk force_blockchain

test -f node/libdogecoin_server_a-snapshot_fetch.o
test -f rpc/libdogecoin_server_a-blockchain.o

log "Updating libdogecoin_server.a"
# Replace/add objects in archive
x86_64-w64-mingw32-ar r libdogecoin_server.a \
  node/libdogecoin_server_a-snapshot_fetch.o \
  rpc/libdogecoin_server_a-blockchain.o
x86_64-w64-mingw32-ranlib libdogecoin_server.a

# Confirm symbols (C++ mangled names contain the bare identifier)
if ! x86_64-w64-mingw32-nm libdogecoin_server.a 2>/dev/null | grep -F 'FetchSnapshotArtifact' >/dev/null; then
  log "FATAL: FetchSnapshotArtifact missing from archive"
  x86_64-w64-mingw32-ar t libdogecoin_server.a | grep snapshot || true
  exit 1
fi
if ! x86_64-w64-mingw32-nm libdogecoin_server.a 2>/dev/null | grep -F 'fetchassumeutxo' >/dev/null; then
  log "FATAL: fetchassumeutxo RPC missing from archive"
  exit 1
fi
log "Archive symbols OK"

log "Relinking dogecoind.exe dogecoin-cli.exe"
rm -f dogecoind.exe dogecoin-cli.exe
make dogecoind.exe dogecoin-cli.exe 2>&1 | tail -40

test -f dogecoind.exe
test -f dogecoin-cli.exe
file dogecoind.exe dogecoin-cli.exe

log "Symbol gate (nm/strings to file — avoid pipefail SIGPIPE false fails)"
FAIL=0
NM=$(mktemp); STR=$(mktemp)
x86_64-w64-mingw32-nm dogecoind.exe >"$NM" 2>/dev/null || true
strings dogecoind.exe >"$STR" 2>/dev/null || true
if grep -F 'FetchSnapshotArtifact' "$NM" >/dev/null; then echo " OK FetchSnapshotArtifact"; else echo " FAIL FetchSnapshotArtifact"; FAIL=1; fi
if grep -F 'fetchassumeutxo' "$STR" >/dev/null; then echo " OK string fetchassumeutxo"; else echo " FAIL string fetchassumeutxo"; FAIL=1; fi
rm -f "$NM" "$STR"
[[ "$FAIL" -eq 0 ]] || exit 1

log "Copy to smoke-run"
# strip not required for smoke
cp -f dogecoind.exe dogecoin-cli.exe "$SMOKE/"
# keep qt if present
ls -la "$SMOKE"/dogecoind.exe "$SMOKE"/dogecoin-cli.exe
date -u +"RELINK_FETCH %Y-%m-%dT%H:%M:%SZ" > "$SMOKE/RELINK_STAMP.txt"
echo RELINK_OK
