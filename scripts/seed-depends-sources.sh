#!/usr/bin/env bash
# Pre-seed depends/sources for packages whose upstream URLs are dead
# or whose dogecoincore.org fallback has an expired TLS cert.
set -euo pipefail
export PATH=/usr/bin:/bin

ROOT="${1:-$HOME/dogedev-winbuild/depends}"
SRC="${ROOT}/sources"
STAMP="${SRC}/download-stamps"
mkdir -p "${SRC}" "${STAMP}"

# curl: allow expired cert on dogecoincore mirror; prefer launchpad/github when possible
fetch() {
  local name="$1" hash="$2" url="$3"
  local out="${SRC}/${name}"
  if [[ -f "${out}" ]]; then
    if echo "${hash}  ${out}" | sha256sum -c - >/dev/null 2>&1; then
      echo "OK cached ${name}"
      (cd "${SRC}" && echo "${hash}  ${name}" > "download-stamps/.stamp_fetched-${name%.tar*}.hash" 2>/dev/null || true)
      # Use the exact stamp names depends expects
      return 0
    fi
    rm -f "${out}"
  fi
  echo "GET ${name} from ${url}"
  curl -fL --retry 3 --connect-timeout 20 -k -o "${out}.temp" "${url}"
  echo "${hash}  ${out}.temp" | sha256sum -c -
  mv "${out}.temp" "${out}"
  echo "OK ${name}"
}

stamp() {
  local package="$1" file="$2" hash="$3"
  (cd "${SRC}" && echo "${hash}  ${file}" > "download-stamps/.stamp_fetched-${package}-${file}.hash")
}

# zlib 1.3
ZLIB_H=ff0ba4c292013dbc27530b3a81e1f9a813cd39de01ca5e0f8bf355702efa593e
fetch zlib-1.3.tar.gz "$ZLIB_H" "https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz" || \
fetch zlib-1.3.tar.gz "$ZLIB_H" "https://depends.dogecoincore.org/zlib-1.3.tar.gz"
stamp zlib zlib-1.3.tar.gz "$ZLIB_H"

# miniupnpc
MU_H=d3c368627f5cdfb66d3ebd64ca39ba54d6ff14a61966dbecb8dd296b7039f16a
fetch miniupnpc-2.0.20170509.tar.gz "$MU_H" "https://depends.dogecoincore.org/miniupnpc-2.0.20170509.tar.gz" || \
fetch miniupnpc-2.0.20170509.tar.gz "$MU_H" "http://miniupnp.tuxfamily.org/files/download.php?file=miniupnpc-2.0.20170509.tar.gz"
stamp miniupnpc miniupnpc-2.0.20170509.tar.gz "$MU_H"

# bdb 5.3.28 NC
BDB_H=76a25560d9e52a198d37a31440fd07632b5f1f8f9f2b6d5438f4bc3e7c9013ef
fetch db-5.3.28.NC.tar.gz "$BDB_H" "https://depends.dogecoincore.org/db-5.3.28.NC.tar.gz" || \
fetch db-5.3.28.NC.tar.gz "$BDB_H" "https://download.oracle.com/berkeley-db/db-5.3.28.NC.tar.gz"
stamp bdb db-5.3.28.NC.tar.gz "$BDB_H"

# Qt 5.7.1 submodules
QT_BASE_H=95f83e532d23b3ddbde7973f380ecae1bac13230340557276f75f2e37984e410
QT_TR_H=3a15aebd523c6d89fb97b2d3df866c94149653a26d27a00aac9b6d3020bc5a1d
QT_TOOLS_H=22d67de915cb8cd93e16fdd38fa006224ad9170bd217c2be1e53045a8dd02f0f
BASE=https://download.qt.io/new_archive/qt/5.7/5.7.1/submodules
MIRROR=https://depends.dogecoincore.org

fetch qtbase-opensource-src-5.7.1.tar.gz "$QT_BASE_H" "${BASE}/qtbase-opensource-src-5.7.1.tar.gz" || \
fetch qtbase-opensource-src-5.7.1.tar.gz "$QT_BASE_H" "${MIRROR}/qtbase-opensource-src-5.7.1.tar.gz"
stamp qt qtbase-opensource-src-5.7.1.tar.gz "$QT_BASE_H"

fetch qttranslations-opensource-src-5.7.1.tar.gz "$QT_TR_H" "${BASE}/qttranslations-opensource-src-5.7.1.tar.gz" || \
fetch qttranslations-opensource-src-5.7.1.tar.gz "$QT_TR_H" "${MIRROR}/qttranslations-opensource-src-5.7.1.tar.gz"
stamp qt qttranslations-opensource-src-5.7.1.tar.gz "$QT_TR_H"

fetch qttools-opensource-src-5.7.1.tar.gz "$QT_TOOLS_H" "${BASE}/qttools-opensource-src-5.7.1.tar.gz" || \
fetch qttools-opensource-src-5.7.1.tar.gz "$QT_TOOLS_H" "${MIRROR}/qttools-opensource-src-5.7.1.tar.gz"
stamp qt qttools-opensource-src-5.7.1.tar.gz "$QT_TOOLS_H"

# Also write the exact stamp names used by depends packages (from package+filename)
for pair in \
  "zlib:zlib-1.3.tar.gz:${ZLIB_H}" \
  "miniupnpc:miniupnpc-2.0.20170509.tar.gz:${MU_H}" \
  "bdb:db-5.3.28.NC.tar.gz:${BDB_H}" \
  "qt:qtbase-opensource-src-5.7.1.tar.gz:${QT_BASE_H}"
do
  IFS=: read -r pkg file hash <<<"$pair"
  (cd "${SRC}" && echo "${hash}  ${file}" > "download-stamps/.stamp_fetched-${pkg}-${file}.hash")
done

echo "Seeded sources in ${SRC}:"
ls -lah "${SRC}" | head -40
