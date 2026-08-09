#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild
S=/mnt/c/dogedev
cd "$B/src"

cp -f "$S/src/qt/memestreamclient.cpp" "$S/src/qt/memestreamclient.h" \
      "$S/src/qt/memestreampage.cpp" "$S/src/qt/memestreampage.h" \
      "$S/src/qt/memestreamrail.cpp" "$S/src/qt/memestreamrail.h" \
      qt/

# Use same flags as other qt lib objects (from chainstate-style private makefile)
cat > /tmp/build-meme.mk <<'EOF'
include Makefile
.PHONY: meme
meme:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-memestreamclient.o qt/memestreamclient.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-memestreampage.o qt/memestreampage.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-memestreamrail.o qt/memestreamrail.cpp
EOF

make -f /tmp/build-meme.mk meme 2>&1 | tail -80
ls -la qt/libdogecoinqt_a-memestreamclient.o qt/libdogecoinqt_a-memestreampage.o qt/libdogecoinqt_a-memestreamrail.o

x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-memestreamclient.o \
  qt/libdogecoinqt_a-memestreampage.o \
  qt/libdogecoinqt_a-memestreamrail.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

rm -f qt/dogecoin-qt.exe
make qt/dogecoin-qt.exe 2>&1 | tail -25
file qt/dogecoin-qt.exe
strings qt/dogecoin-qt.exe | grep -F 'system CA count' | head -2
cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/
echo MEME_QT_OK
