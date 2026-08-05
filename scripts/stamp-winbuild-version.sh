#!/usr/bin/env bash
set -euo pipefail
cd /home/theretardedelon/dogedev-winbuild
./config.status src/config/dogecoin-config.h
python3 <<'PY'
from pathlib import Path
import re
p = Path('src/config/dogecoin-config.h')
t = p.read_text()
t = re.sub(r'#define CLIENT_VERSION_REVISION \d+', '#define CLIENT_VERSION_REVISION 101', t)
t = re.sub(r'#define CLIENT_VERSION_MINOR \d+', '#define CLIENT_VERSION_MINOR 14', t)
t = re.sub(r'#define CLIENT_VERSION_MAJOR \d+', '#define CLIENT_VERSION_MAJOR 1', t)
t = re.sub(r'#define CLIENT_VERSION_BUILD \d+', '#define CLIENT_VERSION_BUILD 0', t)
t = re.sub(r'#define CLIENT_VERSION_IS_RELEASE \w+', '#define CLIENT_VERSION_IS_RELEASE 1', t)
t = re.sub(r'#define PACKAGE_VERSION "[^"]*"', '#define PACKAGE_VERSION "1.14.101"', t)
t = re.sub(r'#define PACKAGE_STRING "[^"]*"', '#define PACKAGE_STRING "Dogecoin Core 1.14.101"', t)
# any leftover package version strings
t = t.replace('1.14.100', '1.14.101').replace('1.14.99', '1.14.101')
p.write_text(t)
print('stamped ok')
PY
grep -E 'CLIENT_VERSION_|PACKAGE_VERSION|PACKAGE_STRING|HAVE_BYTESWAP' src/config/dogecoin-config.h | head -25

# Also stamp clientversion.h in src if present
if [[ -f src/clientversion.h ]]; then
  python3 <<'PY'
from pathlib import Path
import re
p = Path('src/clientversion.h')
t = p.read_text()
t = re.sub(r'#define CLIENT_VERSION_REVISION \d+', '#define CLIENT_VERSION_REVISION 101', t)
t = re.sub(r'#define CLIENT_VERSION_IS_RELEASE \w+', '#define CLIENT_VERSION_IS_RELEASE true', t)
p.write_text(t)
print('clientversion.h stamped')
PY
fi

# setup.nsi
if [[ -f share/setup.nsi ]]; then
  sed -i 's/!define VERSION .*/!define VERSION 1.14.101/' share/setup.nsi
  grep VERSION share/setup.nsi | head -3
fi
