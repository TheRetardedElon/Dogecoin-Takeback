#!/usr/bin/env bash
export PATH=/usr/bin:/bin
PREFIX=/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32
echo "=== prefix libQt ==="
ls -la "$PREFIX/lib"/libQt* 2>&1 | head -40
echo "=== built/qt ==="
ls -la /home/theretardedelon/dogedev-winbuild/depends/built/x86_64-w64-mingw32/qt/
echo "=== find libQt5Network ==="
find /home/theretardedelon/dogedev-winbuild/depends -name 'libQt5Network.a' 2>/dev/null
echo "=== qconfig openssl ==="
grep -i openssl "$PREFIX/mkspecs/qconfig.pri" 2>/dev/null || true
grep -i ssl "$PREFIX/include/QtNetwork/qconfig-network.h" 2>/dev/null | head || true
ls "$PREFIX/include/QtNetwork" 2>&1 | head
echo "=== log tail ==="
tail -n 30 /tmp/qt-ssl-rebuild.log
echo "=== make exit? ==="
# depends should have copied package into prefix during extract_and_install
ls -la "$PREFIX/lib" | head -40
