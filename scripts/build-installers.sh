#!/usr/bin/env bash
# Build Linux .deb and/or Windows NSIS installer (Client / Server / Hybrid).
# Run under WSL.
set -euo pipefail
export PATH="/usr/bin:/bin:/usr/local/bin:${PATH:-}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${VERSION:-1.14.104}"
DO_DEB="${DO_DEB:-1}"
DO_NSIS="${DO_NSIS:-1}"

echo "==> Core Pro installers VERSION=${VERSION}"

if [[ "$DO_DEB" == "1" ]]; then
  if command -v dpkg-deb >/dev/null 2>&1; then
    echo "==> Linux .deb"
    VERSION="$VERSION" bash "${ROOT}/deploy/debian/build-deb.sh"
  else
    echo "WARN: dpkg-deb not found; skip .deb (install dpkg-dev)"
  fi
fi

if [[ "$DO_NSIS" == "1" ]]; then
  if command -v makensis >/dev/null 2>&1; then
    echo "==> Windows NSIS"
    VERSION="$VERSION" bash "${ROOT}/scripts/build-win-installer.sh"
  else
    echo "WARN: makensis not found; skip NSIS (sudo apt install nsis)"
  fi
fi

echo "==> done"
ls -lh "${ROOT}/release/debian/dogecoin-core-pro_${VERSION}-1_amd64.deb" 2>/dev/null || true
ls -lh "${ROOT}/release/dogecoin-${VERSION}-win64-setup-rpcsecure.exe" 2>/dev/null || true
