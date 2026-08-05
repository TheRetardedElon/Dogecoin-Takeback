#!/usr/bin/env bash
# Cross-compile Dogecoin Core Pro for Windows (x86_64) via WSL2 + depends + MinGW.
# Produces zip + NSIS installer under release/ for GitHub release upload.
set -euo pipefail

# WSL imports Windows PATH; entries with spaces/parentheses break depends shell recipes.
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

SRC_WIN="/mnt/c/dogedev"
BUILD_ROOT="${BUILD_ROOT:-$HOME/dogedev-winbuild}"
HOST="x86_64-w64-mingw32"
JOBS="${JOBS:-2}"
STAGE="${1:-all}"  # all | sync | depends | build | package

log() { echo "[win-build $(date +%H:%M:%S)] $*"; }

need_posix_mingw() {
  local gxx
  gxx="$(readlink -f "$(command -v x86_64-w64-mingw32-g++)")"
  if [[ "$gxx" != *posix* ]]; then
    log "ERROR: mingw g++ is not posix ($gxx). Run as root:"
    log "  update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix"
    log "  update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix"
    exit 1
  fi
  log "mingw: $gxx"
}

sync_tree() {
  log "Syncing $SRC_WIN -> $BUILD_ROOT"
  mkdir -p "$BUILD_ROOT"
  rsync -a \
    --exclude='.git' \
    --exclude='html' \
    --exclude='node_modules' \
    --exclude='release' \
    --exclude='*.o' \
    --exclude='*.lo' \
    --exclude='*.a' \
    --exclude='.deps' \
    --exclude='src/qt/dogecoin-qt' \
    --exclude='src/dogecoind' \
    --exclude='src/dogecoin-cli' \
    --exclude='src/dogecoin-tx' \
    --exclude='src/qt/test/test_dogecoin-qt' \
    --exclude='src/test/test_dogecoin' \
    --exclude='src/bench/bench_dogecoin' \
    "$SRC_WIN/" "$BUILD_ROOT/"
  # Keep a clean depends work area if present but preserve built prefix if any
  du -sh "$BUILD_ROOT"
  log "Sync complete"
}

build_depends() {
  need_posix_mingw
  cd "$BUILD_ROOT/depends"
  log "Building depends for HOST=$HOST (jobs=$JOBS) — this can take a long time"
  make HOST="$HOST" -j"$JOBS"
  test -f "$BUILD_ROOT/depends/$HOST/share/config.site"
  log "Depends ready: depends/$HOST"
}

build_core() {
  need_posix_mingw
  cd "$BUILD_ROOT"
  if [[ ! -f configure || ! -f Makefile.in ]]; then
    log "Running autogen.sh"
    ./autogen.sh
  fi
  log "Configuring for Windows cross-build"
  CONFIG_SITE="$PWD/depends/$HOST/share/config.site" ./configure \
    --prefix=/ \
    --disable-ccache \
    --disable-maintainer-mode \
    --disable-dependency-tracking \
    --with-gui=qt5 \
    --enable-reduce-exports
  log "Building dogecoin binaries (jobs=$JOBS)"
  make -j"$JOBS"
  # Ensure PE binaries exist
  file src/qt/dogecoin-qt.exe src/dogecoind.exe src/dogecoin-cli.exe src/dogecoin-tx.exe
  log "Build complete"
}

package_release() {
  cd "$BUILD_ROOT"
  log "Creating release layout"
  make deploy || true

  # Strip + install layout similar to gitian-win
  VERSION="$(grep -E '^#define CLIENT_VERSION_(MAJOR|MINOR|REVISION)' src/clientversion.h 2>/dev/null || true)"
  # Prefer configure-derived version from setup.nsi if present
  REL_NAME="dogecoin-1.14.101-win64"
  if [[ -f share/setup.nsi ]]; then
    # OutFile may encode version; still use stable naming for Takeback/Pro
    :
  fi

  OUT_WIN="$SRC_WIN/release"
  mkdir -p "$OUT_WIN" "$BUILD_ROOT/release-staging/${REL_NAME}"

  # Prefer make install DESTDIR layout
  INSTALLPATH="$BUILD_ROOT/release-staging/${REL_NAME}"
  rm -rf "$INSTALLPATH"
  mkdir -p "$INSTALLPATH"
  make install DESTDIR="$INSTALLPATH"

  # Flatten expected zip structure: bin/, share/, etc.
  # make install with --prefix=/ puts things under INSTALLPATH/bin
  if [[ -d "$INSTALLPATH/bin" ]]; then
    # Strip PE binaries
    for f in "$INSTALLPATH"/bin/*.exe; do
      [[ -f "$f" ]] || continue
      x86_64-w64-mingw32-strip -s "$f" || true
    done
  fi

  # Copy any DLLs that install put in lib/ into bin for portable zip
  if [[ -d "$INSTALLPATH/lib" ]]; then
    find "$INSTALLPATH/lib" -name '*.dll' -exec cp -f {} "$INSTALLPATH/bin/" \; 2>/dev/null || true
  fi

  # Also copy top-level release/*.exe if make deploy put them there
  if ls "$BUILD_ROOT"/release/*.exe >/dev/null 2>&1; then
    mkdir -p "$INSTALLPATH/bin"
    cp -f "$BUILD_ROOT"/release/*.exe "$INSTALLPATH/bin/" || true
  fi

  # Ensure core binaries present at zip root-ish structure used by official releases
  # Official dogecoin-*-win64.zip has: dogecoin-qt.exe, daemon/dogecoind.exe, etc via install
  cd "$BUILD_ROOT/release-staging"
  ZIP_PATH="$OUT_WIN/${REL_NAME}.zip"
  rm -f "$ZIP_PATH"
  (cd "$REL_NAME" && find . -type f | sort | zip -X@ "$ZIP_PATH")
  log "Wrote $ZIP_PATH ($(du -h "$ZIP_PATH" | awk '{print $1}'))"

  # NSIS installer if make deploy produced it, or run makensis
  SETUP_SRC=""
  if ls "$BUILD_ROOT"/dogecoin-*-setup*.exe >/dev/null 2>&1; then
    SETUP_SRC=$(ls "$BUILD_ROOT"/dogecoin-*-setup*.exe | head -1)
  elif ls "$BUILD_ROOT"/release/dogecoin-*-setup*.exe >/dev/null 2>&1; then
    SETUP_SRC=$(ls "$BUILD_ROOT"/release/dogecoin-*-setup*.exe | head -1)
  elif [[ -f "$BUILD_ROOT/share/setup.nsi" ]] && command -v makensis >/dev/null; then
    # make deploy prepares release/ and runs nsis; try again
    mkdir -p "$BUILD_ROOT/release"
    for b in dogecoin-qt dogecoind dogecoin-cli dogecoin-tx; do
      if [[ -f "$BUILD_ROOT/src/qt/${b}.exe" ]]; then
        cp -f "$BUILD_ROOT/src/qt/${b}.exe" "$BUILD_ROOT/release/"
      elif [[ -f "$BUILD_ROOT/src/${b}.exe" ]]; then
        cp -f "$BUILD_ROOT/src/${b}.exe" "$BUILD_ROOT/release/"
      fi
    done
    # dogecoin-qt is under src/qt
    [[ -f "$BUILD_ROOT/src/qt/dogecoin-qt.exe" ]] && cp -f "$BUILD_ROOT/src/qt/dogecoin-qt.exe" "$BUILD_ROOT/release/"
    for b in dogecoind dogecoin-cli dogecoin-tx; do
      [[ -f "$BUILD_ROOT/src/${b}.exe" ]] && cp -f "$BUILD_ROOT/src/${b}.exe" "$BUILD_ROOT/release/"
    done
    makensis -V2 "$BUILD_ROOT/share/setup.nsi" || true
    if ls "$BUILD_ROOT"/dogecoin-*-setup*.exe >/dev/null 2>&1; then
      SETUP_SRC=$(ls "$BUILD_ROOT"/dogecoin-*-setup*.exe | head -1)
    fi
  fi

  if [[ -n "$SETUP_SRC" && -f "$SETUP_SRC" ]]; then
    SETUP_DST="$OUT_WIN/dogecoin-1.14.101-win64-setup.exe"
    cp -f "$SETUP_SRC" "$SETUP_DST"
    log "Wrote $SETUP_DST ($(du -h "$SETUP_DST" | awk '{print $1}'))"
  else
    log "WARN: NSIS setup.exe not produced; zip is still usable"
  fi

  # SHA256 sums for release notes
  (
    cd "$OUT_WIN"
    sha256sum dogecoin-1.14.101-win64.zip dogecoin-1.14.101-win64-setup.exe 2>/dev/null \
      | tee SHA256SUMS-win64.txt || true
  )

  # Release notes snippet
  cat > "$OUT_WIN/RELEASE_NOTES_win64.md" <<'EOF'
# Dogecoin Core Pro / Takeback — Windows x64

## Artifacts
- `dogecoin-1.14.101-win64.zip` — portable binaries
- `dogecoin-1.14.101-win64-setup.exe` — NSIS installer (if present)

## Version
- Client: **1.14.101** (Pro / Takeback tree)
- Target: Windows 64-bit (x86_64)
- Built via WSL2 cross-compile (depends + MinGW-w64 posix)

## Install
### Installer
1. Download `dogecoin-1.14.101-win64-setup.exe`
2. Run and follow the wizard
3. Launch **Dogecoin Core** from Start Menu

### Portable zip
1. Extract `dogecoin-1.14.101-win64.zip`
2. Run `bin/dogecoin-qt.exe` (or the packaged layout path)

## Notes
- First sync still downloads the full Dogecoin chain (or use pruned mode).
- Windows SmartScreen may warn on unsigned builds — expected until code-signed.
- Pure DOGE client (no EVM). Pro features: Home, Meme Stream, Business, Network, Options.

## Verify
```
sha256sum -c SHA256SUMS-win64.txt
```
EOF

  log "Artifacts in $OUT_WIN:"
  ls -lah "$OUT_WIN"
}

case "$STAGE" in
  sync) sync_tree ;;
  depends) build_depends ;;
  build) build_core ;;
  package) package_release ;;
  all)
    sync_tree
    build_depends
    build_core
    package_release
    ;;
  *)
    echo "Usage: $0 [all|sync|depends|build|package]"
    exit 2
    ;;
esac

log "Stage '$STAGE' finished OK"
