#!/usr/bin/env bash
# Build the Windows NSIS installer with Client / Server / Hybrid page.
# Run under WSL with makensis installed.
set -euo pipefail
export PATH="/usr/bin:/bin:/usr/local/bin:${PATH:-}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${VERSION:-1.14.105}"
OUT_DIR="${OUT_DIR:-${ROOT}/release}"
NSI="${ROOT}/share/setup-rpcsecure.nsi"
SETUP_NAME="dogecoin-${VERSION}-win64-setup-rpcsecure.exe"

need() { command -v "$1" >/dev/null || { echo "missing $1"; exit 1; }; }
need makensis

[[ -f "$NSI" ]] || { echo "missing $NSI"; exit 1; }
[[ -f "${ROOT}/release/dogecoind.exe" ]] || { echo "missing release/dogecoind.exe"; exit 1; }
[[ -f "${ROOT}/release/dogecoin-cli.exe" ]] || { echo "missing release/dogecoin-cli.exe"; exit 1; }
[[ -f "${ROOT}/share/pixmaps/dogecoin.ico" ]] || { echo "missing share/pixmaps/dogecoin.ico"; exit 1; }

echo "==> binaries that will be packed"
ls -lh "${ROOT}/release/dogecoind.exe" "${ROOT}/release/dogecoin-cli.exe"
ls -lh "${ROOT}/release/smoke-pro-gui/dogecoin-pro-gui-smoke.exe" 2>/dev/null || \
  ls -lh "${ROOT}/pro-gui/build-win/dogecoin-pro-gui.exe" 2>/dev/null || \
  echo "WARN: no ImGui GUI exe found (Client/Hybrid will fall back to Qt if present)"
ls -lh /mnt/c/dogedevGPEnode/gpenode-tui/gpenode-tui.exe 2>/dev/null || \
  echo "WARN: no gpenode-tui.exe (Server/Hybrid TUI missing)"

mkdir -p "$OUT_DIR"
echo "==> makensis $NSI"
# cwd = share so relative File lookups in includes resolve
( cd "${ROOT}/share" && makensis -V3 -DVERSION="${VERSION}" "$NSI" )

OUT_ABS="${OUT_DIR}/${SETUP_NAME}"
if [[ ! -f "$OUT_ABS" ]]; then
  found="$(find "${ROOT}/release" "${ROOT}/share" /tmp -maxdepth 2 -name "${SETUP_NAME}" 2>/dev/null | head -1 || true)"
  if [[ -n "$found" ]]; then
    mv -f "$found" "$OUT_ABS"
  fi
fi

[[ -f "$OUT_ABS" ]] || { echo "ERROR: installer not produced at $OUT_ABS"; exit 1; }

if command -v sha256sum >/dev/null 2>&1; then
  ( cd "$OUT_DIR" && sha256sum "$SETUP_NAME" > "${SETUP_NAME}.sha256" )
fi
echo "==> INSTALLER_OK $OUT_ABS"
ls -lh "$OUT_ABS" "${OUT_ABS}.sha256" 2>/dev/null || ls -lh "$OUT_ABS"
cat "${OUT_ABS}.sha256" 2>/dev/null || true
