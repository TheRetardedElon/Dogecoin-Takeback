#!/usr/bin/env bash
# Reliable driver for Windows depends build on WSL Ubuntu 26.04.
set -euo pipefail

# Do NOT put a host gcc ahead of PATH as "gcc" — Qt's win32 qmake will
# wrongly use it for target objects (e.g. pcre + -fno-keep-inline-dllexport).
# Boost jam is patched to call gcc-11 explicitly.
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
export JOBS="${JOBS:-2}"

SRC_WIN="/mnt/c/dogedev"
BUILD_ROOT="${BUILD_ROOT:-$HOME/dogedev-winbuild}"
HOST="x86_64-w64-mingw32"
LOG="${LOG:-/tmp/win-depends.log}"

# Ensure posix mingw
if ! readlink -f "$(command -v x86_64-w64-mingw32-g++)" | grep -q posix; then
  echo "ERROR: mingw g++ is not posix" >&2
  exit 1
fi

# WSL tries to execute PE cross-test binaries via Win32 interop → break configure.
if [[ -w /proc/sys/fs/binfmt_misc/status ]]; then
  echo 0 > /proc/sys/fs/binfmt_misc/status || true
elif command -v sudo >/dev/null 2>&1; then
  sudo -n bash -c 'echo 0 > /proc/sys/fs/binfmt_misc/status' 2>/dev/null || true
fi
# Prefer root helper when available
if [[ "$(id -u)" -eq 0 ]]; then
  echo 0 > /proc/sys/fs/binfmt_misc/status || true
fi

# Sync critical depends files from Windows tree
mkdir -p "${BUILD_ROOT}/depends/packages" \
         "${BUILD_ROOT}/depends/patches/boost" \
         "${BUILD_ROOT}/depends/patches/libevent" \
         "${BUILD_ROOT}/depends/patches/qt" \
         "${BUILD_ROOT}/scripts"
cp -f "${SRC_WIN}/depends/Makefile" "${BUILD_ROOT}/depends/Makefile"
cp -f "${SRC_WIN}/depends/packages/"*.mk "${BUILD_ROOT}/depends/packages/"
cp -f "${SRC_WIN}/depends/patches/libevent/"* "${BUILD_ROOT}/depends/patches/libevent/" 2>/dev/null || true
cp -f "${SRC_WIN}/depends/patches/qt/"* "${BUILD_ROOT}/depends/patches/qt/" 2>/dev/null || true
cp -f "${SRC_WIN}/scripts/patch-boost-jam-for-modern-gcc.py" "${BUILD_ROOT}/scripts/"

# Point boost preprocess at local script copy
sed -i "s|python3 /mnt/c/dogedev/scripts/patch-boost-jam-for-modern-gcc.py|python3 ${BUILD_ROOT}/scripts/patch-boost-jam-for-modern-gcc.py|" \
  "${BUILD_ROOT}/depends/packages/boost.mk"

# Wipe incomplete Qt worktree so it rebuilds with correct cross CC
rm -rf "${BUILD_ROOT}/depends/work/build/${HOST}/qt"
rm -rf "${BUILD_ROOT}/depends/built/${HOST}/qt"

echo "[run-win-depends] starting make HOST=${HOST} -j${JOBS}" | tee "${LOG}"
cd "${BUILD_ROOT}/depends"
make HOST="${HOST}" -j"${JOBS}" 2>&1 | tee -a "${LOG}"
test -f "${BUILD_ROOT}/depends/${HOST}/share/config.site"
echo "[run-win-depends] SUCCESS config.site ready" | tee -a "${LOG}"

# Re-enable Win32 interop if we disabled it as root
if [[ "$(id -u)" -eq 0 && -w /proc/sys/fs/binfmt_misc/status ]]; then
  echo 1 > /proc/sys/fs/binfmt_misc/status || true
fi
