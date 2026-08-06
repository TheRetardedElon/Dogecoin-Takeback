#!/usr/bin/env bash
# Stage latest winbuild PE binaries, verify Arcade/Meme Stream, zip + NSIS setup.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
VERSION=1.14.101
REL=dogecoin-${VERSION}-win64
HOST=x86_64-w64-mingw32

log() { echo "[package $(date +%H:%M:%S)] $*"; }

test -f "$BUILD/src/qt/dogecoin-qt.exe"
test -f "$BUILD/src/dogecoind.exe"
test -f "$BUILD/src/dogecoin-cli.exe"

log "=== Feature check (dogecoin-qt) ==="
QT="$BUILD/src/qt/dogecoin-qt.exe"
for s in "Meme Stream" "Doge Business" "Arcade" "PeerMap" "assumeutxo" "Syncing headers first"; do
  if strings "$QT" | grep -Fq "$s"; then
    echo "  OK  $s"
  else
    echo "  MISS $s"
  fi
done
if ! x86_64-w64-mingw32-nm "$BUILD/src/qt/libdogecoinqt.a" 2>/dev/null | grep -q 'ArcadePageC1'; then
  log "WARN: ArcadePage missing from libdogecoinqt.a — attempting re-insert"
  if [[ -f $BUILD/src/qt/libdogecoinqt_a-arcadepage.o ]]; then
    x86_64-w64-mingw32-ar r "$BUILD/src/qt/libdogecoinqt.a" \
      "$BUILD/src/qt/libdogecoinqt_a-arcadepage.o" \
      "$BUILD/src/qt/libdogecoinqt_a-arcadegamewidget.o" \
      "$BUILD/src/qt/libdogecoinqt_a-moc_arcadepage.o" \
      "$BUILD/src/qt/libdogecoinqt_a-moc_arcadegamewidget.o"
    x86_64-w64-mingw32-ranlib "$BUILD/src/qt/libdogecoinqt.a"
    (cd "$BUILD/src" && rm -f qt/dogecoin-qt.exe && make qt/dogecoin-qt.exe)
  fi
fi

# dogecoin-tx optional
if [[ ! -f $BUILD/src/dogecoin-tx.exe ]]; then
  log "Building dogecoin-tx.exe"
  (cd "$BUILD/src" && make dogecoin-tx.exe -j"$(nproc 2>/dev/null || echo 2)" || true)
fi

log "=== Stage stripped release/ ==="
mkdir -p "$BUILD/release" "$OUT"
for src in \
  "$BUILD/src/qt/dogecoin-qt.exe" \
  "$BUILD/src/dogecoind.exe" \
  "$BUILD/src/dogecoin-cli.exe"
do
  base=$(basename "$src")
  cp -f "$src" "$BUILD/release/$base"
  ${HOST}-strip -s "$BUILD/release/$base" || true
  ls -la "$BUILD/release/$base"
done
if [[ -f $BUILD/src/dogecoin-tx.exe ]]; then
  cp -f "$BUILD/src/dogecoin-tx.exe" "$BUILD/release/dogecoin-tx.exe"
  ${HOST}-strip -s "$BUILD/release/dogecoin-tx.exe" || true
fi

# Also refresh smoke-run (best-effort; may fail if GUI holds lock)
log "=== smoke-run copy (best effort) ==="
cp -f "$BUILD/release/dogecoin-qt.exe" "$SRC/smoke-run/dogecoin-qt.exe" 2>/dev/null || log "smoke-run qt locked (client running?) — skip"
cp -f "$BUILD/release/dogecoind.exe" "$SRC/smoke-run/dogecoind.exe" 2>/dev/null || true
cp -f "$BUILD/release/dogecoin-cli.exe" "$SRC/smoke-run/dogecoin-cli.exe" 2>/dev/null || true

log "=== Portable zip ==="
STAGE="$BUILD/release-staging/$REL"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/daemon" "$STAGE/doc"
cp -f "$BUILD/release/dogecoin-qt.exe" "$STAGE/"
cp -f "$BUILD/release/dogecoind.exe" "$STAGE/daemon/"
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/daemon/"
if [[ -f $BUILD/release/dogecoin-tx.exe ]]; then
  cp -f "$BUILD/release/dogecoin-tx.exe" "$STAGE/bin/"
fi
# also put cli in bin for classic layout
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/bin/"
cp -f "$SRC/COPYING" "$STAGE/COPYING.txt" 2>/dev/null || true
if [[ -f $SRC/doc/README_windows.txt ]]; then
  cp -f "$SRC/doc/README_windows.txt" "$STAGE/readme.txt"
else
  cp -f "$SRC/README.md" "$STAGE/readme.txt"
fi
# Pro release blurb
cat > "$STAGE/CORE_PRO.txt" <<EOF
Dogecoin Core Pro / Takeback ${VERSION}
https://github.com/TheRetardedElon/Dogecoin-Takeback

Includes: Core Pro shell (Business, Meme Stream, Network, Arcade),
IBD/P2P upgrades (getibdinfo), AssumeUTXO dual-chainstate RPCs.

Pre-release: use at your own risk for mining/merchant float.
EOF

ZIP_PATH="$OUT/${REL}.zip"
rm -f "$ZIP_PATH"
(
  cd "$BUILD/release-staging"
  find "$REL" -type f | sort | zip -X@ "$ZIP_PATH"
)
log "ZIP: $ZIP_PATH ($(du -h "$ZIP_PATH" | awk '{print $1}'))"

log "=== NSIS installer ==="
bash "$SRC/scripts/make-setup-1.14.101.sh"

# Checksums + notes
(
  cd "$OUT"
  sha256sum ${REL}.zip ${REL}-setup.exe 2>/dev/null | tee SHA256SUMS-win64.txt || \
    sha256sum ${REL}.zip | tee SHA256SUMS-win64.txt
)

cat > "$OUT/RELEASE_NOTES_win64.md" <<EOF
# Dogecoin Core Pro ${VERSION} — Windows x64

## Artifacts
- \`${REL}.zip\` — portable
- \`${REL}-setup.exe\` — NSIS installer
- \`SHA256SUMS-win64.txt\`

## What's inside
- **dogecoin-qt.exe** — Core Pro GUI (Home, Business, Meme Stream, Network, Arcade, themes)
- **daemon/dogecoind.exe** — full node
- **bin/dogecoin-cli.exe** / **daemon/dogecoin-cli.exe**
- Optional **bin/dogecoin-tx.exe**

## Features (Pro / Takeback)
- Pure DOGE settlement (no EVM wrap product path)
- IBD telemetry (\`getibdinfo\`), stall rescue, parallel download, ASMAP
- AssumeUTXO dual chainstate RPCs (regtest proven; mainnet map empty until attested)
- Doge Business invoices/POS, Meme Stream tips on-chain, Arcade mini-game

## Build
- Cross-compiled via WSL2 + MinGW + depends (Qt 5.7.1)
- Strip applied to release copies

## Smoke
\`\`\`powershell
powershell -ExecutionPolicy Bypass -File scripts\\smoke-assumeutxo-regtest.ps1
powershell -ExecutionPolicy Bypass -File scripts\\smoke-assumeutxo-two-node.ps1
\`\`\`

## Install
Run \`${REL}-setup.exe\` or unzip portable and launch \`dogecoin-qt.exe\`.

**Pre-release** — review before high-value use.
EOF

log "=== OUT ==="
ls -lah "$OUT"
echo PACKAGE_OK
