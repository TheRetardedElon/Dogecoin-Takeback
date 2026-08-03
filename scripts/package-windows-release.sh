#!/usr/bin/env bash
# Package Windows PE binaries into zip + optional NSIS installer for GitHub release.
set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

BUILD_ROOT="${BUILD_ROOT:-$HOME/dogedev-winbuild}"
SRC_WIN="/mnt/c/dogedev"
OUT_WIN="${SRC_WIN}/release"
REL_NAME="dogecoin-1.14.100-win64"
VERSION="1.14.100"

HOST="x86_64-w64-mingw32"
log() { echo "[package $(date +%H:%M:%S)] $*"; }

cd "${BUILD_ROOT}"
test -f src/qt/dogecoin-qt.exe
test -f src/dogecoind.exe

mkdir -p "${OUT_WIN}" "${BUILD_ROOT}/release" "${BUILD_ROOT}/release-staging/${REL_NAME}"

# Strip release copies into BUILD_ROOT/release (NSIS expects these paths)
log "Staging stripped binaries"
for src in \
  src/qt/dogecoin-qt.exe \
  src/dogecoind.exe \
  src/dogecoin-cli.exe \
  src/dogecoin-tx.exe
do
  base="$(basename "$src")"
  cp -f "$src" "${BUILD_ROOT}/release/${base}"
  ${HOST}-strip -s "${BUILD_ROOT}/release/${base}" || true
done

# Portable zip layout (similar to official win64)
STAGE="${BUILD_ROOT}/release-staging/${REL_NAME}"
rm -rf "${STAGE}"
mkdir -p "${STAGE}/bin" "${STAGE}/daemon" "${STAGE}/doc"

cp -f "${BUILD_ROOT}/release/dogecoin-qt.exe" "${STAGE}/"
cp -f "${BUILD_ROOT}/release/dogecoind.exe" "${STAGE}/daemon/"
cp -f "${BUILD_ROOT}/release/dogecoin-cli.exe" "${STAGE}/bin/"
cp -f "${BUILD_ROOT}/release/dogecoin-tx.exe" "${STAGE}/bin/"
cp -f "${BUILD_ROOT}/COPYING" "${STAGE}/COPYING.txt" 2>/dev/null || true
if [[ -f "${BUILD_ROOT}/doc/README_windows.txt" ]]; then
  cp -f "${BUILD_ROOT}/doc/README_windows.txt" "${STAGE}/readme.txt"
elif [[ -f "${BUILD_ROOT}/README.md" ]]; then
  cp -f "${BUILD_ROOT}/README.md" "${STAGE}/readme.txt"
fi

ZIP_PATH="${OUT_WIN}/${REL_NAME}.zip"
rm -f "${ZIP_PATH}"
(
  cd "${BUILD_ROOT}/release-staging"
  find "${REL_NAME}" -type f | sort | zip -X@ "${ZIP_PATH}"
)
log "Wrote ${ZIP_PATH} ($(du -h "${ZIP_PATH}" | awk '{print $1}'))"

# NSIS installer via make deploy if possible
SETUP_DST="${OUT_WIN}/${REL_NAME}-setup.exe"
if [[ -f share/setup.nsi ]] && command -v makensis >/dev/null; then
  log "Building NSIS installer"
  # Ensure setup.nsi paths point at our tree; make deploy regenerates it when configured
  make deploy 2>&1 | tail -40 || true
fi

# Prefer exact versioned setup; avoid multi-glob cp (fails when >1 match)
SETUP_SRC=""
if [[ -f "${BUILD_ROOT}/${REL_NAME}-setup.exe" ]]; then
  SETUP_SRC="${BUILD_ROOT}/${REL_NAME}-setup.exe"
elif [[ -f "${REL_NAME}-setup.exe" ]]; then
  SETUP_SRC="${REL_NAME}-setup.exe"
else
  # Manual makensis if OutFile is absolute to abs_top_srcdir
  if [[ -f share/setup.nsi ]]; then
    makensis -V2 share/setup.nsi || true
  fi
  if [[ -f "${REL_NAME}-setup.exe" ]]; then
    SETUP_SRC="${REL_NAME}-setup.exe"
  elif [[ -f "${BUILD_ROOT}/${REL_NAME}-setup.exe" ]]; then
    SETUP_SRC="${BUILD_ROOT}/${REL_NAME}-setup.exe"
  fi
fi

if [[ -n "${SETUP_SRC}" ]]; then
  cp -f "${SETUP_SRC}" "${SETUP_DST}"
  log "Wrote ${SETUP_DST} ($(du -h "${SETUP_DST}" | awk '{print $1}'))"
else
  log "WARN: NSIS setup not produced; zip is still usable"
fi

# Checksums
(
  cd "${OUT_WIN}"
  sha256sum ${REL_NAME}.zip ${REL_NAME}-setup.exe 2>/dev/null | tee SHA256SUMS-win64.txt || \
  sha256sum ${REL_NAME}.zip | tee SHA256SUMS-win64.txt
)

cat > "${OUT_WIN}/RELEASE_NOTES_win64.md" <<EOF
# Dogecoin Core Pro / Takeback — Windows x64

## Version
- Client: **${VERSION}** (Pro / Takeback tree)
- Target: Windows 64-bit (x86_64)
- Built: WSL2 cross-compile (depends + MinGW-w64 posix, Qt 5.7.1)

## Artifacts
- \`${REL_NAME}.zip\` — portable package
- \`${REL_NAME}-setup.exe\` — NSIS installer (if present)
- \`SHA256SUMS-win64.txt\` — checksums

## Zip layout
\`\`\`
${REL_NAME}/
  dogecoin-qt.exe      # GUI wallet
  daemon/dogecoind.exe # full node daemon
  bin/dogecoin-cli.exe
  bin/dogecoin-tx.exe
  COPYING.txt
  readme.txt
\`\`\`

## Install
### Installer
1. Run \`${REL_NAME}-setup.exe\`
2. Launch **Dogecoin Core** from the Start Menu

### Portable
1. Extract the zip
2. Run \`dogecoin-qt.exe\`

## Notes
- First sync downloads the full Dogecoin chain (or enable pruned mode in Options).
- Windows SmartScreen may warn on **unsigned** builds — expected until code-signed.
- Pure DOGE client (no EVM). Pro features: Home, Meme Stream, Business, Network, Options, Arcade.
- Arcade: Retr-Doge Shibe Blaster — retro client-side mini-game (no wallet/network).

## Verify
\`\`\`
sha256sum -c SHA256SUMS-win64.txt
\`\`\`
EOF

log "Artifacts:"
ls -lah "${OUT_WIN}"
log "Done"
