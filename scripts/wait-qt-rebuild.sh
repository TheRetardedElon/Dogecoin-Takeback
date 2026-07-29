#!/usr/bin/env bash
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
PID="${1:-128880}"
LOG=/tmp/qt-ssl-rebuild.log
while kill -0 "$PID" 2>/dev/null; do
  sleep 60
  lines=$(wc -l <"$LOG" 2>/dev/null || echo 0)
  echo "$(date +%H:%M:%S) still building... lines=$lines"
done
echo "=== QT MAKE EXITED ==="
tail -n 50 "$LOG"
echo "=== built packages ==="
ls -la /home/theretardedelon/dogedev-winbuild/depends/built/x86_64-w64-mingw32/ 2>/dev/null || true
if test -f /home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/lib/libQt5Network.a; then
  echo QT_NETWORK_OK
  # Check that SSL symbols exist in QtNetwork
  x86_64-w64-mingw32-nm /home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/lib/libQt5Network.a 2>/dev/null | grep -i QSslSocket | head -5 || true
else
  echo QT_NETWORK_MISSING
fi
# exit status of make from log
if grep -q "error:" "$LOG" 2>/dev/null; then
  echo "NOTE: log contains error: lines (may be false positives)"
fi
if ls /home/theretardedelon/dogedev-winbuild/depends/built/x86_64-w64-mingw32/qt/* >/dev/null 2>&1; then
  echo QT_PACKAGE_BUILT
  exit 0
fi
echo QT_PACKAGE_NOT_BUILT
exit 1
