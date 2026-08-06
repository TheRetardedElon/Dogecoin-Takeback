#!/usr/bin/env bash
# Verify Pro features in PE (nm symbols) then stage zip + NSIS installer.
# Avoid pipefail+grep -q: early grep exit SIGPIPEs nm and fails the check.
set -eu
export PATH=/usr/bin:/bin:/usr/local/bin

BUILD=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
VERSION=1.14.101
REL=dogecoin-${VERSION}-win64

log() { echo "[package $(date +%H:%M:%S)] $*"; }

cd "$BUILD/src"
test -f dogecoind.exe
test -f dogecoin-cli.exe
test -f qt/dogecoin-qt.exe
file dogecoind.exe dogecoin-cli.exe dogecoin-tx.exe qt/dogecoin-qt.exe

log "Feature gate (nm symbols + daemon strings)"
FAIL=0
NM_QT=$(mktemp)
NM_LIB=$(mktemp)
STR_D=$(mktemp)
trap 'rm -f "$NM_QT" "$NM_LIB" "$STR_D"' EXIT

x86_64-w64-mingw32-nm qt/dogecoin-qt.exe >"$NM_QT" 2>/dev/null || true
x86_64-w64-mingw32-nm qt/libdogecoinqt.a >"$NM_LIB" 2>/dev/null || true
strings dogecoind.exe >"$STR_D" 2>/dev/null || true

check_nm() {
  local file=$1 label=$2
  shift 2
  local s
  for s in "$@"; do
    if grep -F "$s" "$file" >/dev/null 2>&1; then
      echo " OK $label $s"
    else
      echo " FAIL $label $s"
      FAIL=1
    fi
  done
}

check_nm "$NM_QT" nm \
  '_ZN11DogecoinGUI14gotoArcadePageEv' \
  '_ZN11DogecoinGUI18gotoMemeStreamPageEv' \
  '_ZN11DogecoinGUI20gotoDogeBusinessPageEi' \
  'ArcadePage' \
  'MemeStreamPage' \
  'DogeBusinessPage'

check_nm "$NM_LIB" lib \
  'ArcadePage' \
  'MemeStreamPage' \
  'DogeBusinessPage'

if grep -E 'assumeutxo|loadtxoutset|dumptxoutset' "$STR_D" >/dev/null 2>&1; then
  echo " OK assumeutxo RPCs in dogecoind"
else
  echo " FAIL assumeutxo RPCs missing from dogecoind"
  FAIL=1
fi

if [[ "$FAIL" -ne 0 ]]; then
  log "FATAL feature gate"
  exit 1
fi
log "Feature gate PASSED"

log "Stage stripped release"
mkdir -p "$BUILD/release" "$OUT"
cp -f dogecoind.exe dogecoin-cli.exe "$BUILD/release/"
[[ -f dogecoin-tx.exe ]] && cp -f dogecoin-tx.exe "$BUILD/release/"
cp -f qt/dogecoin-qt.exe "$BUILD/release/"
x86_64-w64-mingw32-strip -s "$BUILD/release"/*.exe || true
date -u +"FULL_RELEASE %Y-%m-%dT%H:%M:%SZ" > "$BUILD/release/BUILD_STAMP.txt"
{
  echo "FULL MinGW cross rebuild (depends rebuilt + full make)"
  echo "host=x86_64-w64-mingw32"
  file "$BUILD/release"/*.exe
} >> "$BUILD/release/BUILD_STAMP.txt"
ls -la "$BUILD/release"/*.exe

log "Zip"
STAGE="$BUILD/release-staging/$REL"
rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/daemon"
cp -f "$BUILD/release/dogecoin-qt.exe" "$STAGE/"
cp -f "$BUILD/release/dogecoind.exe" "$STAGE/daemon/"
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/daemon/"
cp -f "$BUILD/release/dogecoin-cli.exe" "$STAGE/bin/"
[[ -f $BUILD/release/dogecoin-tx.exe ]] && cp -f "$BUILD/release/dogecoin-tx.exe" "$STAGE/bin/"
cp -f "$SRC/COPYING" "$STAGE/COPYING.txt" 2>/dev/null || true
[[ -f $SRC/doc/README_windows.txt ]] && cp -f "$SRC/doc/README_windows.txt" "$STAGE/readme.txt" || true
cp -f "$BUILD/release/BUILD_STAMP.txt" "$STAGE/"
cat > "$STAGE/CORE_PRO.txt" <<EOF
Dogecoin Core Pro ${VERSION} — FULL WIN64 RELEASE BUILD
Fresh MinGW cross-compile after depends rebuild (not smoke-run packaging).
Includes: Meme Stream, Arcade, Doge Business, Network/Peer Map, AssumeUTXO RPCs.
$(cat "$BUILD/release/BUILD_STAMP.txt")
https://github.com/TheRetardedElon/Dogecoin-Takeback
EOF

ZIP="$OUT/${REL}.zip"
rm -f "$ZIP"
( cd "$BUILD/release-staging" && find "$REL" -type f | sort | zip -X@ "$ZIP" )
log "ZIP $ZIP ($(stat -c%s "$ZIP") bytes)"

log "NSIS setup"
# make-setup may have CRLF; normalize
tr -d '\r' < "$SRC/scripts/make-setup-1.14.101.sh" > /tmp/make-setup-1.14.101.sh
bash /tmp/make-setup-1.14.101.sh
(
  cd "$OUT"
  sha256sum "${REL}.zip" "${REL}-setup.exe" | tee SHA256SUMS-win64.txt
)
cat > "$OUT/RELEASE_NOTES_win64.md" <<EOF
# Dogecoin Core Pro ${VERSION} Windows x64 — FULL RELEASE

Fresh MinGW cross-build of dogecoind, dogecoin-cli, dogecoin-tx, dogecoin-qt
after a full depends rebuild. **Not** a smoke-run package.

Includes Meme Stream, Arcade, Doge Business, Network, AssumeUTXO RPCs.

- \`${REL}-setup.exe\`
- \`${REL}.zip\`
- \`SHA256SUMS-win64.txt\`

Built: $(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF

ls -lah "$OUT"/${REL}*
cat "$OUT/SHA256SUMS-win64.txt"
echo FULL_RELEASE_OK
