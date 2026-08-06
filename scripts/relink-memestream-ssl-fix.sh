#!/usr/bin/env bash
# Rebuild only memestreamclient (no Windows CA inject) and relink dogecoin-qt.
set -euo pipefail
export PATH=/usr/bin:/bin
BUILD=/home/theretardedelon/dogedev-winbuild
cd "$BUILD/src"
DEP="$BUILD/depends/x86_64-w64-mingw32"

cp -f /mnt/c/dogedev/src/qt/memestreamclient.cpp qt/memestreamclient.cpp

CXXFLAGS="-std=c++11 -DHAVE_CONFIG_H -I. -I../src/config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads -I${DEP}/include -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include -I./qt -I./qt/forms -DQT_NO_KEYWORDS -I${DEP}/include/QtCore -I${DEP}/include/QtGui -I${DEP}/include/QtWidgets -I${DEP}/include/QtNetwork -I${DEP}/include/QtTest -I${DEP}/include/QtDBus -I${DEP}/include/QtPrintSupport -I${DEP}/include/ -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden"

echo "Compile memestreamclient..."
x86_64-w64-mingw32-g++ $CXXFLAGS -c -o qt/libdogecoinqt_a-memestreamclient.o qt/memestreamclient.cpp
strings qt/libdogecoinqt_a-memestreamclient.o | grep -F 'not injecting Windows ROOT' | head -1

# Ensure wallet moc still present in archive (from prior remoc)
if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q 'T _ZN11DogecoinGUI16gotoOverviewPageEv'; then
  echo "WARN: gotoOverviewPage missing — remoc dogecoingui with ENABLE_WALLET"
  MOC="$DEP/native/bin/moc"
  DEFS="-DHAVE_CONFIG_H -DENABLE_WALLET -DQT_NO_KEYWORDS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB"
  INCS="-I. -I../src/config -I./obj -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include -I./qt -I./qt/forms -I${DEP}/include -I${DEP}/include/QtCore -I${DEP}/include/QtGui -I${DEP}/include/QtWidgets -I${DEP}/include/QtNetwork"
  cp -f /mnt/c/dogedev/src/qt/dogecoingui.h qt/
  "$MOC" $DEFS $INCS -o qt/moc_dogecoingui.cpp qt/dogecoingui.h
  x86_64-w64-mingw32-g++ $CXXFLAGS -c -o qt/libdogecoinqt_a-moc_dogecoingui.o qt/moc_dogecoingui.cpp
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a qt/libdogecoinqt_a-moc_dogecoingui.o
fi

x86_64-w64-mingw32-ar r qt/libdogecoinqt.a qt/libdogecoinqt_a-memestreamclient.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

# Capture link recipe, then link AFTER final ar so make cannot wipe us mid-link with stale archive.
rm -f qt/dogecoin-qt.exe
make -n V=1 qt/dogecoin-qt.exe > /tmp/qt_link_recipe.txt 2>&1 || true
# Prefer actual make link once; immediately re-ar memestream + remoc and re-link via extracted g++ line
make V=1 qt/dogecoin-qt.exe 2>&1 | tee /tmp/qt_link_ms.log | tail -25

# make may have rebuilt .a from objects — our .o is newest so should be included.
# Still force-replace our object and re-link if binary lacks the fix string.
x86_64-w64-mingw32-ar r qt/libdogecoinqt.a qt/libdogecoinqt_a-memestreamclient.o
if x86_64-w64-mingw32-nm qt/libdogecoinqt_a-moc_dogecoingui.o 2>/dev/null | grep -q gotoOverviewPage; then
  x86_64-w64-mingw32-ar r qt/libdogecoinqt.a qt/libdogecoinqt_a-moc_dogecoingui.o
fi
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

LINK_LINE=$(grep 'x86_64-w64-mingw32-g++' /tmp/qt_link_ms.log | grep -E 'dogecoin-qt(\.exe)?' | tail -1 || true)
if [ -z "$LINK_LINE" ]; then
  # fallback: any long g++ line with -o qt/dogecoin-qt
  LINK_LINE=$(grep 'x86_64-w64-mingw32-g++' /tmp/qt_link_ms.log | grep '\-o qt/dogecoin-qt' | tail -1 || true)
fi
if [ -n "$LINK_LINE" ]; then
  echo "Re-link with patched archive:"
  echo "$LINK_LINE" | head -c 200
  echo "..."
  rm -f qt/dogecoin-qt.exe
  eval "$LINK_LINE"
else
  echo "No link line captured; make again"
  rm -f qt/dogecoin-qt.exe
  make qt/dogecoin-qt.exe 2>&1 | tail -10
fi

echo "Verify binary..."
if ! strings qt/dogecoin-qt.exe | grep -q 'not injecting Windows ROOT'; then
  echo "FATAL: fix string not in dogecoin-qt.exe"
  exit 1
fi
echo "OK: $(strings qt/dogecoin-qt.exe | grep -F 'not injecting Windows ROOT' | head -1)"
x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep 'T _ZN11DogecoinGUI16gotoOverviewPageEv' | head -1 || echo "WARN no gotoOverviewPage in .a"
ls -la qt/dogecoin-qt.exe
cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/dogecoin-qt.exe
echo MEMESTREAM_SSL_FIX_OK
