#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

BUILD=/home/theretardedelon/dogedev-winbuild
cd "$BUILD"

echo "=== OpenSSL symbols in dogecoin-qt.exe ==="
x86_64-w64-mingw32-nm src/qt/dogecoin-qt.exe 2>/dev/null | grep -E 'SSL_CTX_new|SSL_library_init|OPENSSL_init|TLS_method|SSLv23' | head -20 || true
x86_64-w64-mingw32-nm -C src/qt/dogecoin-qt.exe 2>/dev/null | grep -E 'QSslSocket::supportsSsl|supportsSsl' | head -10 || true

echo "=== link libs from Makefile ==="
grep -E 'QT_LIBS|SSL_LIBS|CRYPTO_LIBS|dogecoin_qt_LDADD' src/Makefile | head -30

echo "=== strings supportsSsl path ==="
# If linked properly, openssl version string often appears
x86_64-w64-mingw32-strings src/qt/dogecoin-qt.exe | grep -E 'OpenSSL [0-9]|TLSv1|SSLv3|https://' | head -20

# Package
bash /mnt/c/dogedev/scripts/package-windows-release.sh

# Fix NSIS version if needed
if grep -q 'VERSION 1.14.99' share/setup.nsi 2>/dev/null; then
  sed -i 's/!define VERSION 1.14.99/!define VERSION 1.14.100/' share/setup.nsi
  sed -i 's/PACKAGE_VERSION = 1.14.99/PACKAGE_VERSION = 1.14.100/' Makefile || true
  sed -i 's/CLIENT_VERSION_REVISION = 99/CLIENT_VERSION_REVISION = 100/' Makefile || true
  make deploy 2>&1 | tail -20
  cp -f dogecoin-1.14.100-win64-setup.exe /mnt/c/dogedev/release/ 2>/dev/null || \
    cp -f dogecoin-*-win64-setup.exe /mnt/c/dogedev/release/dogecoin-1.14.100-win64-setup.exe
  (
    cd /mnt/c/dogedev/release
    sha256sum dogecoin-1.14.100-win64.zip dogecoin-1.14.100-win64-setup.exe | tee SHA256SUMS-win64.txt
  )
fi

ls -lah /mnt/c/dogedev/release/
echo ALL_DONE
