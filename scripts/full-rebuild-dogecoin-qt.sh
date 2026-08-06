#!/usr/bin/env bash
# Full clean rebuild of Windows dogecoin-qt from C:\dogedev sources.
# Uses depends Qt 5.7 uic/moc — do NOT copy Qt 5.15 ui_*.h from the Windows tree.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
SMOKE=/mnt/c/dogedev/smoke-run

echo "=== 1. Sync GUI/node sources (NOT ui_*.h) ==="
cd "$BUILD"

# Force-sync stability sources only (never forms/ui_*.h — those must be regeneraged by depends uic)
for f in \
  qt/dogecoin.cpp qt/dogecoingui.cpp qt/dogecoingui.h \
  qt/clientmodel.cpp qt/clientmodel.h \
  qt/networkpage.cpp qt/networkpage.h \
  qt/peermapwidget.cpp qt/peermapwidget.h \
  qt/memestreamclient.cpp qt/memestreamclient.h \
  qt/memestreamrail.cpp qt/memestreamrail.h \
  qt/memestreampage.cpp qt/memestreampage.h \
  qt/walletview.cpp qt/walletview.h \
  qt/walletframe.cpp qt/walletframe.h \
  qt/arcadepage.cpp qt/arcadepage.h \
  qt/arcadegamewidget.cpp qt/arcadegamewidget.h \
  qt/thememanager.cpp qt/thememanager.h \
  node/chainstate.cpp node/chainstate.h \
  node/utxo_snapshot.cpp node/utxo_snapshot.h
do
  if [ -f "$SRC/src/$f" ]; then
    mkdir -p "$BUILD/src/$(dirname "$f")"
    cp -f "$SRC/src/$f" "$BUILD/src/$f"
  fi
done

# .ui forms can be synced; generated ui_*.h must come from depends uic
if [ -d "$SRC/src/qt/forms" ]; then
  mkdir -p "$BUILD/src/qt/forms"
  cp -f "$SRC/src/qt/forms/"*.ui "$BUILD/src/qt/forms/" 2>/dev/null || true
fi

echo "=== 2. Drop incompatible Qt5.15 ui_*.h and stale qt objects ==="
cd "$BUILD/src"
find qt/forms -name 'ui_*.h' -type f -delete 2>/dev/null || true
rm -f qt/dogecoin-qt.exe qt/libdogecoinqt.a
find qt -name '*.o' -type f -delete 2>/dev/null || true
find qt -name 'moc_*.cpp' -type f -delete 2>/dev/null || true
# Stale qrc from wrong/newer rcc can reference qResourceFeatureZlib (Qt 5.12+)
rm -f qt/qrc_dogecoin.cpp qt/qrc_dogecoin_locale.cpp

echo "=== 3. Config check ==="
grep -n 'ENABLE_WALLET' config/dogecoin-config.h | head -3

echo "=== 4. make qt/dogecoin-qt.exe (regenerates ui_ + moc with depends toolchain) ==="
NPROC=$(nproc 2>/dev/null || echo 4)
make -j"$NPROC" qt/dogecoin-qt.exe 2>&1 | tee /tmp/full-qt-build.log | tail -100

if [ ! -f qt/dogecoin-qt.exe ]; then
  echo "FATAL: dogecoin-qt.exe missing"
  grep -E 'error:|Error|undefined reference' /tmp/full-qt-build.log | tail -40
  exit 1
fi

# Confirm regenerated ui headers are Qt 5.7 style (no QT_CONFIG)
if head -20 qt/forms/ui_optionsdialog.h | grep -q '5.15'; then
  echo "FATAL: ui_optionsdialog.h still Qt 5.15 generated"
  exit 1
fi
if grep -q 'QT_CONFIG' qt/forms/ui_optionsdialog.h 2>/dev/null; then
  echo "FATAL: ui headers still use QT_CONFIG (wrong uic)"
  exit 1
fi
echo "OK: ui headers regenerated without QT_CONFIG"

echo "=== 5. Symbol sanity ==="
if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q 'gotoOverviewPage'; then
  echo "FATAL: gotoOverviewPage missing (moc/ENABLE_WALLET)"
  exit 1
fi
echo "OK: gotoOverviewPage present"
if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q 'ArcadePageC'; then
  echo "FATAL: ArcadePage missing"
  exit 1
fi
echo "OK: ArcadePage present"
if ! x86_64-w64-mingw32-nm qt/libdogecoinqt.a | grep -q 'PeerMapWidget'; then
  echo "WARN: PeerMapWidget missing"
else
  echo "OK: PeerMapWidget present"
fi

strings qt/dogecoin-qt.exe | grep -F 'not injecting Windows ROOT' | head -1 || echo "note: meme SSL string absent"
strings qt/dogecoin-qt.exe | grep -F 'to load feed' | head -1 || echo "note: defer feed string absent"
strings qt/dogecoin-qt.exe | grep -F 'GUI init step' | head -1 || true

file qt/dogecoin-qt.exe
ls -la qt/dogecoin-qt.exe

echo "=== 6. Install ==="
mkdir -p "$SMOKE"
cp -f qt/dogecoin-qt.exe "$SMOKE/dogecoin-qt.exe"
ls -la "$SMOKE/dogecoin-qt.exe"
echo "FULL_QT_REBUILD_OK"
