#!/usr/bin/env bash
# Windows PE dogecoind + cli with -archivepath / getarchiveinfo / verifyarchive.
# Does not build Qt. Does not start or kill a node.
set -euo pipefail
export PATH="/usr/bin:/bin:/usr/local/bin:${PATH:-}"

SRC=/mnt/c/dogedev
BUILD=/home/theretardedelon/dogedev-winbuild
OUT="$SRC/release"
LOG="$OUT/rebuild-win-daemon-archive.log"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
HOST=x86_64-w64-mingw32

mkdir -p "$OUT"
exec > >(tee -a "$LOG") 2>&1
echo "======== rebuild-win-daemon-archive $(date -Is) jobs=$JOBS ========"

[[ -d "$BUILD/src" ]] || { echo "missing winbuild $BUILD"; exit 1; }

log() { echo "[archive-pe $(date +%H:%M:%S)] $*"; }

log "1/4 sync sources"
sync_files=(
  src/blockarchive.cpp
  src/blockarchive.h
  src/cloudpath.h
  src/init.cpp
  src/validation.cpp
  src/rpc/blockchain.cpp
  src/node/snapshot_fetch.cpp
  src/node/snapshot_fetch.h
  src/qt/intro.cpp
  src/dbengine.cpp
  src/dbengine.h
  src/dbmigrate.cpp
  src/dbmigrate.h
  src/dbwrapper.cpp
  src/dbwrapper.h
  src/mdbx/mdbx.c
  src/mdbx/mdbx.h
  src/mdbx/mdbx-internals.h
  src/mdbx/mdbx-wingetopt.h
)
for p in "${sync_files[@]}"; do
  if [[ -f "$SRC/$p" ]]; then
    mkdir -p "$(dirname "$BUILD/$p")"
    cp -a "$SRC/$p" "$BUILD/$p"
    log "  synced $p"
  else
    log "  MISSING $p"
    exit 1
  fi
done

log "2/4 ensure Makefile knows archive + mdbx stack"
mkdir -p "$BUILD/src/mdbx"
python3 - <<'PY'
from pathlib import Path
p = Path("/home/theretardedelon/dogedev-winbuild/src/Makefile")
t = p.read_text()

def add_obj(text, after_obj, new_obj):
    old = "\t%s.$(OBJEXT) \\\n" % after_obj
    new = old + "\t%s.$(OBJEXT) \\\n" % new_obj
    if ("%s.$(OBJEXT)" % new_obj) in text:
        return text
    if old not in text:
        raise SystemExit("missing object line %s" % after_obj)
    return text.replace(old, new, 1)

def add_cxx_rule(text, stem, src):
    key = "%s.o: %s" % (stem, src)
    if key in text:
        return text
    rule = (
        "\n%s.o: %s\n"
        "\t$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -c -o %s.o `test -f '%s' || echo '$(srcdir)/'`%s\n"
        % (stem, src, stem, src, src)
    )
    needle = "libdogecoin_server_a-ibdstats.o: ibdstats.cpp"
    if needle in text:
        return text.replace(needle, rule + "\n" + needle, 1)
    return text + rule

def add_c_rule(text, stem, src):
    key = "%s.o: %s" % (stem, src)
    if key in text:
        return text
    rule = (
        "\n%s.o: %s\n"
        "\t$(AM_V_CC)$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -Wno-error -c -o %s.o `test -f '%s' || echo '$(srcdir)/'`%s\n"
        % (stem, src, stem, src, src)
    )
    return text + rule

t = add_obj(t, "libdogecoin_server_a-bloom", "libdogecoin_server_a-blockarchive")
t = add_obj(t, "libdogecoin_server_a-init", "libdogecoin_server_a-dbengine")
t = add_obj(t, "libdogecoin_server_a-dbwrapper", "libdogecoin_server_a-dbmigrate")
t = add_obj(t, "libdogecoin_server_a-dbmigrate", "mdbx/libdogecoin_server_a-mdbx")
t = add_cxx_rule(t, "libdogecoin_server_a-blockarchive", "blockarchive.cpp")
t = add_cxx_rule(t, "libdogecoin_server_a-dbengine", "dbengine.cpp")
t = add_cxx_rule(t, "libdogecoin_server_a-dbmigrate", "dbmigrate.cpp")
t = add_c_rule(t, "mdbx/libdogecoin_server_a-mdbx", "mdbx/mdbx.c")
p.write_text(t)
print("Makefile ready")
PY

log "3/4 drop stale objects and make daemon"
rm -f \
  "$BUILD/src/libdogecoin_server_a-blockarchive.o" \
  "$BUILD/src/libdogecoin_server_a-init.o" \
  "$BUILD/src/libdogecoin_server_a-validation.o" \
  "$BUILD/src/libdogecoin_server_a-dbengine.o" \
  "$BUILD/src/libdogecoin_server_a-dbmigrate.o" \
  "$BUILD/src/libdogecoin_server_a-dbwrapper.o" \
  "$BUILD/src/mdbx/libdogecoin_server_a-mdbx.o" \
  "$BUILD/src/rpc/libdogecoin_server_a-blockchain.o" \
  "$BUILD/src/node/libdogecoin_server_a-snapshot_fetch.o" \
  "$BUILD/src/dogecoind.exe" \
  "$BUILD/src/dogecoin-cli.exe"

cd "$BUILD/src"
make dogecoind.exe dogecoin-cli.exe -j"${JOBS}"

test -f "$BUILD/src/dogecoind.exe"
test -f "$BUILD/src/dogecoin-cli.exe"

log "4/4 stage PE (side copy + smoke if unlocked)"
mkdir -p "$OUT" "$OUT/smoke-pro-gui"
cp -f "$BUILD/src/dogecoind.exe" "$OUT/dogecoind-archive.exe"
cp -f "$BUILD/src/dogecoin-cli.exe" "$OUT/dogecoin-cli-archive.exe"
${HOST}-strip -s "$OUT/dogecoind-archive.exe" || true
${HOST}-strip -s "$OUT/dogecoin-cli-archive.exe" || true

if cp -f "$OUT/dogecoind-archive.exe" "$OUT/dogecoind.exe" 2>/dev/null; then
  log "  updated release/dogecoind.exe"
else
  log "  release/dogecoind.exe locked — left dogecoind-archive.exe"
fi
if cp -f "$OUT/dogecoin-cli-archive.exe" "$OUT/dogecoin-cli.exe" 2>/dev/null; then
  log "  updated release/dogecoin-cli.exe"
fi
if cp -f "$OUT/dogecoind-archive.exe" "$OUT/smoke-pro-gui/dogecoind.exe" 2>/dev/null; then
  log "  updated smoke-pro-gui/dogecoind.exe"
else
  log "  smoke dogecoind.exe locked"
fi
if cp -f "$OUT/dogecoin-cli-archive.exe" "$OUT/smoke-pro-gui/dogecoin-cli.exe" 2>/dev/null; then
  log "  updated smoke-pro-gui/dogecoin-cli.exe"
fi

log "version / archive strings"
strings "$OUT/dogecoind-archive.exe" | grep -E 'archivepath|getarchiveinfo|verifyarchive|1\.14\.103' | head -20 || true

log "DONE $(date -Is)"
ls -lh "$OUT/dogecoind-archive.exe" "$OUT/dogecoin-cli-archive.exe"
echo "REBUILD_WIN_DAEMON_ARCHIVE_OK"
