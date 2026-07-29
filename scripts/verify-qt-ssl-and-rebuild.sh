#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

BUILD=/home/theretardedelon/dogedev-winbuild
PREFIX="$BUILD/depends/x86_64-w64-mingw32"
SRC=/mnt/c/dogedev

echo "=== Qt SSL config ==="
# Headers: is QT_NO_SSL defined?
if grep -R "QT_NO_SSL" "$PREFIX/include/QtNetwork/" 2>/dev/null | head -10; then
  true
fi
# qconfig
grep -E 'openssl|ssl|OpenSSL' "$PREFIX/mkspecs/qconfig.pri" 2>/dev/null || true
cat "$PREFIX/mkspecs/qmodule.pri" 2>/dev/null | head -40 || true

echo "=== nm demangled SSL ==="
x86_64-w64-mingw32-nm -C "$PREFIX/lib/libQt5Network.a" 2>/dev/null | grep -i 'QSslSocket\|ssl' | head -20 || true
# Also look for object files
x86_64-w64-mingw32-nm "$PREFIX/lib/libQt5Network.a" 2>/dev/null | grep -i 'ssl' | head -20 || true

echo "=== list network ssl objects in archive ==="
x86_64-w64-mingw32-ar t "$PREFIX/lib/libQt5Network.a" | grep -i ssl | head -30 || true

echo "=== sync sources ==="
cp -f "$SRC/src/qt/memestreamclient.cpp" "$BUILD/src/qt/memestreamclient.cpp"
cp -f "$SRC/depends/packages/qt.mk" "$BUILD/depends/packages/qt.mk"

# Ensure mingw configure (not native linux)
cd "$BUILD"
if ! grep -q 'x86_64-w64-mingw32' config.status 2>/dev/null; then
  echo "WARN: config.status is not mingw — reconfiguring"
fi
# Show host from config
grep 'host_alias\|host=' config.log 2>/dev/null | head -5 || true

# CONFIG_SITE for depends
export CONFIG_SITE="$PREFIX/share/config.site"
export PATH="/usr/bin:/bin:$PREFIX/native/bin"

# Stamp version if needed
if test -f "$SRC/scripts/stamp-win-config-h.py"; then
  python3 "$SRC/scripts/stamp-win-config-h.py" "$BUILD/src/config/dogecoin-config.h" || true
fi

echo "=== force rebuild of qt objs that need new QtNetwork ==="
# Touch memestream and wipe qt .o that link network heavily if needed
rm -f src/qt/memestreamclient.o src/qt/libdogecoinqt.a src/qt/dogecoin-qt.exe 2>/dev/null || true
# Many qt objects may need relink only; full qt recompile if headers changed
# Safer: make clean-qt-ish by removing all qt objects
find src/qt -name '*.o' -delete 2>/dev/null || true
find src/qt -name '*.a' -delete 2>/dev/null || true

echo "=== make dogecoin-qt ==="
make -j"$(nproc)" -C src qt/dogecoin-qt.exe 2>&1 | tee /tmp/rebuild-qt-ssl-client.log | tail -n 80

echo "=== verify PE ==="
file src/qt/dogecoin-qt.exe
x86_64-w64-mingw32-strings src/qt/dogecoin-qt.exe | grep -F 'Protocol' | head -5 || true
x86_64-w64-mingw32-strings src/qt/dogecoin-qt.exe | grep -F 'supportsSsl\|OpenSSL\|TLSv1' | head -10 || true
# Check linked openssl symbols
x86_64-w64-mingw32-nm src/qt/dogecoin-qt.exe 2>/dev/null | grep -i 'SSL_CTX\|QSsl' | head -10 || true

echo DONE
