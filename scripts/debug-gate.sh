#!/usr/bin/env bash
set -x
export PATH=/usr/bin:/bin:/usr/local/bin
cd /home/theretardedelon/dogedev-winbuild/src
s='_ZN11DogecoinGUI14gotoArcadePageEv'
x86_64-w64-mingw32-nm qt/dogecoin-qt.exe 2>/dev/null | grep -F "$s" | head -2
echo nm_grep_exit=${PIPESTATUS[0]}/${PIPESTATUS[1]}
if x86_64-w64-mingw32-nm qt/dogecoin-qt.exe 2>/dev/null | grep -Fq "$s"; then
  echo OK_nm
else
  echo FAIL_nm
fi
if strings dogecoind.exe | grep -Eq 'assumeutxo|loadtxoutset|dumptxoutset'; then
  echo OK_assume
else
  echo FAIL_assume
fi
if x86_64-w64-mingw32-nm qt/libdogecoinqt.a 2>/dev/null | grep -q ArcadePage; then
  echo OK_lib
else
  echo FAIL_lib
fi
# hexdump first line of verify script for CR
head -1 /mnt/c/dogedev/scripts/verify-and-package-release.sh | od -c | head -3
# show the for-loop symbols as bash sees them
bash -c '
source /dev/null
for s in \
  _ZN11DogecoinGUI14gotoArcadePageEv \
  _ZN11DogecoinGUI18gotoMemeStreamPageEv
do
  printf "SYM=[%s] len=%s\n" "$s" "${#s}"
  x86_64-w64-mingw32-nm /home/theretardedelon/dogedev-winbuild/src/qt/dogecoin-qt.exe 2>/dev/null | grep -Fq "$s" && echo ok || echo fail
done
'
