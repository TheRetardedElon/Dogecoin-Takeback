#!/usr/bin/env bash
# Build real 1.14.102 zip + NSIS setup from current winbuild binaries, stage release/latest.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin

B=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
VERSION=1.14.102
REL=dogecoin-${VERSION}-win64
HOST=x86_64-w64-mingw32
LATEST="$OUT/latest"

cd "$B/src"
test -f qt/dogecoin-qt.exe
test -f dogecoind.exe
test -f dogecoin-cli.exe

echo "=== stage winbuild/release (strip) ==="
mkdir -p "$B/release"
cp -f dogecoind.exe dogecoin-cli.exe "$B/release/"
[[ -f dogecoin-tx.exe ]] && cp -f dogecoin-tx.exe "$B/release/" || true
cp -f qt/dogecoin-qt.exe "$B/release/"
${HOST}-strip -s "$B/release"/*.exe || true
date -u +"SHIP_LATEST %Y-%m-%dT%H:%M:%SZ" > "$B/release/BUILD_STAMP.txt"
echo "Dogecoin Core Pro ${VERSION} Fast Sync + shutdown fix" > "$B/release/CORE_PRO.txt"
ls -lh "$B/release"/*.exe

echo "=== zip ==="
STAGE="$B/release-staging/${REL}"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/daemon"
cp -f "$B/release/dogecoin-qt.exe" "$STAGE/"
cp -f "$B/release/dogecoind.exe" "$STAGE/daemon/"
cp -f "$B/release/dogecoin-cli.exe" "$STAGE/daemon/"
cp -f "$B/release/dogecoin-cli.exe" "$STAGE/bin/"
[[ -f $B/release/dogecoin-tx.exe ]] && cp -f "$B/release/dogecoin-tx.exe" "$STAGE/bin/" || true
cp -f "$SRC/COPYING" "$STAGE/COPYING.txt" 2>/dev/null || true
cp -f "$B/release/BUILD_STAMP.txt" "$STAGE/"
cp -f "$B/release/CORE_PRO.txt" "$STAGE/"
ZIP="$OUT/${REL}.zip"
rm -f "$ZIP"
( cd "$B/release-staging" && find "$REL" -type f | sort | zip -X@ "$ZIP" )
ls -lh "$ZIP"

echo "=== NSIS setup ==="
VERSION="$VERSION" bash "$SRC/scripts/make-setup-1.14.101.sh"
ls -lh "$OUT/${REL}-setup.exe"

echo "=== latest/ ==="
rm -rf "$LATEST"
mkdir -p "$LATEST"
cp -f "$OUT/${REL}.zip" "$LATEST/"
cp -f "$OUT/${REL}-setup.exe" "$LATEST/"
# Standalone Fast Sync GUI tool = same qt binary, clear name for GitHub asset
cp -f "$B/release/dogecoin-qt.exe" "$LATEST/dogecoin-qt-fastsync.exe"

cat > "$LATEST/FASTSYNC-README.txt" <<'EOF'
Dogecoin Core Pro — dogecoin-qt-fastsync.exe
============================================

WHAT IT IS
----------
Standalone Windows GUI of Dogecoin Core Pro with Fast Sync enabled
(WinHTTP HTTPS CDN download + Settings → Fast Sync from CDN…).

Same full-node client as dogecoin-qt.exe inside the zip/installer:
same Dogecoin mainnet, same consensus, same DOGE.
Not a new coin. Not a hard fork. Not a separate network.

WHAT IT IS FOR
--------------
• Optional GitHub asset for testers who want a clearly named Fast Sync GUI
• Same binary lineage as the package dogecoin-qt.exe

WHAT IT IS NOT
--------------
• Not a light client — still a full node
• Not the CDN/GPENode server (that hosts snapshots; this is the client)
• Not required if you install the setup or use the zip (use dogecoin-qt.exe there)
• Not "trust the cloud forever" — fail-closed file hash + background P2P prove

HOW TO USE
----------
1. Prefer a NEW empty datadir for first Fast Sync tests.
2. Run dogecoin-qt-fastsync.exe  (or dogecoin-qt.exe from zip/setup).
3. Settings → Fast Sync from CDN…
CDN: https://sync.doge.gopastearth.com/latest.json

RECOMMENDED FOR MOST PEOPLE
---------------------------
• dogecoin-1.14.102-win64-setup.exe
• dogecoin-1.14.102-win64.zip

Pre-release. Review before large balances.
EOF

(
  cd "$LATEST"
  sha256sum "${REL}.zip" "${REL}-setup.exe" dogecoin-qt-fastsync.exe | tee SHA256SUMS.txt
)

echo "=== DONE: $LATEST ==="
ls -lah "$LATEST"
cat "$LATEST/SHA256SUMS.txt"
echo SHIP_LATEST_OK
