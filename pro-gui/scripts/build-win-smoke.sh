#!/usr/bin/env bash
# Cross-compile dogecoin-pro-gui.exe (MinGW) for Windows smoke testing.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/../release/smoke-pro-gui"
TOOLS="${HOME}/tools"
CMAKE_BIN="${TOOLS}/cmake/bin/cmake"
export PATH="$(dirname "$CMAKE_BIN"):/usr/bin:/bin:${PATH:-}"

if [[ ! -x "$CMAKE_BIN" ]]; then
  echo "Need portable cmake (run build-wsl.sh once first)"
  exit 1
fi
command -v x86_64-w64-mingw32-g++ >/dev/null || { echo "need mingw-w64 g++"; exit 1; }
[[ -f "$ROOT/third_party/imgui/imgui.h" ]] || { echo "need third_party/imgui"; exit 1; }

cd "$ROOT"
rm -rf build-win
cmake -B build-win \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DGLFW_BUILD_WAYLAND=OFF \
  -DGLFW_BUILD_X11=OFF \
  -DGLFW_BUILD_WIN32=ON
cmake --build build-win -j"$(nproc 2>/dev/null || echo 4)"

mkdir -p "$OUT"
cp -f build-win/dogecoin-pro-gui.exe "$OUT/dogecoin-pro-gui-smoke.exe"
cp -f build-win/dogecoin-pro-gui.exe "$ROOT/build-win/dogecoin-pro-gui-smoke.exe" 2>/dev/null || true
# WebView2 loader for in-panel arcade cabinet
if [[ -f build-win/WebView2Loader.dll ]]; then
  cp -f build-win/WebView2Loader.dll "$OUT/WebView2Loader.dll"
elif [[ -f third_party/webview2/x64/WebView2Loader.dll ]]; then
  cp -f third_party/webview2/x64/WebView2Loader.dll "$OUT/WebView2Loader.dll"
fi
# dogeinit = optional bootstrap
if [[ -f build-win/dogeinit.exe ]]; then
  cp -f build-win/dogeinit.exe "$OUT/dogeinit.exe"
  echo "Bundled dogeinit.exe"
fi
rm -rf "$OUT/assets"
cp -a assets "$OUT/assets"
if [[ -f "$ROOT/../release/dogecoind.exe" ]]; then
  cp -f "$ROOT/../release/dogecoind.exe" "$OUT/dogecoind.exe"
  echo "Bundled dogecoind.exe into smoke package"
fi
if [[ -f "$ROOT/../release/dogecoin-cli.exe" ]]; then
  cp -f "$ROOT/../release/dogecoin-cli.exe" "$OUT/dogecoin-cli.exe" || true
fi

# Verify no MinGW DLL deps (or document)
echo "=== PE imports (should not list libgcc/libstdc++) ==="
x86_64-w64-mingw32-objdump -p "$OUT/dogecoin-pro-gui-smoke.exe" 2>/dev/null \
  | grep -iE 'DLL Name|libgcc|libstdc|libwinpthread' || true

cat > "$OUT/README-SMOKE.txt" <<EOF
Dogecoin Core Pro - Windows smoke
=================================

Recommended: dogecoin-pro-gui-smoke.exe
  - Shows boot checklist (WHO LET THE DOGE OUT / start node / index / wallet)
  - Auto-starts dogecoind with -server -prune=5500
  - On exit: RPC stop + wait for clean flush (like Qt)

Optional: dogeinit.exe only if you want a console bootstrap before the GUI.

If the node dies: %APPDATA%\\Dogecoin\\debug.log
EOF

ls -lh "$OUT"
echo "WIN_SMOKE_OK $OUT"
