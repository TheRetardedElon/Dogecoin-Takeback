#!/usr/bin/env bash
# Re-extract all cached depends packages into the host prefix, then verify Qt+SSL.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

BUILD=/home/theretardedelon/dogedev-winbuild
cd "$BUILD/depends"

echo "=== prefix before ==="
ls -la x86_64-w64-mingw32/lib 2>/dev/null | head -50 || true
ls x86_64-w64-mingw32/include 2>/dev/null | head -40 || true

# Full depends install: extracts cached tarballs into prefix
echo "=== make install all packages ==="
make HOST=x86_64-w64-mingw32 -j"$(nproc)" 2>&1 | tee /tmp/depends-install.log | tail -n 40

echo "=== prefix after ==="
ls -la x86_64-w64-mingw32/lib | head -60
test -f x86_64-w64-mingw32/lib/libQt5Network.a
test -f x86_64-w64-mingw32/lib/libQt5Core.a
echo QT_LIBS_OK

# Confirm OpenSSL in Qt config
if grep -qi openssl x86_64-w64-mingw32/mkspecs/qconfig.pri 2>/dev/null; then
  grep -i openssl x86_64-w64-mingw32/mkspecs/qconfig.pri
fi
# QT_NO_SSL should not be forced
if test -f x86_64-w64-mingw32/include/QtNetwork/qconfig-network.h; then
  echo "--- qconfig-network.h ---"
  cat x86_64-w64-mingw32/include/QtNetwork/qconfig-network.h | head -40
fi

# SSL symbols
if x86_64-w64-mingw32-nm x86_64-w64-mingw32/lib/libQt5Network.a 2>/dev/null | grep -q QSslSocket; then
  echo QT_SSL_SYMBOLS_OK
else
  echo QT_SSL_SYMBOLS_MISSING
fi
