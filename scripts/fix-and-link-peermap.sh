#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
cd /home/theretardedelon/dogedev-winbuild/src

# Fix accidental multi-source deps from naive string replace
python3 - <<'PY'
from pathlib import Path
p = Path("Makefile")
t = p.read_text()
# undo botched moc_networkpage multi-file deps
t = t.replace(
    "qt/libdogecoinqt_a-moc_networkpage.o: qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp",
    "qt/libdogecoinqt_a-moc_networkpage.o: qt/moc_networkpage.cpp",
)
t = t.replace(
    "qt/libdogecoinqt_a-moc_networkpage.obj: qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp",
    "qt/libdogecoinqt_a-moc_networkpage.obj: qt/moc_networkpage.cpp",
)
t = t.replace(
    "`test -f 'qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp' || echo '$(srcdir)/'`qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp",
    "`test -f 'qt/moc_networkpage.cpp' || echo '$(srcdir)/'`qt/moc_networkpage.cpp",
)
t = t.replace(
    "source='qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp'",
    "source='qt/moc_networkpage.cpp'",
)
t = t.replace(
    "`if test -f 'qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp'; then $(CYGPATH_W) 'qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp'; else $(CYGPATH_W) '$(srcdir)/qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp'; fi`",
    "`if test -f 'qt/moc_networkpage.cpp'; then $(CYGPATH_W) 'qt/moc_networkpage.cpp'; else $(CYGPATH_W) '$(srcdir)/qt/moc_networkpage.cpp'; fi`",
)
# Also fix GEN rules if broken
t = t.replace(
    "qt/moc_networkpage.cpp: $(srcdir)/qt/networkpage.h qt/moc_peermapwidget.cpp",
    "qt/moc_networkpage.cpp: $(srcdir)/qt/networkpage.h",
)
p.write_text(t)
print("makefile moc rules cleaned")
PY

FLAGS="-DHAVE_CONFIG_H -I. -I../src/config -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -I. -I./obj -mthreads"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include"
FLAGS="$FLAGS -I./leveldb/include -I./leveldb/helpers/memenv -I./secp256k1/include -I./univalue/include"
FLAGS="$FLAGS -I./qt -I./qt/forms -DQT_NO_KEYWORDS"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtCore"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtGui"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtWidgets"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtNetwork"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtTest"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtDBus"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/QtPrintSupport"
FLAGS="$FLAGS -I/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/share/../include/"
FLAGS="$FLAGS -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS -DBOOST_THREAD_USE_LIB"
FLAGS="$FLAGS -Wstack-protector -fstack-protector-all -pipe -O2 -fvisibility=hidden"

compile() {
  echo "CXX $1"
  x86_64-w64-mingw32-g++ -std=c++11 $FLAGS -c -o "$1" "$2"
}

MOC=../depends/x86_64-w64-mingw32/native/bin/moc
RCC=../depends/x86_64-w64-mingw32/native/bin/rcc

"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_peermapwidget.cpp qt/peermapwidget.h
"$MOC" -DHAVE_CONFIG_H -DQT_NO_KEYWORDS -I. -Iqt -o qt/moc_networkpage.cpp qt/networkpage.h
"$RCC" -name dogecoin -o qt/qrc_dogecoin.cpp qt/dogecoin.qrc

compile qt/libdogecoinqt_a-peermapwidget.o qt/peermapwidget.cpp
compile qt/libdogecoinqt_a-moc_peermapwidget.o qt/moc_peermapwidget.cpp
compile qt/libdogecoinqt_a-networkpage.o qt/networkpage.cpp
compile qt/libdogecoinqt_a-moc_networkpage.o qt/moc_networkpage.cpp
compile qt/libdogecoinqt_a-qrc_dogecoin.o qt/qrc_dogecoin.cpp

# Touch objects so make thinks they're fresh, archive ourselves, then link only
rm -f qt/libdogecoinqt.a
x86_64-w64-mingw32-ar cr qt/libdogecoinqt.a qt/libdogecoinqt_a-*.o
x86_64-w64-mingw32-ranlib qt/libdogecoinqt.a

# Extract link line from make -n
LINK=$(make -n qt/dogecoin-qt.exe 2>/dev/null | tr ';' '\n' | grep -E 'dogecoin-qt.exe' | grep x86_64 | tail -1)
echo "LINK=$LINK"
if [[ -z "$LINK" ]]; then
  # Fallback: make should work now that moc rules fixed
  rm -f qt/dogecoin-qt.exe
  # prevent remake of objects by touching
  touch qt/libdogecoinqt_a-*.o qt/libdogecoinqt.a
  make qt/dogecoin-qt.exe 2>&1 | tee /tmp/peermap-link2.log | tail -n 30
else
  rm -f qt/dogecoin-qt.exe
  eval "$LINK"
fi

file qt/dogecoin-qt.exe
x86_64-w64-mingw32-strings qt/dogecoin-qt.exe | grep -E 'Peers on map|ip-api' | head -5 || true

bash /mnt/c/dogedev/scripts/package-windows-release.sh || true
OUT=/mnt/c/dogedev/release
cp -f ../dogecoin-1.14.100-win64-setup.exe "$OUT/" 2>/dev/null || \
  cp -f /home/theretardedelon/dogedev-winbuild/dogecoin-1.14.100-win64-setup.exe "$OUT/" 2>/dev/null || true
(
  cd "$OUT"
  sha256sum dogecoin-1.14.100-win64.zip dogecoin-1.14.100-win64-setup.exe 2>/dev/null | tee SHA256SUMS-win64.txt
  ls -lah dogecoin-1.14.100-win64*
)
echo ALL_DONE
