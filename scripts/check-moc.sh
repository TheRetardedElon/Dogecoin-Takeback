#!/usr/bin/env bash
export PATH=/usr/bin:/bin
set -x
grep -E '^MOC |^RCC |^UIC ' /home/theretardedelon/dogedev-winbuild/src/Makefile | head
echo NATIVE:
/home/theretardedelon/dogedev-winbuild/depends/x86_64-w64-mingw32/native/bin/moc -v || true
echo SYSTEM:
moc -v || true
ls /usr/lib/*/qt5/bin/moc 2>/dev/null || true
ls /usr/lib/qt5/bin/moc 2>/dev/null || true
which -a moc
