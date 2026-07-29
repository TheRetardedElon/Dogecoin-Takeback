#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release

cp -f "$SRC/src/qt/dogecoingui.cpp" "$BUILD/src/qt/dogecoingui.cpp"

cd "$BUILD"
# Force recompile of dogecoingui
rm -f src/qt/libdogecoinqt_a-dogecoingui.o src/qt/libdogecoinqt.a src/qt/dogecoin-qt.exe

make -j"$(nproc)" -C src qt/dogecoin-qt.exe 2>&1 | tee /tmp/rebuild-invoice-fix.log | tail -n 40

file src/qt/dogecoin-qt.exe
# quick string check that createTrayIcon path still exists
x86_64-w64-mingw32-strings src/qt/dogecoin-qt.exe | grep -F 'Invoice created' | head -3 || true

bash "$SRC/scripts/package-windows-release.sh"

# Ensure setup is present
if [[ ! -f "$OUT/dogecoin-1.14.100-win64-setup.exe" ]]; then
  cp -f "$BUILD"/dogecoin-1.14.100-win64-setup.exe "$OUT/" 2>/dev/null || \
    cp -f "$BUILD"/dogecoin-*-win64-setup.exe "$OUT/dogecoin-1.14.100-win64-setup.exe"
fi

(
  cd "$OUT"
  sha256sum dogecoin-1.14.100-win64.zip dogecoin-1.14.100-win64-setup.exe | tee SHA256SUMS-win64.txt
  ls -lah
)
echo FIX_PACKAGED
