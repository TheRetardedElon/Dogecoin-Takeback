#!/usr/bin/env bash
# Fix winbuild: explicit compile snapshot_fetch, ar, link PE, package 1.14.102
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
VERSION=1.14.102
REL=dogecoin-${VERSION}-win64
LOG=/home/theretardedelon/win-fix-link-102.log
DEP_INC=/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include

exec > >(tee "$LOG") 2>&1
echo "==> $(date -Is)"

mkdir -p "$B/src/node" "$B/src/rpc"
cp -f "$SRC/src/node/snapshot_fetch.cpp" "$SRC/src/node/snapshot_fetch.h" "$B/src/node/"
cp -f "$SRC/src/rpc/blockchain.cpp" "$B/src/rpc/"
cp -f "$SRC/src/node/utxo_snapshot.cpp" "$SRC/src/node/utxo_snapshot.h" "$B/src/node/"
cp -f "$SRC/src/clientversion.h" "$B/src/" 2>/dev/null || true

cd "$B/src"

CXXFLAGS_COMMON=(
  -std=c++11 -DHAVE_CONFIG_H
  -I. -I../src/config
  -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
  -I. -I./obj -mthreads
  "-I${DEP_INC}"
  -I./leveldb/include -I./leveldb/helpers/memenv
  -I./secp256k1/include -I./univalue/include
  -DSTATICLIB -DMINIUPNP_STATICLIB
  "-I${DEP_INC}/"
  -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS
  -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB
  -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden
)

compile_one() {
  local out="$1" src="$2"
  echo "CXX $src -> $out"
  rm -f "$out"
  x86_64-w64-mingw32-g++ "${CXXFLAGS_COMMON[@]}" -c -o "$out" "$src"
  local sz
  sz=$(stat -c%s "$out")
  echo "  size=$sz"
  [[ "$sz" -gt 1000 ]] || { echo "FATAL tiny $out"; exit 1; }
}

compile_one node/libdogecoin_server_a-snapshot_fetch.o node/snapshot_fetch.cpp
compile_one node/libdogecoin_server_a-utxo_snapshot.o node/utxo_snapshot.cpp
compile_one rpc/libdogecoin_server_a-blockchain.o rpc/blockchain.cpp

echo "==> ar libdogecoin_server.a"
ar r libdogecoin_server.a \
  node/libdogecoin_server_a-snapshot_fetch.o \
  node/libdogecoin_server_a-utxo_snapshot.o \
  rpc/libdogecoin_server_a-blockchain.o
ranlib libdogecoin_server.a
nm libdogecoin_server.a | grep FetchSnapshotArtifact | head -5
nm libdogecoin_server.a | grep ResolveSnapshotFromManifest | head -3

echo "==> link dogecoind.exe"
rm -f dogecoind.exe
make dogecoind.exe
test -f dogecoind.exe
x86_64-w64-mingw32-strings dogecoind.exe | grep -E 'dumptxoutset|fetchassumeutxomanifest' | head -5

echo "==> dogecoin-cli.exe"
make dogecoin-cli.exe || true

echo "==> dogecoin-qt.exe"
make qt/dogecoin-qt.exe
test -f qt/dogecoin-qt.exe

echo "==> stage"
mkdir -p "$B/release" "$OUT"
cp -f dogecoind.exe dogecoin-cli.exe "$B/release/"
[[ -f dogecoin-tx.exe ]] && cp -f dogecoin-tx.exe "$B/release/" || true
cp -f qt/dogecoin-qt.exe "$B/release/"
x86_64-w64-mingw32-strip -s "$B/release"/*.exe || true
date -u +"FULL_RELEASE %Y-%m-%dT%H:%M:%SZ" > "$B/release/BUILD_STAMP.txt"
ls -la "$B/release"/*.exe

STAGE="$B/release-staging/$REL"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/daemon"
cp -f "$B/release/dogecoin-qt.exe" "$STAGE/"
cp -f "$B/release/dogecoind.exe" "$STAGE/daemon/"
cp -f "$B/release/dogecoin-cli.exe" "$STAGE/daemon/"
cp -f "$B/release/dogecoin-cli.exe" "$STAGE/bin/"
[[ -f $B/release/dogecoin-tx.exe ]] && cp -f "$B/release/dogecoin-tx.exe" "$STAGE/bin/" || true
cp -f "$SRC/COPYING" "$STAGE/COPYING.txt" 2>/dev/null || true
cp -f "$B/release/BUILD_STAMP.txt" "$STAGE/"
echo "Dogecoin Core Pro ${VERSION} FULL WIN64" > "$STAGE/CORE_PRO.txt"
ZIP="$OUT/${REL}.zip"
rm -f "$ZIP"
( cd "$B/release-staging" && find "$REL" -type f | sort | zip -X@ "$ZIP" )
ls -lh "$ZIP"

echo "==> NSIS"
VERSION="$VERSION" bash "$SRC/scripts/make-setup-1.14.101.sh"
(
  cd "$OUT"
  sha256sum "${REL}.zip" "${REL}-setup.exe" 2>/dev/null | tee SHA256SUMS-win64.txt
)
ls -lah "$OUT"/${REL}*
echo FULL_RELEASE_102_OK
