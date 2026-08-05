#!/usr/bin/env bash
# Stamp 1.14.101 into winbuild, recompile version + P0 objects, link, package.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"

echo "[1] Sync P0 sources (do NOT overwrite MinGW dogecoin-config.h)"
# Version is stamped separately by stamp-winbuild-version.sh into the
# platform-correct generated config (HAVE_BYTESWAP_H must stay undefined for MinGW).
bash /mnt/c/dogedev/scripts/stamp-winbuild-version.sh
cp -f "$SRC/src/clientversion.h" .
cp -f "$SRC/share/qt/Info.plist" ../share/qt/Info.plist 2>/dev/null || true

cp -f "$SRC/src/ibdstats.cpp" "$SRC/src/ibdstats.h" .
cp -f "$SRC/src/net_processing.cpp" "$SRC/src/net_processing.h" .
cp -f "$SRC/src/validation.cpp" "$SRC/src/validation.h" .
cp -f "$SRC/src/init.cpp" .
cp -f "$SRC/src/rpc/blockchain.cpp" rpc/
cp -f "$SRC/src/rpc/net.cpp" rpc/
cp -f "$SRC/src/qt/clientmodel.cpp" "$SRC/src/qt/clientmodel.h" qt/
cp -f "$SRC/src/qt/networkpage.cpp" "$SRC/src/qt/networkpage.h" qt/
# clientversion lives in util or common
if [[ -f "$SRC/src/clientversion.cpp" ]]; then
  cp -f "$SRC/src/clientversion.cpp" .
fi

# Patch PACKAGE_VERSION in winbuild Makefile if present
if grep -q 'PACKAGE_VERSION = 1.14' ../Makefile 2>/dev/null; then
  sed -i 's/PACKAGE_VERSION = 1\.14\.[0-9]*/PACKAGE_VERSION = 1.14.101/' ../Makefile
fi
if grep -q '1.14.100' ../share/setup.nsi 2>/dev/null; then
  sed -i 's/1\.14\.100/1.14.101/g' ../share/setup.nsi
fi

DEP_INC="$BUILD/depends/x86_64-w64-mingw32/share/../include"
FLAGS="-DHAVE_CONFIG_H -I. -I../src/config -I./config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads"
FLAGS="$FLAGS -I$DEP_INC"
FLAGS="$FLAGS -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include"
FLAGS="$FLAGS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB"
FLAGS="$FLAGS -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden -std=c++11"
QTFLAGS="$FLAGS -I./qt -I./qt/forms -DQT_NO_KEYWORDS"
QTFLAGS="$QTFLAGS -I$DEP_INC/QtCore -I$DEP_INC/QtGui -I$DEP_INC/QtWidgets -I$DEP_INC/QtNetwork"
QTFLAGS="$QTFLAGS -I$DEP_INC/QtTest -I$DEP_INC/QtDBus -I$DEP_INC/QtPrintSupport -I$DEP_INC/"

compile() {
  echo "  CXX $1"
  x86_64-w64-mingw32-g++ $3 -c -o "$1" "$2"
}

echo "[2] Compile version + P0 objects"
# clientversion is usually in libdogecoin_util
if [[ -f clientversion.cpp ]]; then
  if [[ -f libdogecoin_util_a-clientversion.o ]]; then
    compile libdogecoin_util_a-clientversion.o clientversion.cpp "$FLAGS"
  fi
fi
# Also rebuild util archive member if named differently
ls libdogecoin_util_a-clientversion.o 2>/dev/null || true

compile libdogecoin_server_a-ibdstats.o ibdstats.cpp "$FLAGS"
compile libdogecoin_server_a-net_processing.o net_processing.cpp "$FLAGS"
compile libdogecoin_server_a-validation.o validation.cpp "$FLAGS"
compile libdogecoin_server_a-init.o init.cpp "$FLAGS"
compile rpc/libdogecoin_server_a-blockchain.o rpc/blockchain.cpp "$FLAGS"
compile rpc/libdogecoin_server_a-net.o rpc/net.cpp "$FLAGS"

MOC="$BUILD/depends/x86_64-w64-mingw32/native/bin/moc"
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_networkpage.cpp qt/networkpage.h
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_clientmodel.cpp qt/clientmodel.h
compile qt/libdogecoinqt_a-clientmodel.o qt/clientmodel.cpp "$QTFLAGS"
compile qt/libdogecoinqt_a-networkpage.o qt/networkpage.cpp "$QTFLAGS"
compile qt/libdogecoinqt_a-moc_networkpage.o qt/moc_networkpage.cpp "$QTFLAGS"
compile qt/libdogecoinqt_a-moc_clientmodel.o qt/moc_clientmodel.cpp "$QTFLAGS"

# windres for version resources if present
if [[ -f dogecoin-qt-res.rc ]] || [[ -f qt/res/dogecoin-qt-res.rc ]]; then
  echo "  (resource scripts present — make will refresh if needed)"
fi

echo "[3] Rebuild util archive if clientversion updated"
if [[ -f libdogecoin_util_a-clientversion.o && -f libdogecoin_util.a ]]; then
  x86_64-w64-mingw32-ar r libdogecoin_util.a libdogecoin_util_a-clientversion.o
  x86_64-w64-mingw32-ranlib libdogecoin_util.a
fi

echo "[4] Rebuild server archive"
SERVER_OBJS=(
  libdogecoin_server_a-addrman.o
  libdogecoin_server_a-addrdb.o
  libdogecoin_server_a-bloom.o
  libdogecoin_server_a-blockencodings.o
  libdogecoin_server_a-chain.o
  libdogecoin_server_a-checkpoints.o
  libdogecoin_server_a-httprpc.o
  libdogecoin_server_a-httpserver.o
  libdogecoin_server_a-ibdstats.o
  libdogecoin_server_a-init.o
  libdogecoin_server_a-dbwrapper.o
  libdogecoin_server_a-merkleblock.o
  libdogecoin_server_a-miner.o
  libdogecoin_server_a-net.o
  libdogecoin_server_a-net_processing.o
  libdogecoin_server_a-noui.o
  policy/libdogecoin_server_a-fees.o
  policy/libdogecoin_server_a-policy.o
  libdogecoin_server_a-pow.o
  libdogecoin_server_a-rest.o
  rpc/libdogecoin_server_a-auxcache.o
  rpc/libdogecoin_server_a-auxpow.o
  rpc/libdogecoin_server_a-blockchain.o
  rpc/libdogecoin_server_a-mining.o
  rpc/libdogecoin_server_a-misc.o
  rpc/libdogecoin_server_a-net.o
  rpc/libdogecoin_server_a-rawtransaction.o
  rpc/libdogecoin_server_a-server.o
  script/libdogecoin_server_a-sigcache.o
  script/libdogecoin_server_a-ismine.o
  libdogecoin_server_a-timedata.o
  libdogecoin_server_a-torcontrol.o
  libdogecoin_server_a-txdb.o
  libdogecoin_server_a-txmempool.o
  libdogecoin_server_a-txrequest.o
  libdogecoin_server_a-ui_interface.o
  libdogecoin_server_a-validation.o
  libdogecoin_server_a-validationinterface.o
  libdogecoin_server_a-versionbits.o
)
for extra in libdogecoin_server_a-dogecoin.o libdogecoin_server_a-dogecoin-fees.o; do
  [[ -f "$extra" ]] && SERVER_OBJS+=("$extra")
done

missing=0
for o in "${SERVER_OBJS[@]}"; do
  if [[ ! -f "$o" ]]; then echo "MISSING $o"; missing=1; fi
done
[[ "$missing" -eq 0 ]] || exit 1

rm -f libdogecoin_server.a
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
i=0
STAGED=()
for o in "${SERVER_OBJS[@]}"; do
  dest="$STAGE/${i}_$(basename "$o")"
  cp -f "$o" "$dest"
  STAGED+=("$dest")
  i=$((i + 1))
done
x86_64-w64-mingw32-ar cr libdogecoin_server.a "${STAGED[@]}"
x86_64-w64-mingw32-ranlib libdogecoin_server.a

x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-clientmodel.o \
  qt/libdogecoinqt_a-networkpage.o \
  qt/libdogecoinqt_a-moc_networkpage.o \
  qt/libdogecoinqt_a-moc_clientmodel.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

echo "[5] Link"
rm -f dogecoind.exe qt/dogecoin-qt.exe
# Refresh version resources via make if possible
make dogecoind-res.o qt/res/dogecoin-qt-res.o 2>/dev/null || true
make dogecoind.exe 2>&1 | tail -15
make qt/dogecoin-qt.exe 2>&1 | tail -20

echo "[6] Version string check"
x86_64-w64-mingw32-strings dogecoind.exe | grep -E '1\.14\.101|getibdinfo' | head -10
x86_64-w64-mingw32-strings qt/dogecoin-qt.exe | grep -E '1\.14\.101|getibdinfo|IBD telemetry|Arcade' | head -15
file dogecoind.exe qt/dogecoin-qt.exe

echo "[7] Package 1.14.101"
bash /mnt/c/dogedev/scripts/package-windows-release.sh 2>&1 | tail -40
if [[ -f "$BUILD/dogecoin-1.14.101-win64-setup.exe" ]]; then
  cp -f "$BUILD/dogecoin-1.14.101-win64-setup.exe" /mnt/c/dogedev/release/
fi
(
  cd /mnt/c/dogedev/release
  sha256sum dogecoin-1.14.101-win64.zip dogecoin-1.14.101-win64-setup.exe 2>/dev/null | tee SHA256SUMS-win64.txt
  ls -lah dogecoin-1.14.101-win64*
)
echo RELEASE_1_14_101_DONE
