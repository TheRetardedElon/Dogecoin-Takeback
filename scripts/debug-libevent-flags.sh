#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin
B=$(echo /home/theretardedelon/dogedev-winbuild/depends/work/build/x86_64-w64-mingw32/libevent/2.1.12-stable-*/)
echo "B=$B"
echo "=== config.log CFLAGS/CPPFLAGS ==="
grep -E "CPPFLAGS|CFLAGS|WIN32_WINNT|configure:.*cc" "$B/config.log" | head -40
echo "=== Makefile snippet ==="
grep -E "^CPPFLAGS|^CFLAGS|WIN32_WINNT" "$B/Makefile" | head -20
echo "=== host_os from depends ==="
cd /home/theretardedelon/dogedev-winbuild/depends
# print what make thinks host_os is
make HOST=x86_64-w64-mingw32 -p 2>/dev/null | grep -E "^host_os|^host_arch|^libevent_cppflags|^libevent_cflags" | head -30
