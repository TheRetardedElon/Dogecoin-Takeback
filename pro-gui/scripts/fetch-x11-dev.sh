#!/usr/bin/env bash
# Extract X11 GLFW deps into $HOME/x11-prefix without root (apt download + dpkg-deb -x).
set -euo pipefail
PREFIX="${HOME}/x11-prefix"
WORKDIR="${HOME}/x11-debs"
mkdir -p "$PREFIX" "$WORKDIR"
cd "$WORKDIR"

PKGS=(
  libxrandr-dev
  libxinerama-dev
  libxcursor-dev
  libxi-dev
  libxext-dev
  libx11-dev
  libxrender-dev
  libxcb1-dev
  x11proto-dev
  libxfixes-dev
  libgl-dev
  libgl1-mesa-dev
  libglu1-mesa-dev
  mesa-common-dev
  libxrandr2
  libxinerama1
  libxcursor1
  libxi6
)

echo "Downloading debs…"
apt-get download "${PKGS[@]}" 2>&1 | tail -20 || true

for deb in *.deb; do
  [[ -f "$deb" ]] || continue
  echo "extract $deb"
  dpkg-deb -x "$deb" "$PREFIX"
done

echo "PREFIX=$PREFIX"
ls "$PREFIX/usr/include/X11" 2>/dev/null | head || ls "$PREFIX" | head
