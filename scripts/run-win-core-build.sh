#!/usr/bin/env bash
# Configure + build Dogecoin Core for Windows using completed depends prefix.
set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export JOBS="${JOBS:-2}"

SRC_WIN="/mnt/c/dogedev"
BUILD_ROOT="${BUILD_ROOT:-$HOME/dogedev-winbuild}"
HOST="x86_64-w64-mingw32"
LOG=/tmp/win-core-build.log

# Disable WSL PE interop for configure tests
if [[ -w /proc/sys/fs/binfmt_misc/status ]]; then
  echo 0 > /proc/sys/fs/binfmt_misc/status || true
fi

test -f "${BUILD_ROOT}/depends/${HOST}/share/config.site"

# Sync latest Pro sources (keep depends prefix)
echo "[win-core] syncing sources (excluding depends work/built)" | tee "$LOG"
rsync -a \
  --exclude='.git' \
  --exclude='html' \
  --exclude='release' \
  --exclude='depends/work' \
  --exclude='depends/built' \
  --exclude='depends/sources' \
  --exclude='depends/x86_64-w64-mingw32' \
  --exclude='*.o' --exclude='*.lo' --exclude='*.a' --exclude='.deps' \
  --exclude='src/qt/dogecoin-qt' \
  --exclude='src/dogecoind' \
  --exclude='**/moc_*.cpp' \
  --exclude='**/ui_*.h' \
  --exclude='**/qrc_*.cpp' \
  "${SRC_WIN}/" "${BUILD_ROOT}/"
# Drop stale host moc/uic artifacts under qt/ only (never delete src/ui_interface.h)
find "${BUILD_ROOT}/src/qt" -name 'moc_*.cpp' -delete 2>/dev/null || true
find "${BUILD_ROOT}/src/qt" -name 'ui_*.h' -delete 2>/dev/null || true
find "${BUILD_ROOT}/src/qt" -name 'qrc_*.cpp' -delete 2>/dev/null || true

# Keep depends Makefile + package patches we fixed
cp -f "${SRC_WIN}/depends/Makefile" "${BUILD_ROOT}/depends/Makefile" 2>/dev/null || true
cp -f "${SRC_WIN}/depends/packages/"*.mk "${BUILD_ROOT}/depends/packages/" 2>/dev/null || true

cd "${BUILD_ROOT}"
if [[ ! -f configure ]]; then
  echo "[win-core] autogen.sh" | tee -a "$LOG"
  ./autogen.sh 2>&1 | tee -a "$LOG"
fi

echo "[win-core] configure" | tee -a "$LOG"
CONFIG_SITE="$PWD/depends/${HOST}/share/config.site" ./configure \
  --prefix=/ \
  --disable-ccache \
  --disable-maintainer-mode \
  --disable-dependency-tracking \
  --with-gui=qt5 \
  --enable-reduce-exports \
  2>&1 | tee -a "$LOG"

echo "[win-core] make -j${JOBS}" | tee -a "$LOG"
make -j"${JOBS}" 2>&1 | tee -a "$LOG"

echo "[win-core] PE binaries:" | tee -a "$LOG"
file src/qt/dogecoin-qt.exe src/dogecoind.exe src/dogecoin-cli.exe src/dogecoin-tx.exe | tee -a "$LOG"
ls -lah src/qt/dogecoin-qt.exe src/dogecoind.exe src/dogecoin-cli.exe src/dogecoin-tx.exe | tee -a "$LOG"
echo "[win-core] BUILD OK" | tee -a "$LOG"
