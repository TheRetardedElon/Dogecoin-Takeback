#!/usr/bin/env bash
# Reconfigure winbuild as MinGW cross (Windows PE) and full-build release binaries.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin

BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
HOST=x86_64-w64-mingw32
VERSION=1.14.102
REL=dogecoin-${VERSION}-win64
JOBS="${JOBS:-4}"

log() { echo "[release $(date +%H:%M:%S)] $*"; }

GXX=$(readlink -f "$(command -v ${HOST}-g++)")
log "g++=$GXX"
[[ "$GXX" == *posix* ]] || { log "need posix mingw g++"; exit 1; }
test -f "$BUILD/depends/${HOST}/share/config.site"

# --- copy Pro sources without clobbering winbuild configure/depends ---
log "Sync Pro GUI and node sources only"
for f in \
  src/clientversion.h \
  src/qt/dogecoin.cpp src/qt/dogecoingui.cpp src/qt/dogecoingui.h \
  src/qt/clientmodel.cpp src/qt/clientmodel.h \
  src/qt/networkpage.cpp src/qt/networkpage.h \
  src/qt/peermapwidget.cpp src/qt/peermapwidget.h \
  src/qt/memestreamclient.cpp src/qt/memestreamclient.h \
  src/qt/memestreamrail.cpp src/qt/memestreamrail.h \
  src/qt/memestreampage.cpp src/qt/memestreampage.h \
  src/qt/win_image_decode.cpp src/qt/win_image_decode.h \
  src/qt/arcadepage.cpp src/qt/arcadepage.h \
  src/qt/arcadegamewidget.cpp src/qt/arcadegamewidget.h \
  src/qt/dogebusinesspage.cpp src/qt/dogebusinesspage.h \
  src/qt/themeswitcher.cpp src/qt/themeswitcher.h \
  src/qt/thememanager.cpp src/qt/thememanager.h \
  src/qt/modaloverlay.cpp src/qt/modaloverlay.h \
  src/qt/optionsdialog.cpp src/qt/optionsdialog.h \
  src/qt/optionsmodel.cpp src/qt/optionsmodel.h \
  src/qt/intro.cpp src/qt/intro.h \
  src/qt/walletview.cpp src/qt/walletview.h \
  src/qt/walletframe.cpp src/qt/walletframe.h \
  src/node/chainstate.cpp src/node/chainstate.h \
  src/node/utxo_snapshot.cpp src/node/utxo_snapshot.h \
  src/node/snapshot_fetch.cpp src/node/snapshot_fetch.h \
  src/rpc/blockchain.cpp \
  src/ibdstats.cpp src/ibdstats.h \
  src/init.cpp src/validation.cpp src/net_processing.cpp \
  src/chainparams.cpp src/chainparams.h
do
  if [[ -f "$SRC/$f" ]]; then
    mkdir -p "$BUILD/$(dirname "$f")"
    cp -f "$SRC/$f" "$BUILD/$f"
  fi
done
mkdir -p "$BUILD/src/qt/forms"
cp -f "$SRC"/src/qt/forms/*.ui "$BUILD/src/qt/forms/" 2>/dev/null || true
# version stamp for configure (reconf if needed)
if [[ -f "$SRC/configure.ac" ]]; then
  cp -f "$SRC/configure.ac" "$BUILD/configure.ac"
fi

# --- reconfigure for Windows if not already mingw ---
cd "$BUILD"
NEED_RECONF=0
if [[ ! -f Makefile ]]; then NEED_RECONF=1; fi
if grep -q 'target os.*=.*linux' config.log 2>/dev/null; then NEED_RECONF=1; fi
if ! grep -q 'x86_64-w64-mingw32' src/config/dogecoin-config.h 2>/dev/null \
   && ! grep -q 'WIN32' src/config/dogecoin-config.h 2>/dev/null; then
  # config may still define WIN32 via compiler - check Makefile
  if ! grep -q 'x86_64-w64-mingw32-g++' src/Makefile 2>/dev/null; then
    NEED_RECONF=1
  fi
fi

if [[ "$NEED_RECONF" -eq 1 ]]; then
  log "Reconfigure for MinGW host=$HOST"
  if [[ -f Makefile ]]; then make distclean 2>/dev/null || true; fi
  if [[ ! -f configure ]]; then ./autogen.sh; fi
  CONFIG_SITE="$PWD/depends/${HOST}/share/config.site" ./configure \
    --prefix=/ \
    --host=${HOST} \
    --disable-ccache \
    --disable-maintainer-mode \
    --disable-dependency-tracking \
    --with-gui=qt5 \
    --enable-reduce-exports 2>&1 | tee /tmp/win-reconf.log | tail -50
  grep -E 'host system|target os|with gui|CXX' /tmp/win-reconf.log | head -20
  grep -q 'x86_64-w64-mingw32' src/Makefile || {
    log "FATAL: Makefile not using mingw"
    exit 1
  }
else
  log "Makefile already looks like MinGW cross — skip reconf"
fi

# --- clean PE outputs and force qt/server recompile ---
log "Clean PE targets and qt objects"
cd "$BUILD/src"
rm -f qt/dogecoin-qt.exe dogecoind.exe dogecoin-cli.exe dogecoin-tx.exe
rm -f qt/libdogecoinqt.a
find qt -name '*.o' -delete 2>/dev/null || true
find qt -name 'moc_*.cpp' -delete 2>/dev/null || true
find qt -name 'qrc_*.cpp' -delete 2>/dev/null || true
find qt/forms -name 'ui_*.h' -delete 2>/dev/null || true
# rebuild server libs that include assumeutxo
rm -f libdogecoin_server.a
find . -name 'libdogecoin_server_a-*.o' -delete 2>/dev/null || true
find node -name '*.o' -delete 2>/dev/null || true
find rpc -name 'libdogecoin_server_a-blockchain.o' -delete 2>/dev/null || true

log "Full make -j${JOBS}"
cd "$BUILD"
make -j"$JOBS" 2>&1 | tee /tmp/win-full-build.log | tail -100

# Targets may live under src/
cd "$BUILD/src"
if [[ ! -f dogecoind.exe ]]; then
  make -j"$JOBS" dogecoind.exe dogecoin-cli.exe dogecoin-tx.exe 2>&1 | tail -40
fi
if [[ ! -f qt/dogecoin-qt.exe ]]; then
  make -j"$JOBS" qt/dogecoin-qt.exe 2>&1 | tail -40
fi

# Arcade re-insert if AR dropped it
if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a 2>/dev/null | grep -q 'ArcadePageC1'; then
  log "Re-ar Arcade + Meme objects"
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
    qt/libdogecoinqt_a-arcadepage.o \
    qt/libdogecoinqt_a-arcadegamewidget.o \
    qt/libdogecoinqt_a-moc_arcadepage.o \
    qt/libdogecoinqt_a-moc_arcadegamewidget.o \
    qt/libdogecoinqt_a-memestreampage.o \
    qt/libdogecoinqt_a-memestreamrail.o \
    qt/libdogecoinqt_a-memestreamclient.o \
    qt/libdogecoinqt_a-moc_memestreampage.o \
    qt/libdogecoinqt_a-moc_memestreamrail.o \
    qt/libdogecoinqt_a-dogebusinesspage.o \
    qt/libdogecoinqt_a-peermapwidget.o \
    qt/libdogecoinqt_a-moc_peermapwidget.o 2>/dev/null || true
  x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe
fi

test -f dogecoind.exe
test -f dogecoin-cli.exe
test -f qt/dogecoin-qt.exe
file dogecoind.exe dogecoin-cli.exe dogecoin-tx.exe qt/dogecoin-qt.exe

log "Feature gate (nm to file — avoid pipefail SIGPIPE false fails)"
FAIL=0
NM_QT=$(mktemp); NM_LIB=$(mktemp); STR_D=$(mktemp)
x86_64-w64-mingw32-nm qt/dogecoin-qt.exe >"$NM_QT" 2>/dev/null || true
x86_64-w64-mingw32-nm qt/libdogecoinqt.a >"$NM_LIB" 2>/dev/null || true
strings dogecoind.exe >"$STR_D" 2>/dev/null || true
for s in \
  '_ZN11DogecoinGUI14gotoArcadePageEv' \
  '_ZN11DogecoinGUI18gotoMemeStreamPageEv' \
  'ArcadePage' \
  'MemeStreamPage' \
  'DogeBusinessPage'
do
  if grep -F "$s" "$NM_QT" >/dev/null 2>&1; then echo " OK nm $s"; else echo " FAIL nm $s"; FAIL=1; fi
done
if grep -E 'assumeutxo|loadtxoutset|dumptxoutset|fetchassumeutxo' "$STR_D" >/dev/null 2>&1; then
  echo " OK assumeutxo RPCs"
else
  echo " FAIL assumeutxo RPCs"; FAIL=1
fi
if grep -F 'fetchassumeutxomanifest' "$STR_D" >/dev/null 2>&1; then
  echo " OK fetchassumeutxomanifest"
else
  echo " WARN fetchassumeutxomanifest string missing (check blockchain.o link)"
fi
if grep -F 'sync.doge.gopastearth.com' "$STR_D" >/dev/null 2>&1; then
  echo " OK GPE CDN default host"
else
  echo " WARN GPE CDN host string missing"
fi
grep -F ArcadePage "$NM_LIB" >/dev/null && echo " OK ArcadePage lib" || { echo " FAIL ArcadePage lib"; FAIL=1; }
grep -F MemeStreamPage "$NM_LIB" >/dev/null && echo " OK MemeStreamPage lib" || { echo " FAIL MemeStreamPage lib"; FAIL=1; }
rm -f "$NM_QT" "$NM_LIB" "$STR_D"
[[ "$FAIL" -eq 0 ]] || { log "FATAL feature gate"; exit 1; }

log "Stage stripped release"
mkdir -p "$BUILD/release" "$OUT"
cp -f dogecoind.exe dogecoin-cli.exe "$BUILD/release/"
[[ -f dogecoin-tx.exe ]] && cp -f dogecoin-tx.exe "$BUILD/release/"
cp -f qt/dogecoin-qt.exe "$BUILD/release/"
x86_64-w64-mingw32-strip -s "$BUILD/release"/*.exe || true
date -u +"FULL_RELEASE %Y-%m-%dT%H:%M:%SZ" > "$BUILD/release/BUILD_STAMP.txt"
ls -la "$BUILD/release"/*.exe

log "Zip"
STAGE="$BUILD/release-staging/$REL"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/daemon"
cp -f "$BUILD/release/dogecoin-qt.exe" "$STAGE/"
cp -f "$BUILD/release/dogecoind.exe" "$STAGE/daemon/"
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/daemon/"
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/bin/"
[[ -f $BUILD/release/dogecoin-tx.exe ]] && cp -f "$BUILD/release/dogecoin-tx.exe" "$STAGE/bin/"
cp -f "$SRC/COPYING" "$STAGE/COPYING.txt" 2>/dev/null || true
[[ -f $SRC/doc/README_windows.txt ]] && cp -f "$SRC/doc/README_windows.txt" "$STAGE/readme.txt" || true
cp -f "$BUILD/release/BUILD_STAMP.txt" "$STAGE/"
cat > "$STAGE/CORE_PRO.txt" <<EOF
Dogecoin Core Pro ${VERSION} — FULL WIN64 RELEASE BUILD
Not smoke-run. Fresh MinGW cross-compile.
$(cat "$BUILD/release/BUILD_STAMP.txt")
https://github.com/TheRetardedElon/Dogecoin-Takeback
EOF

ZIP="$OUT/${REL}.zip"
rm -f "$ZIP"
( cd "$BUILD/release-staging" && find "$REL" -type f | sort | zip -X@ "$ZIP" )
log "ZIP $ZIP"

log "NSIS"
# NSIS installer (script name may lag version; pass VERSION via env)
VERSION="$VERSION" bash "$SRC/scripts/make-setup-1.14.101.sh" || \
  VERSION="$VERSION" bash "$SRC/scripts/make-setup-win64.sh" || true
if [[ ! -f "$OUT/${REL}-setup.exe" ]]; then
  log "NSIS helper missing setup — try package-core-pro-win64"
  VERSION="$VERSION" bash "$SRC/scripts/package-core-pro-win64.sh" 2>/dev/null || true
fi
(
  cd "$OUT"
  sha256sum "${REL}.zip" "${REL}-setup.exe" 2>/dev/null | tee SHA256SUMS-win64.txt
  cp -f "$SRC/release/RELEASE_NOTES_win64.md" RELEASE_NOTES_win64.md 2>/dev/null || true
)

ls -lah "$OUT"/${REL}*
echo FULL_RELEASE_OK
