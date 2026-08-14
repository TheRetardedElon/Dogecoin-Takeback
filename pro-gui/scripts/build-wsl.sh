#!/usr/bin/env bash
# Build dogecoin-pro-gui under WSL with portable CMake + FetchContent GLFW.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="${HOME}/tools"
CMAKE_BIN="${TOOLS}/cmake/bin/cmake"

if [[ ! -x "$CMAKE_BIN" ]]; then
  echo "Installing portable CMake into ${TOOLS}/cmake …"
  mkdir -p "$TOOLS"
  cd "$TOOLS"
  curl -fsSL -o cmake.tgz \
    "https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-x86_64.tar.gz"
  tar -xzf cmake.tgz
  rm -f cmake.tgz
  rm -rf cmake
  mv cmake-3.30.5-linux-x86_64 cmake
fi

export PATH="$(dirname "$CMAKE_BIN"):${PATH}"
echo "Using: $(command -v cmake)"
cmake --version | head -1

# Optional user-local X11 headers (from scripts/fetch-x11-dev.sh)
X11P="${HOME}/x11-prefix"
if [[ -d "${X11P}/usr/include" ]]; then
  export CMAKE_PREFIX_PATH="${X11P}/usr${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}"
  export CPATH="${X11P}/usr/include${CPATH:+:${CPATH}}"
  export LIBRARY_PATH="${X11P}/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:${LIBRARY_PATH}}"
  export PKG_CONFIG_PATH="${X11P}/usr/lib/x86_64-linux-gnu/pkgconfig:${X11P}/usr/share/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
  export LD_LIBRARY_PATH="${X11P}/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
  echo "Using X11 prefix: $X11P"
fi

cd "$ROOT"
[[ -f third_party/imgui/imgui.h ]] || {
  echo "FATAL: third_party/imgui missing (docking branch)"
  exit 1
}

# Clean reconfigure if previous failed cache exists
if [[ -f build/CMakeCache.txt ]] && ! grep -q 'CMAKE_PROJECT_NAME:STATIC=dogecoin-pro-gui' build/CMakeCache.txt 2>/dev/null; then
  rm -rf build
fi
# Always clear if last configure failed mid-way
if [[ -d build ]] && [[ ! -f build/Makefile ]] && [[ ! -f build/build.ninja ]]; then
  rm -rf build
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DGLFW_BUILD_WAYLAND=OFF \
  -DGLFW_BUILD_X11=ON \
  -DCMAKE_PREFIX_PATH="${X11P}/usr" \
  -DCMAKE_CXX_FLAGS="-I${X11P}/usr/include" \
  -DCMAKE_C_FLAGS="-I${X11P}/usr/include"
cmake --build build -j"$(nproc 2>/dev/null || echo 4)"
echo "OK: ${ROOT}/build/dogecoin-pro-gui"
ls -lh build/dogecoin-pro-gui
