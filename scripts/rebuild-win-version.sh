#!/usr/bin/env bash
# Restore mingw cross config if needed, apply version/about sources, rebuild PE, package.
set -euo pipefail
export PATH="/usr/bin:/bin:/usr/sbin:/usr/local/bin"

BUILD_ROOT="${HOME}/dogedev-winbuild"
SRC_WIN="/mnt/c/dogedev"
HOST="x86_64-w64-mingw32"

if [[ -w /proc/sys/fs/binfmt_misc/status ]]; then
  echo 0 > /proc/sys/fs/binfmt_misc/status || true
fi

echo "[rebuild] syncing versioned sources"
cp -f "${SRC_WIN}/configure.ac" "${BUILD_ROOT}/configure.ac"
cp -f "${SRC_WIN}/src/clientversion.h" "${BUILD_ROOT}/src/clientversion.h"
cp -f "${SRC_WIN}/src/init.cpp" "${BUILD_ROOT}/src/init.cpp"
cp -f "${SRC_WIN}/src/qt/thememanager.cpp" "${BUILD_ROOT}/src/qt/thememanager.cpp" 2>/dev/null || true
cp -f "${SRC_WIN}/src/qt/themeswitcher.cpp" "${BUILD_ROOT}/src/qt/themeswitcher.cpp" 2>/dev/null || true
cp -f "${SRC_WIN}/src/qt/res/dogecoin-qt-res.rc" "${BUILD_ROOT}/src/qt/res/dogecoin-qt-res.rc" 2>/dev/null || true

cd "${BUILD_ROOT}"

# Detect if we lost the mingw toolchain (config.status --recheck can wipe it)
need_reconf=0
if ! grep -q 'x86_64-w64-mingw32' src/Makefile 2>/dev/null; then
  need_reconf=1
fi
if ! grep -q 'EXEEXT = \.exe' src/Makefile 2>/dev/null; then
  need_reconf=1
fi

if [[ "$need_reconf" -eq 1 ]]; then
  echo "[rebuild] restoring mingw cross-configure"
  test -f "depends/${HOST}/share/config.site"
  CONFIG_SITE="$PWD/depends/${HOST}/share/config.site" ./configure \
    --prefix=/ \
    --disable-ccache \
    --disable-maintainer-mode \
    --disable-dependency-tracking \
    --with-gui=qt5 \
    --enable-reduce-exports
fi

# Always stamp version macros into generated config header (matches configure.ac)
python3 - <<'PY'
from pathlib import Path
p = Path("src/config/dogecoin-config.h")
t = p.read_text(encoding="utf-8", errors="replace")
repls = {
    "#define CLIENT_VERSION_REVISION 99": "#define CLIENT_VERSION_REVISION 100",
    "#define CLIENT_VERSION_REVISION 100": "#define CLIENT_VERSION_REVISION 100",
    "#define COPYRIGHT_YEAR 2024": "#define COPYRIGHT_YEAR 2026",
    "#define COPYRIGHT_YEAR 2026": "#define COPYRIGHT_YEAR 2026",
    '#define PACKAGE_STRING "Dogecoin Core 1.14.99"': '#define PACKAGE_STRING "Dogecoin Core 1.14.100"',
    '#define PACKAGE_STRING "Dogecoin Core 1.14.100"': '#define PACKAGE_STRING "Dogecoin Core 1.14.100"',
    '#define PACKAGE_VERSION "1.14.99"': '#define PACKAGE_VERSION "1.14.100"',
    '#define PACKAGE_VERSION "1.14.100"': '#define PACKAGE_VERSION "1.14.100"',
    '#define PACKAGE_BUGREPORT "https://github.com/dogecoin/dogecoin/issues"':
        '#define PACKAGE_BUGREPORT "https://github.com/TheRetardedElon/Dogecoin-Takeback/issues"',
    '#define PACKAGE_BUGREPORT "https://github.com/TheRetardedElon/Dogecoin-Takeback/issues"':
        '#define PACKAGE_BUGREPORT "https://github.com/TheRetardedElon/Dogecoin-Takeback/issues"',
    '#define PACKAGE_URL "https://dogecoin.com/"':
        '#define PACKAGE_URL "https://github.com/TheRetardedElon/Dogecoin-Takeback"',
    '#define PACKAGE_URL "https://github.com/TheRetardedElon/Dogecoin-Takeback"':
        '#define PACKAGE_URL "https://github.com/TheRetardedElon/Dogecoin-Takeback"',
}
for a, b in repls.items():
    t = t.replace(a, b)
p.write_text(t, encoding="utf-8")
print("config header stamped")
# sanity
assert "CLIENT_VERSION_REVISION 100" in t
assert "COPYRIGHT_YEAR 2026" in t
assert "1.14.100" in t
assert "Takeback" in t
PY

# Force rebuild of version-bearing units
find src -type f \( -name '*clientversion*.o' -o -name '*init.o' -o -name '*utilitydialog*.o' -o -name '*splashscreen*.o' -o -name 'dogecoin-qt-res.o' \) -delete 2>/dev/null || true
rm -f src/qt/dogecoin-qt.exe src/dogecoind.exe src/dogecoin-cli.exe src/dogecoin-tx.exe

echo "[rebuild] make -j2"
make -j2 2>&1 | tee /tmp/win-rebuild-version.log
file src/qt/dogecoin-qt.exe src/dogecoind.exe
x86_64-w64-mingw32-strings src/qt/dogecoin-qt.exe | grep -E "1\.14\.100|2013-2026|Takeback|2026" | head -30 || true

echo "[rebuild] package"
bash "${SRC_WIN}/scripts/package-windows-release.sh"
echo "[rebuild] done"
