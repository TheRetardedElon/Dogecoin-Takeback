#!/usr/bin/env bash
# Build dogecoin-core-pro_${VERSION}-1_${ARCH}.deb from staged Linux binaries.
# Run on Linux/WSL with dpkg-deb installed.
#
# Asks the same Client / Server / Hybrid question as the Windows NSIS installer
# (via debconf templates + config).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${HERE}/../.." && pwd)"
VERSION="${VERSION:-1.14.105}"
ARCH="${ARCH:-amd64}"
PKG_REL="${PKG_REL:-1}"
OUT_DIR="${OUT_DIR:-${ROOT}/release/debian}"
STAGE="${OUT_DIR}/stage-dogecoin-core-pro"
DEB_NAME="dogecoin-core-pro_${VERSION}-${PKG_REL}_${ARCH}.deb"

BIN_DIR="${BIN_DIR:-}"
LIB_DIR="${LIB_DIR:-}"
GPENODE="${GPENODE:-/mnt/c/dogedevGPEnode}"
if [[ ! -d "$GPENODE" && -d /mnt/c/dogedevGPEnode ]]; then
  GPENODE=/mnt/c/dogedevGPEnode
fi
if [[ ! -d "$GPENODE" && -d "C:/dogedevGPEnode" ]]; then
  GPENODE="C:/dogedevGPEnode"
fi

if [[ -z "$BIN_DIR" ]]; then
  for cand in \
    "${ROOT}/src" \
    "${ROOT}/out/dump-daemon/bin" \
    "${GPENODE}/out/dump-daemon/bin" \
    "${GPENODE}/out/headless-release/dogecoin-gpenode-linux-x86_64-"*/bin \
    "${GPENODE}/src"
  do
    if [[ -x "${cand}/dogecoind" ]]; then
      BIN_DIR="$cand"
      break
    fi
  done
fi

if [[ -n "$BIN_DIR" && ! -d "$BIN_DIR" ]]; then
  # shellcheck disable=SC2086
  BIN_DIR="$(ls -d $BIN_DIR 2>/dev/null | head -1 || true)"
fi

need() { command -v "$1" >/dev/null || { echo "missing $1"; exit 1; }; }
need dpkg-deb

[[ -n "$BIN_DIR" && -d "$BIN_DIR" ]] || {
  echo "ERROR: set BIN_DIR to a directory containing dogecoind and dogecoin-cli"
  echo "  export BIN_DIR=/path/to/bin"
  exit 1
}

find_bin() {
  local n="$1"
  if [[ -x "${BIN_DIR}/${n}" ]]; then echo "${BIN_DIR}/${n}"; return 0; fi
  if [[ -x "${ROOT}/src/${n}" ]]; then echo "${ROOT}/src/${n}"; return 0; fi
  if [[ -x "${GPENODE}/gpenode-ops/${n}" ]]; then echo "${GPENODE}/gpenode-ops/${n}"; return 0; fi
  if [[ -x "${GPENODE}/gpenode-ops/${n}-linux-amd64" ]]; then echo "${GPENODE}/gpenode-ops/${n}-linux-amd64"; return 0; fi
  if [[ -x "${GPENODE}/gpenode-tui/${n}" ]]; then echo "${GPENODE}/gpenode-tui/${n}"; return 0; fi
  if [[ -x "${GPENODE}/gpenode-tray/${n}" ]]; then echo "${GPENODE}/gpenode-tray/${n}"; return 0; fi
  if [[ -x "${GPENODE}/out/dump-daemon/bin/${n}" ]]; then echo "${GPENODE}/out/dump-daemon/bin/${n}"; return 0; fi
  return 1
}

DOGECOIND="$(find_bin dogecoind || true)"
CLI="$(find_bin dogecoin-cli || true)"
OPS="$(find_bin gpenode-ops || true)"
TUI="$(find_bin gpenode-tui || true)"
TRAY="$(find_bin gpenode-tray || true)"
if [[ -z "$TRAY" && -x "${GPENODE}/gpenode-tray/gpenode-tray" ]]; then
  TRAY="${GPENODE}/gpenode-tray/gpenode-tray"
fi
GUI=""
for g in \
  "${ROOT}/pro-gui/build/dogecoin-pro-gui" \
  "${BIN_DIR}/dogecoin-pro-gui" \
  "${ROOT}/release/dogecoin-pro-gui"
do
  if [[ -x "$g" ]]; then GUI="$g"; break; fi
done

[[ -n "$DOGECOIND" && -x "$DOGECOIND" ]] || { echo "missing dogecoind in BIN_DIR=$BIN_DIR"; exit 1; }
[[ -n "$CLI" && -x "$CLI" ]] || { echo "missing dogecoin-cli"; exit 1; }

if [[ -z "$LIB_DIR" ]]; then
  parent="$(dirname "$BIN_DIR")"
  if [[ -d "${parent}/lib" ]]; then LIB_DIR="${parent}/lib"; fi
  if [[ -z "$LIB_DIR" && -d "${GPENODE}/out/headless-release/dogecoin-gpenode-linux-x86_64-1.14.102-gpenode1/lib" ]]; then
    LIB_DIR="${GPENODE}/out/headless-release/dogecoin-gpenode-linux-x86_64-1.14.102-gpenode1/lib"
  fi
fi
# Harvest Boost 1.90 + miniupnpc from the build machine when no staged lib dir.
if [[ -z "$LIB_DIR" || ! -d "$LIB_DIR" ]]; then
  HARVEST="/tmp/corepro-libs-$$"
  mkdir -p "$HARVEST"
  for so in libboost_filesystem.so.1.90.0 libboost_program_options.so.1.90.0 \
            libboost_thread.so.1.90.0 libboost_chrono.so.1.90.0 libminiupnpc.so.21; do
    if [[ -f "/usr/lib/x86_64-linux-gnu/${so}" ]]; then
      cp -a "/usr/lib/x86_64-linux-gnu/${so}" "$HARVEST/"
    fi
  done
  if ls "$HARVEST"/libboost_*.so* >/dev/null 2>&1; then
    LIB_DIR="$HARVEST"
  fi
fi

echo "==> binaries"
echo "    dogecoind: $DOGECOIND"
echo "    cli:       $CLI"
echo "    ops:       ${OPS:-<none>}"
echo "    tui:       ${TUI:-<none>}"
echo "    tray:      ${TRAY:-<none>}"
echo "    gui:       ${GUI:-<none - Client launcher will fall back>}"
echo "    libs:      ${LIB_DIR:-<none>}"

echo "==> staging package tree"
rm -rf "$STAGE"
mkdir -p \
  "${STAGE}/DEBIAN" \
  "${STAGE}/usr/bin" \
  "${STAGE}/usr/lib/dogecoin-core-pro" \
  "${STAGE}/usr/share/dogecoin-core-pro/conf" \
  "${STAGE}/usr/share/doc/dogecoin-core-pro" \
  "${STAGE}/usr/share/pixmaps" \
  "${STAGE}/usr/share/applications" \
  "${STAGE}/etc/xdg/autostart" \
  "${STAGE}/lib/systemd/system" \
  "${STAGE}/etc/dogecoin-core-pro"

# Real ELF next to bundled Boost; thin wrappers on PATH set LD_LIBRARY_PATH.
install -m 755 "$DOGECOIND" "${STAGE}/usr/lib/dogecoin-core-pro/dogecoind"
install -m 755 "$CLI" "${STAGE}/usr/lib/dogecoin-core-pro/dogecoin-cli"
if command -v strip >/dev/null 2>&1; then
  strip --strip-unneeded \
    "${STAGE}/usr/lib/dogecoin-core-pro/dogecoind" \
    "${STAGE}/usr/lib/dogecoin-core-pro/dogecoin-cli" 2>/dev/null || true
fi
install -m 755 "${HERE}/dogecoind-wrapper" "${STAGE}/usr/bin/dogecoind"
install -m 755 "${HERE}/dogecoin-cli-wrapper" "${STAGE}/usr/bin/dogecoin-cli"
if [[ -n "$OPS" ]]; then
  install -m 755 "$OPS" "${STAGE}/usr/bin/gpenode-ops"
fi
if [[ -n "$TUI" ]]; then
  install -m 755 "$TUI" "${STAGE}/usr/bin/gpenode-tui"
fi
if [[ -n "$TRAY" ]]; then
  install -m 755 "$TRAY" "${STAGE}/usr/bin/gpenode-tray"
fi
if [[ -n "$GUI" ]]; then
  install -m 755 "$GUI" "${STAGE}/usr/bin/dogecoin-pro-gui"
  if [[ -d "${ROOT}/pro-gui/assets" ]]; then
    mkdir -p "${STAGE}/usr/share/dogecoin-core-pro/assets"
    cp -a "${ROOT}/pro-gui/assets/." "${STAGE}/usr/share/dogecoin-core-pro/assets/"
  fi
fi

install -m 755 "${HERE}/dogecoin-core-pro" "${STAGE}/usr/bin/dogecoin-core-pro"

if [[ -n "$LIB_DIR" && -d "$LIB_DIR" ]]; then
  cp -a "${LIB_DIR}/." "${STAGE}/usr/lib/dogecoin-core-pro/" 2>/dev/null || true
fi

install -m 644 "${HERE}/dogecoin-core-pro.service" \
  "${STAGE}/lib/systemd/system/dogecoin-core-pro.service"
install -m 755 "${HERE}/gen-rpc-password.sh" \
  "${STAGE}/usr/share/dogecoin-core-pro/gen-rpc-password.sh"
install -m 755 "${HERE}/write-install-conf.sh" \
  "${STAGE}/usr/share/dogecoin-core-pro/write-install-conf.sh"
install -m 644 "${HERE}/dogecoin-core-pro.desktop" \
  "${STAGE}/usr/share/applications/dogecoin-core-pro.desktop"
if [[ -f "${HERE}/dogecoin-core-pro-testnet.desktop" ]]; then
  install -m 644 "${HERE}/dogecoin-core-pro-testnet.desktop" \
    "${STAGE}/usr/share/applications/dogecoin-core-pro-testnet.desktop"
fi
if [[ -f "${HERE}/dogecoin-core-pro-tray.desktop" ]]; then
  install -m 644 "${HERE}/dogecoin-core-pro-tray.desktop" \
    "${STAGE}/usr/share/applications/dogecoin-core-pro-tray.desktop"
fi
install -m 644 "${HERE}/preseed.example" \
  "${STAGE}/usr/share/doc/dogecoin-core-pro/preseed.example"

if [[ -f "${ROOT}/share/pixmaps/dogecoin128.png" ]]; then
  install -m 644 "${ROOT}/share/pixmaps/dogecoin128.png" \
    "${STAGE}/usr/share/pixmaps/dogecoin.png"
fi
if [[ -f "${ROOT}/share/pixmaps/dogecoin-testnet256.png" ]]; then
  install -m 644 "${ROOT}/share/pixmaps/dogecoin-testnet256.png" \
    "${STAGE}/usr/share/pixmaps/dogecoin-testnet.png"
fi
if [[ -f "${ROOT}/doc/install-roles.md" ]]; then
  install -m 644 "${ROOT}/doc/install-roles.md" \
    "${STAGE}/usr/share/doc/dogecoin-core-pro/install-roles.md"
fi
if [[ -f "${ROOT}/COPYING" ]]; then
  install -m 644 "${ROOT}/COPYING" \
    "${STAGE}/usr/share/doc/dogecoin-core-pro/copyright"
fi

if [[ -f "${GPENODE}/deploy/conf/dogecoin.dump.conf.example" ]]; then
  install -m 644 "${GPENODE}/deploy/conf/dogecoin.dump.conf.example" \
    "${STAGE}/usr/share/dogecoin-core-pro/conf/" || true
  install -m 644 "${GPENODE}/deploy/conf/dogecoin.settlement.conf.example" \
    "${STAGE}/usr/share/dogecoin-core-pro/conf/" 2>/dev/null || true
fi

sed -e "s/__VERSION__/${VERSION}/g" -e "s/__ARCH__/${ARCH}/g" \
  "${HERE}/DEBIAN/control.in" > "${STAGE}/DEBIAN/control"
install -m 755 "${HERE}/DEBIAN/postinst.in" "${STAGE}/DEBIAN/postinst"
install -m 755 "${HERE}/DEBIAN/prerm.in" "${STAGE}/DEBIAN/prerm"
install -m 755 "${HERE}/DEBIAN/postrm.in" "${STAGE}/DEBIAN/postrm"
install -m 755 "${HERE}/config" "${STAGE}/DEBIAN/config"
install -m 644 "${HERE}/templates" "${STAGE}/DEBIAN/templates"

# strip CR if scripts were edited on Windows
sed -i 's/\r$//' \
  "${STAGE}/DEBIAN/postinst" "${STAGE}/DEBIAN/prerm" "${STAGE}/DEBIAN/postrm" \
  "${STAGE}/DEBIAN/config" "${STAGE}/DEBIAN/templates" "${STAGE}/DEBIAN/control" \
  "${STAGE}/usr/bin/dogecoin-core-pro" \
  "${STAGE}/usr/bin/dogecoind" "${STAGE}/usr/bin/dogecoin-cli" \
  "${STAGE}/usr/share/dogecoin-core-pro/"*.sh \
  "${STAGE}/lib/systemd/system/dogecoin-core-pro.service" 2>/dev/null || true

chmod 0755 "${STAGE}/DEBIAN" 2>/dev/null || true
chmod 0644 "${STAGE}/DEBIAN/control" "${STAGE}/DEBIAN/templates" 2>/dev/null || true
chmod 0755 "${STAGE}/DEBIAN/postinst" "${STAGE}/DEBIAN/prerm" \
  "${STAGE}/DEBIAN/postrm" "${STAGE}/DEBIAN/config" 2>/dev/null || true

cat > "${STAGE}/usr/share/doc/dogecoin-core-pro/README.Debian" <<EOF
dogecoin-core-pro ${VERSION}

First install asks: Client, Server, or Hybrid (debconf).
Reconfigure: sudo dpkg-reconfigure dogecoin-core-pro
Preseed:     see preseed.example in this directory.

Client:
  run dogecoin-core-pro (or dogecoin-pro-gui)
  datadir ~/.dogecoin
  systemd unit is installed but not enabled

Server / Hybrid:
  sudo systemctl status dogecoin-core-pro
  gpenode-ops status
  gpenode-tui
  sudo cat /var/lib/dogecoin-core-pro/RPC-CREDENTIALS.txt
  Conf:    /etc/dogecoin-core-pro/dogecoin.conf
  Datadir: /var/lib/dogecoin-core-pro

Hybrid launcher: dogecoin-core-pro gfx|tui

One dogecoind. Never run two nodes against the same datadir.

Repo: https://github.com/TheRetardedElon/Dogecoin-Takeback
EOF

mkdir -p "$OUT_DIR"
DEBIAN_MODE="$(stat -c '%a' "${STAGE}/DEBIAN" 2>/dev/null || echo 777)"
BUILD_STAGE="$STAGE"
if [[ "$DEBIAN_MODE" == "777" ]] || [[ "$STAGE" == /mnt/* ]]; then
  BUILD_STAGE="/tmp/dogecoin-core-pro-deb-stage-$$"
  echo "==> restaging under ${BUILD_STAGE} (dpkg-deb needs DEBIAN mode 0755)"
  rm -rf "$BUILD_STAGE"
  mkdir -p "$(dirname "$BUILD_STAGE")"
  cp -a "$STAGE" "$BUILD_STAGE"
  chmod 0755 "${BUILD_STAGE}/DEBIAN"
  chmod 0644 "${BUILD_STAGE}/DEBIAN/control" "${BUILD_STAGE}/DEBIAN/templates"
  chmod 0755 "${BUILD_STAGE}/DEBIAN/postinst" "${BUILD_STAGE}/DEBIAN/prerm" \
    "${BUILD_STAGE}/DEBIAN/postrm" "${BUILD_STAGE}/DEBIAN/config"
fi

echo "==> dpkg-deb --build"
if command -v fakeroot >/dev/null 2>&1; then
  fakeroot dpkg-deb --build "$BUILD_STAGE" "${OUT_DIR}/${DEB_NAME}"
else
  dpkg-deb --build "$BUILD_STAGE" "${OUT_DIR}/${DEB_NAME}"
fi
if [[ "$BUILD_STAGE" != "$STAGE" ]]; then
  rm -rf "$BUILD_STAGE"
fi

if command -v sha256sum >/dev/null 2>&1; then
  ( cd "$OUT_DIR" && sha256sum "$DEB_NAME" > "${DEB_NAME}.sha256" )
elif command -v shasum >/dev/null 2>&1; then
  ( cd "$OUT_DIR" && shasum -a 256 "$DEB_NAME" > "${DEB_NAME}.sha256" )
fi
echo "==> DEB_OK ${OUT_DIR}/${DEB_NAME}"
ls -lh "${OUT_DIR}/${DEB_NAME}" "${OUT_DIR}/${DEB_NAME}.sha256" 2>/dev/null || ls -lh "${OUT_DIR}/${DEB_NAME}"
if [[ -f "${OUT_DIR}/${DEB_NAME}.sha256" ]]; then
  cat "${OUT_DIR}/${DEB_NAME}.sha256"
fi
