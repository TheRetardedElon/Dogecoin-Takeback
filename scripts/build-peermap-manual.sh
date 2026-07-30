#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin

BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
cd "$BUILD/src"

cp -f "$SRC/src/qt/peermapwidget.cpp" qt/
cp -f "$SRC/src/qt/peermapwidget.h" qt/
cp -f "$SRC/src/qt/networkpage.cpp" qt/
cp -f "$SRC/src/qt/networkpage.h" qt/
cp -f "$SRC/src/qt/dogecoin.qrc" qt/
mkdir -p qt/res/images
cp -f "$SRC/src/qt/res/images/worldmap-yellow.png" qt/res/images/

MOC=../depends/x86_64-w64-mingw32/native/bin/moc
RCC=../depends/x86_64-w64-mingw32/native/bin/rcc

# Extract pure g++ lines from make -n
extract_cmd() {
  make -n "$1" 2>/dev/null | tr ';' '\n' | grep 'x86_64-w64-mingw32-g++' | tail -1
}

NET_CMD=$(extract_cmd qt/libdogecoinqt_a-networkpage.o)
echo "NET_CMD=$NET_CMD"
test -n "$NET_CMD"

# moc with same defines as compile (approx)
$MOC -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_peermapwidget.cpp qt/peermapwidget.h

PEER_CMD=${NET_CMD//networkpage/peermapwidget}
echo "Compiling peermapwidget..."
eval "$PEER_CMD"

MOC_NET=$(extract_cmd qt/libdogecoinqt_a-moc_networkpage.o)
MOC_PEER=${MOC_NET//moc_networkpage/moc_peermapwidget}
MOC_PEER=${MOC_PEER//networkpage/peermapwidget}
echo "Compiling moc_peermapwidget..."
eval "$MOC_PEER"

# qrc
$RCC -name dogecoin -o qt/qrc_dogecoin.cpp qt/dogecoin.qrc
QRC_CMD=$(extract_cmd qt/libdogecoinqt_a-qrc_dogecoin.o)
echo "Compiling qrc..."
eval "$QRC_CMD"

echo "Compiling networkpage..."
eval "$NET_CMD"

# Archive all qt objects
rm -f qt/libdogecoinqt.a
OBJS=$(ls qt/libdogecoinqt_a-*.o)
echo "Objects: $(echo $OBJS | wc -w)"
# Ensure peermap objects exist
test -f qt/libdogecoinqt_a-peermapwidget.o
test -f qt/libdogecoinqt_a-moc_peermapwidget.o
x86_64-w64-mingw32-ar cr qt/libdogecoinqt.a $OBJS
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

rm -f qt/dogecoin-qt.exe
make -j$(nproc) qt/dogecoin-qt.exe 2>&1 | tee /tmp/peermap-link.log | tail -n 50
file qt/dogecoin-qt.exe
x86_64-w64-mingw32-strings qt/dogecoin-qt.exe | grep -E 'Peers on map|ip-api' | head -5 || true
