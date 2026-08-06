#!/usr/bin/env bash
# Prefetch depends source tarballs so SSL/404 mirrors don't kill the build.
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
SRC=/home/theretardedelon/dogedev-winbuild/depends/sources
mkdir -p "$SRC"
cd "$SRC"

download() {
  local name="$1" url="$2" expect="$3"
  if [[ -f "$name" ]]; then
    local got
    got=$(sha256sum "$name" | awk '{print $1}')
    if [[ "$got" == "$expect" ]]; then
      echo "OK have $name"
      return 0
    fi
    echo "WARN hash mismatch $name, re-download"
    rm -f "$name"
  fi
  echo "GET $name from $url"
  if curl -fL --retry 3 --connect-timeout 30 -o "$name.part" "$url"; then
    mv -f "$name.part" "$name"
  elif curl -fkL --retry 3 --connect-timeout 30 -o "$name.part" "$url"; then
    mv -f "$name.part" "$name"
  else
    rm -f "$name.part"
    return 1
  fi
  echo "$expect  $name" | sha256sum -c -
  echo "OK $name"
}

# zlib (primary host 404s; dogecoincore cert expired)
download zlib-1.3.tar.gz \
  "https://github.com/madler/zlib/releases/download/v1.3/zlib-1.3.tar.gz" \
  ff0ba4c292013dbc27530b3a81e1f9a813cd39de01ca5e0f8bf355702efa593e

# Berkeley DB
download db-5.3.28.NC.tar.gz \
  "https://download.oracle.com/berkeley-db/db-5.3.28.NC.tar.gz" \
  76a25560d9e52a198d37a31440fd07632b5f1f8f9f2b6d5438f4bc3e7c9013ef \
  || download db-5.3.28.NC.tar.gz \
  "https://ftp.debian.org/debian/pool/main/d/db5.3/db5.3_5.3.28.orig.tar.gz" \
  76a25560d9e52a198d37a31440fd07632b5f1f8f9f2b6d5438f4bc3e7c9013ef \
  || download db-5.3.28.NC.tar.gz \
  "http://download.oracle.com/berkeley-db/db-5.3.28.NC.tar.gz" \
  76a25560d9e52a198d37a31440fd07632b5f1f8f9f2b6d5438f4bc3e7c9013ef

# miniupnpc — try several mirrors
download miniupnpc-2.0.20170509.tar.gz \
  "http://miniupnp.free.fr/files/miniupnpc-2.0.20170509.tar.gz" \
  d3c368627f5cdfb66d3ebd64ca39ba54d6ff14a61966dbecb8dd296b7039f16a \
  || download miniupnpc-2.0.20170509.tar.gz \
  "https://miniupnp.tuxfamily.org/files/download.php?file=miniupnpc-2.0.20170509.tar.gz" \
  d3c368627f5cdfb66d3ebd64ca39ba54d6ff14a61966dbecb8dd296b7039f16a \
  || download miniupnpc-2.0.20170509.tar.gz \
  "https://depends.dogecoincore.org/miniupnpc-2.0.20170509.tar.gz" \
  d3c368627f5cdfb66d3ebd64ca39ba54d6ff14a61966dbecb8dd296b7039f16a

# Qt 5.7.1 submodules
QTBASE=http://download.qt.io/new_archive/qt/5.7/5.7.1/submodules
download qtbase-opensource-src-5.7.1.tar.gz \
  "${QTBASE}/qtbase-opensource-src-5.7.1.tar.gz" \
  95f83e532d23b3ddbde7973f380ecae1bac13230340557276f75f2e37984e410 \
  || download qtbase-opensource-src-5.7.1.tar.gz \
  "https://download.qt.io/new_archive/qt/5.7/5.7.1/submodules/qtbase-opensource-src-5.7.1.tar.gz" \
  95f83e532d23b3ddbde7973f380ecae1bac13230340557276f75f2e37984e410

download qttranslations-opensource-src-5.7.1.tar.gz \
  "${QTBASE}/qttranslations-opensource-src-5.7.1.tar.gz" \
  3a15aebd523c6d89fb97b2d3df866c94149653a26d27a00aac9b6d3020bc5a1d \
  || download qttranslations-opensource-src-5.7.1.tar.gz \
  "https://download.qt.io/new_archive/qt/5.7/5.7.1/submodules/qttranslations-opensource-src-5.7.1.tar.gz" \
  3a15aebd523c6d89fb97b2d3df866c94149653a26d27a00aac9b6d3020bc5a1d

download qttools-opensource-src-5.7.1.tar.gz \
  "${QTBASE}/qttools-opensource-src-5.7.1.tar.gz" \
  22d67de915cb8cd93e16fdd38fa006224ad9170bd217c2be1e53045a8dd02f0f \
  || download qttools-opensource-src-5.7.1.tar.gz \
  "https://download.qt.io/new_archive/qt/5.7/5.7.1/submodules/qttools-opensource-src-5.7.1.tar.gz" \
  22d67de915cb8cd93e16fdd38fa006224ad9170bd217c2be1e53045a8dd02f0f

echo "=== sources ready ==="
ls -lah
echo PREFETCH_OK
