#!/usr/bin/env bash
# Compile WIC image decode + memestream + options, relink dogecoin-qt
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild
S=/mnt/c/dogedev
cd "$B/src"

cp -f "$S/src/qt/win_image_decode.cpp" "$S/src/qt/win_image_decode.h" \
      "$S/src/qt/memestreamclient.cpp" "$S/src/qt/memestreamclient.h" \
      "$S/src/qt/memestreampage.cpp" "$S/src/qt/memestreamrail.cpp" \
      "$S/src/qt/optionsdialog.cpp" "$S/src/qt/optionsmodel.cpp" "$S/src/qt/optionsmodel.h" \
      "$S/src/qt/optionsdialog.h" qt/
cp -f "$S/src/qt/forms/optionsdialog.ui" "$S/src/qt/forms/ui_optionsdialog.h" qt/forms/ 2>/dev/null || true
cp -f "$S/src/qt/forms/ui_optionsdialog.h" qt/forms/ 2>/dev/null || \
  uic "$S/src/qt/forms/optionsdialog.ui" -o qt/forms/ui_optionsdialog.h

# Ensure win_image_decode is in Makefile SOURCES if missing
if ! grep -q 'win_image_decode.cpp' Makefile 2>/dev/null; then
  if grep -q 'memestreamclient.cpp' Makefile; then
    sed -i 's|qt/memestreamclient\.cpp|qt/memestreamclient.cpp \\\n\tqt/win_image_decode.cpp|g' Makefile || true
  fi
fi

cat > /tmp/build-meme2.mk <<'EOF'
include Makefile
.PHONY: meme_objs
meme_objs:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-win_image_decode.o qt/win_image_decode.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-memestreamclient.o qt/memestreamclient.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-memestreampage.o qt/memestreampage.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-memestreamrail.o qt/memestreamrail.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-optionsdialog.o qt/optionsdialog.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-optionsmodel.o qt/optionsmodel.cpp
EOF

make -f /tmp/build-meme2.mk meme_objs 2>&1 | tail -50
ls -la qt/libdogecoinqt_a-win_image_decode.o qt/libdogecoinqt_a-memestreamclient.o

x86_64-w64-mingw32-ar r qt/libdogecoinqt.a \
  qt/libdogecoinqt_a-win_image_decode.o \
  qt/libdogecoinqt_a-memestreamclient.o \
  qt/libdogecoinqt_a-memestreampage.o \
  qt/libdogecoinqt_a-memestreamrail.o \
  qt/libdogecoinqt_a-optionsdialog.o \
  qt/libdogecoinqt_a-optionsmodel.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

rm -f qt/dogecoin-qt.exe
# Link may need windowscodecs - usually via auto-import on mingw
make qt/dogecoin-qt.exe 2>&1 | tail -40
if [[ -f qt/dogecoin-qt.exe ]]; then
  file qt/dogecoin-qt.exe
  strings qt/dogecoin-qt.exe | grep -F 'decoded image via OS' | head -2
  strings qt/dogecoin-qt.exe | grep -F 'Prefer Fast Sync' | head -2
  cp -f qt/dogecoin-qt.exe /mnt/c/dogedev/smoke-run/
  echo QT_JPEG_WIC_OK
else
  echo LINK_FAILED
  exit 1
fi
