#!/usr/bin/env python3
from pathlib import Path
import sys

p = Path(sys.argv[1] if len(sys.argv) > 1 else "src/config/dogecoin-config.h")
t = p.read_text(encoding="utf-8", errors="replace")
t = t.replace("#define CLIENT_VERSION_REVISION 99", "#define CLIENT_VERSION_REVISION 100")
t = t.replace("#define COPYRIGHT_YEAR 2024", "#define COPYRIGHT_YEAR 2026")
t = t.replace("Dogecoin Core 1.14.99", "Dogecoin Core 1.14.100")
t = t.replace('#define PACKAGE_VERSION "1.14.99"', '#define PACKAGE_VERSION "1.14.100"')
t = t.replace(
    "https://github.com/dogecoin/dogecoin/issues",
    "https://github.com/TheRetardedElon/Dogecoin-Takeback/issues",
)
t = t.replace("https://dogecoin.com/", "https://github.com/TheRetardedElon/Dogecoin-Takeback")
# If already 1.14.100 keep it; also force revision if configure wrote something else
import re
t = re.sub(r"#define CLIENT_VERSION_REVISION \d+", "#define CLIENT_VERSION_REVISION 100", t)
t = re.sub(r"#define COPYRIGHT_YEAR \d+", "#define COPYRIGHT_YEAR 2026", t)
t = re.sub(r'#define PACKAGE_VERSION "[^"]+"', '#define PACKAGE_VERSION "1.14.100"', t)
t = re.sub(r'#define PACKAGE_STRING "[^"]+"', '#define PACKAGE_STRING "Dogecoin Core 1.14.100"', t)
p.write_text(t, encoding="utf-8")
print(p, "stamped")
assert "CLIENT_VERSION_REVISION 100" in t
assert "COPYRIGHT_YEAR 2026" in t
