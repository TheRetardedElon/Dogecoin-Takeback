#!/usr/bin/env bash
# Rebuild dogecoin-qt with shutdown heap-corruption fixes and stage to smoke-run.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin

B=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
HOST=x86_64-w64-mingw32
DEP="$B/depends/x86_64-w64-mingw32/share/../include"
LIBDIR="$B/depends/x86_64-w64-mingw32/share/../lib"

cd "$B/src"
for f in peermapwidget.cpp peermapwidget.h networkpage.cpp clientmodel.cpp \
         memestreamclient.cpp memestreamclient.h memestreamrail.cpp memestreamrail.h \
         walletview.cpp dogecoin.cpp; do
  cp -f "$SRC/src/qt/$f" qt/
done

QT_INCLUDES=$(sed -n 's/^QT_INCLUDES = //p' Makefile | head -1)
MOC=$(find "$B/depends" -name moc -type f 2>/dev/null | head -1)

compile() {
  local src=$1 out=$2
  echo "CXX $out"
  # shellcheck disable=SC2086
  ${HOST}-g++ -std=c++11 -DHAVE_CONFIG_H \
    -I. -I../src/config -I. -I./obj -I./qt -I./qt/forms \
    -mthreads $QT_INCLUDES -I"$DEP" -I"$DEP"/ \
    -I./leveldb/include -I./leveldb/helpers/memenv \
    -I./secp256k1/include -I./univalue/include \
    -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS \
    -DBOOST_THREAD_USE_LIB -pipe -O2 \
    -c -o "$out" "$src"
}

# moc headers that changed public API
"$MOC" -o qt/moc_memestreamrail.cpp qt/memestreamrail.h
"$MOC" -o qt/moc_peermapwidget.cpp qt/peermapwidget.h
"$MOC" -o qt/moc_memestreamclient.cpp qt/memestreamclient.h 2>/dev/null || true

compile qt/peermapwidget.cpp qt/libdogecoinqt_a-peermapwidget.o
compile qt/networkpage.cpp qt/libdogecoinqt_a-networkpage.o
compile qt/clientmodel.cpp qt/libdogecoinqt_a-clientmodel.o
compile qt/memestreamclient.cpp qt/libdogecoinqt_a-memestreamclient.o
compile qt/memestreamrail.cpp qt/libdogecoinqt_a-memestreamrail.o
compile qt/walletview.cpp qt/libdogecoinqt_a-walletview.o
compile qt/dogecoin.cpp qt/dogecoin_qt-dogecoin.o
compile qt/moc_memestreamrail.cpp qt/libdogecoinqt_a-moc_memestreamrail.o
compile qt/moc_peermapwidget.cpp qt/libdogecoinqt_a-moc_peermapwidget.o

${HOST}-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-peermapwidget.o \
  qt/libdogecoinqt_a-networkpage.o \
  qt/libdogecoinqt_a-clientmodel.o \
  qt/libdogecoinqt_a-memestreamclient.o \
  qt/libdogecoinqt_a-memestreamrail.o \
  qt/libdogecoinqt_a-walletview.o \
  qt/libdogecoinqt_a-moc_memestreamrail.o \
  qt/libdogecoinqt_a-moc_peermapwidget.o \
  qt/dogecoin_qt-dogecoin.o 2>/dev/null || true
# dogecoin.o is linked separately as dogecoin_qt-dogecoin.o (not in .a)
${HOST}-ranlib qt/libdogecoinqt.a

echo "--- link dogecoin-qt ---"
rm -f qt/dogecoin-qt.exe
/bin/bash ../libtool --tag=CXX --mode=link \
  ${HOST}-g++ -std=c++11 \
  -Wl,--exclude-libs,ALL -pthread \
  -Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va \
  -mwindows -all-static -L"$LIBDIR" \
  -o qt/dogecoin-qt.exe \
  qt/dogecoin_qt-dogecoin.o qt/res/dogecoin-qt-res.o \
  qt/libdogecoinqt.a libdogecoin_server.a libdogecoin_wallet.a \
  libdogecoin_zmq.a -lzmq libdogecoin_cli.a libdogecoin_common.a \
  libdogecoin_util.a libdogecoin_consensus.a crypto/libdogecoin_crypto.a \
  univalue/libunivalue.la leveldb/libleveldb.a leveldb/libmemenv.a \
  -L"$LIBDIR" \
  -lboost_system-mt-s -lboost_filesystem-mt-s -lboost_program_options-mt-s \
  -lboost_thread_win32-mt-s -lboost_chrono-mt-s \
  -lwindowsprintersupport -lqminimal -lqwindows \
  -lQt5PrintSupport -lQt5Widgets -lQt5Network -lQt5Gui -lQt5Core \
  -lqtharfbuzzng -lqtpcre -lqtpng -lz -limm32 \
  -L"$LIBDIR" \
  -L"$B/depends/x86_64-w64-mingw32/share/../plugins/platforms" \
  -L"$B/depends/x86_64-w64-mingw32/share/../plugins/printsupport" \
  -lqrencode -ldb_cxx-5.3 -lssl -lcrypto -lminiupnpc \
  secp256k1/libsecp256k1.la -levent \
  -lQt5PlatformSupport -lssp -lcrypt32 -lwinhttp -liphlpapi -lshlwapi \
  -lmswsock -lws2_32 -ladvapi32 -lrpcrt4 -luuid -loleaut32 -lole32 \
  -lcomctl32 -lshell32 -lwinmm -lwinspool -lcomdlg32 -lgdi32 -luser32 \
  -lkernel32 -lmingwthrd

ls -lh qt/dogecoin-qt.exe
cp -f qt/dogecoin-qt.exe "$SRC/smoke-run/dogecoin-qt.exe"
cp -f qt/dogecoin-qt.exe "$SRC/release/dogecoin-qt-shutdown-fix.exe"
echo SHUTDOWN_FIX_QT_OK
