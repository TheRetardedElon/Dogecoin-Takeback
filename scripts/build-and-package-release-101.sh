#!/usr/bin/env bash
# FULL Windows PE release build for Dogecoin Core Pro 1.14.101
# — Not smoke-run. Rebuilds daemon/cli/tx/qt from winbuild, then zip+NSIS.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin

BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
HOST=x86_64-w64-mingw32
VERSION=1.14.101
REL=dogecoin-${VERSION}-win64
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

log() { echo "[release $(date +%H:%M:%S)] $*"; }

# --- mingw must be posix ---
GXX=$(readlink -f "$(command -v ${HOST}-g++)")
log "g++=$GXX"
if [[ "$GXX" != *posix* ]]; then
  log "ERROR: need ${HOST}-g++-posix"
  exit 1
fi
test -f "$BUILD/depends/${HOST}/share/config.site"

# --- 1) Sync product sources into winbuild (never ui_*.h / qrc from Windows) ---
log "=== Sync sources (C: -> winbuild) ==="
# Core node + RPC + chainstate
rsync -a --delete \
  --exclude='.git' \
  --exclude='*.o' --exclude='*.a' --exclude='*.lo' --exclude='.deps' \
  --exclude='html' --exclude='smoke-run' --exclude='release' \
  --exclude='src/qt/forms/ui_*.h' \
  --exclude='src/qt/qrc_*.cpp' \
  --exclude='src/qt/moc_*.cpp' \
  --exclude='src/qt/dogecoin-qt.exe' \
  --exclude='src/dogecoind.exe' \
  --exclude='src/dogecoin-cli.exe' \
  --exclude='src/dogecoin-tx.exe' \
  "$SRC/" "$BUILD/" 2>/dev/null || {
  log "rsync limited — force-copy critical trees"
  for d in src/qt src/node src/rpc src/wallet src/consensus; do
    mkdir -p "$BUILD/$d"
    # copy sources only
    find "$SRC/$d" -maxdepth 1 -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.ui' -o -name '*.qrc' \) \
      -exec cp -f {} "$BUILD/$d/" \; 2>/dev/null || true
  done
  # recursive for forms .ui only
  mkdir -p "$BUILD/src/qt/forms"
  cp -f "$SRC"/src/qt/forms/*.ui "$BUILD/src/qt/forms/" 2>/dev/null || true
  cp -f "$SRC"/src/*.cpp "$SRC"/src/*.h "$BUILD/src/" 2>/dev/null || true
}

# Force known Pro GUI files
for f in \
  qt/dogecoin.cpp qt/dogecoingui.cpp qt/dogecoingui.h \
  qt/clientmodel.cpp qt/clientmodel.h \
  qt/networkpage.cpp qt/networkpage.h \
  qt/peermapwidget.cpp qt/peermapwidget.h \
  qt/memestreamclient.cpp qt/memestreamclient.h \
  qt/memestreamrail.cpp qt/memestreamrail.h \
  qt/memestreampage.cpp qt/memestreampage.h \
  qt/arcadepage.cpp qt/arcadepage.h \
  qt/arcadegamewidget.cpp qt/arcadegamewidget.h \
  qt/dogebusinesspage.cpp qt/dogebusinesspage.h \
  qt/themeswitcher.cpp qt/themeswitcher.h \
  qt/thememanager.cpp qt/thememanager.h \
  qt/modaloverlay.cpp qt/modaloverlay.h \
  qt/optionsdialog.cpp qt/optionsdialog.h \
  qt/walletview.cpp qt/walletview.h \
  node/chainstate.cpp node/chainstate.h \
  node/utxo_snapshot.cpp node/utxo_snapshot.h \
  rpc/blockchain.cpp \
  ibdstats.cpp ibdstats.h
do
  if [[ -f "$SRC/src/$f" ]]; then
    mkdir -p "$BUILD/src/$(dirname "$f")"
    cp -f "$SRC/src/$f" "$BUILD/src/$f"
  fi
done

# --- 2) Nuke PE outputs + Qt objects so we cannot ship a smoke-era binary ---
log "=== Clean PE + qt objects (full relink) ==="
cd "$BUILD/src"
rm -f qt/dogecoin-qt.exe dogecoind.exe dogecoin-cli.exe dogecoin-tx.exe
rm -f qt/libdogecoinqt.a
# Drop stale moc/qrc/ui that may be wrong Qt version
find qt -name 'moc_*.cpp' -delete 2>/dev/null || true
find qt -name 'qrc_*.cpp' -delete 2>/dev/null || true
find qt/forms -name 'ui_*.h' -delete 2>/dev/null || true
# Object files for qt — force recompile of Pro surface
find qt -name '*.o' -delete 2>/dev/null || true
# Also recompile key node objects used by assumeutxo/ibd
rm -f libdogecoinqt_a-*.o 2>/dev/null || true
rm -f node/*.o rpc/libdogecoin_server_a-blockchain.o 2>/dev/null || true
rm -f libdogecoin_server_a-*.o libdogecoin_server.a 2>/dev/null || true
# safer: make clean in src is huge; selective:
rm -f \
  libdogecoin_server.a libdogecoin_wallet.a libdogecoin_common.a \
  libdogecoin_util.a libdogecoin_cli.a 2>/dev/null || true
# object dirs for server
find . -name 'libdogecoin_server_a-*.o' -delete 2>/dev/null || true
find node -name '*.o' -delete 2>/dev/null || true
find rpc -name '*.o' -delete 2>/dev/null || true

# --- 3) Full make of all PE targets ---
log "=== make -j${JOBS} (dogecoind dogecoin-cli dogecoin-tx dogecoin-qt) ==="
cd "$BUILD"
# ensure configured
if [[ ! -f Makefile ]]; then
  log "No Makefile — reconfigure"
  CONFIG_SITE="$PWD/depends/${HOST}/share/config.site" ./configure \
    --prefix=/ \
    --disable-ccache \
    --disable-maintainer-mode \
    --disable-dependency-tracking \
    --with-gui=qt5 \
    --enable-reduce-exports
fi

make -j"$JOBS" \
  src/dogecoind.exe \
  src/dogecoin-cli.exe \
  src/dogecoin-tx.exe \
  src/qt/dogecoin-qt.exe 2>&1 | tee /tmp/release-build-101.log | tail -80

# If arcade dropped from archive during AR, re-insert and relink qt only
if ! x86_64-w64-mingw32-nm src/qt/libdogecoinqt.a 2>/dev/null | grep -q 'ArcadePageC1'; then
  log "Re-insert Arcade objects into libdogecoinqt.a"
  cd src
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-arcadepage.o \
    qt/libdogecoinqt_a-arcadegamewidget.o \
    qt/libdogecoinqt_a-moc_arcadepage.o \
    qt/libdogecoinqt_a-moc_arcadegamewidget.o \
    qt/libdogecoinqt_a-peermapwidget.o \
    qt/libdogecoinqt_a-moc_peermapwidget.o \
    qt/libdogecoinqt_a-memestreampage.o \
    qt/libdogecoinqt_a-memestreamrail.o \
    qt/libdogecoinqt_a-memestreamclient.o \
    qt/libdogecoinqt_a-dogebusinesspage.o 2>/dev/null || true
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe
  cd ..
fi

test -f src/qt/dogecoin-qt.exe
test -f src/dogecoind.exe
test -f src/dogecoin-cli.exe
file src/qt/dogecoin-qt.exe src/dogecoind.exe src/dogecoin-cli.exe src/dogecoin-tx.exe

# --- 4) Hard feature gate — fail package if Pro UI missing ---
log "=== Feature gate ==="
QT=src/qt/dogecoin-qt.exe
FAIL=0
for s in "Meme Stream" "Doge Business" "gotoArcadePage" "gotoMemeStreamPage" "assumeutxo" "getibdinfo"; do
  if strings "$QT" | grep -Fq "$s"; then
    echo "  OK  $s"
  else
    echo "  FAIL $s"
    FAIL=1
  fi
done
if ! x86_64-w64-mingw32-nm src/qt/libdogecoinqt.a | grep -q 'ArcadePageC1'; then
  echo "  FAIL ArcadePage symbol"
  FAIL=1
else
  echo "  OK  ArcadePage symbol"
fi
if ! x86_64-w64-mingw32-nm src/qt/libdogecoinqt.a | grep -q 'MemeStreamPageC1'; then
  echo "  FAIL MemeStreamPage symbol"
  FAIL=1
else
  echo "  OK  MemeStreamPage symbol"
fi
if [[ "$FAIL" -ne 0 ]]; then
  log "FATAL: release PE missing Pro features — not packaging"
  exit 1
fi

# --- 5) Stage stripped release/ (NOT smoke-run) ---
log "=== Stage stripped $BUILD/release ==="
mkdir -p "$BUILD/release" "$OUT"
for f in dogecoind.exe dogecoin-cli.exe dogecoin-tx.exe; do
  if [[ -f src/$f ]]; then
    cp -f "src/$f" "$BUILD/release/$f"
    ${HOST}-strip -s "$BUILD/release/$f" || true
  fi
done
cp -f src/qt/dogecoin-qt.exe "$BUILD/release/dogecoin-qt.exe"
${HOST}-strip -s "$BUILD/release/dogecoin-qt.exe" || true
ls -la "$BUILD/release"/*.exe

# Stamp that this is a release build (not smoke)
date -u +"%Y-%m-%dT%H:%M:%SZ" > "$BUILD/release/BUILD_UTC.txt"
echo "full-release-build ${VERSION}" >> "$BUILD/release/BUILD_UTC.txt"
strings src/qt/dogecoin-qt.exe | grep -F 'v1.14.101' | head -1 || true

# --- 6) Zip + NSIS ---
log "=== Portable zip ==="
STAGE="$BUILD/release-staging/$REL"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/daemon"
cp -f "$BUILD/release/dogecoin-qt.exe" "$STAGE/"
cp -f "$BUILD/release/dogecoind.exe" "$STAGE/daemon/"
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/daemon/"
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/bin/"
[[ -f $BUILD/release/dogecoin-tx.exe ]] && cp -f "$BUILD/release/dogecoin-tx.exe" "$STAGE/bin/"
cp -f "$SRC/COPYING" "$STAGE/COPYING.txt" 2>/dev/null || true
[[ -f $SRC/doc/README_windows.txt ]] && cp -f "$SRC/doc/README_windows.txt" "$STAGE/readme.txt" || cp -f "$SRC/README.md" "$STAGE/readme.txt"
cat > "$STAGE/CORE_PRO.txt" <<EOF
Dogecoin Core Pro / Takeback ${VERSION}
FULL RELEASE BUILD (not smoke-run)
Built: $(date -u +%Y-%m-%dT%H:%M:%SZ)
https://github.com/TheRetardedElon/Dogecoin-Takeback

GUI: Home, Send, Receive, Transactions, Network, Doge Business,
     Meme Stream, Arcade, Console + themes
Node: getibdinfo, AssumeUTXO dual-chainstate RPCs
EOF

ZIP="$OUT/${REL}.zip"
rm -f "$ZIP"
( cd "$BUILD/release-staging" && find "$REL" -type f | sort | zip -X@ "$ZIP" )
log "ZIP $ZIP ($(du -h "$ZIP" | awk '{print $1}'))"

log "=== NSIS setup ==="
bash "$SRC/scripts/make-setup-1.14.101.sh"

(
  cd "$OUT"
  sha256sum "${REL}.zip" "${REL}-setup.exe" 2>/dev/null | tee SHA256SUMS-win64.txt
)

cat > "$OUT/RELEASE_NOTES_win64.md" <<EOF
# Dogecoin Core Pro ${VERSION} — Windows x64 **FULL RELEASE**

Built from a complete winbuild remake of daemon + CLI + Qt (not a smoke-run copy).

## Artifacts
- \`${REL}-setup.exe\` — NSIS installer
- \`${REL}.zip\` — portable
- \`SHA256SUMS-win64.txt\`

## Includes
- dogecoin-qt (Core Pro: Business, Meme Stream, Network, Arcade)
- dogecoind / dogecoin-cli / dogecoin-tx
- IBD telemetry + AssumeUTXO RPCs

## Install
Run the setup.exe, or unzip and launch dogecoin-qt.exe.

Pre-release: review before high-value use.
EOF

log "=== DONE ==="
ls -lah "$OUT"/${REL}*
echo "FULL_RELEASE_PACKAGE_OK"
