#!/usr/bin/env python3
from pathlib import Path
import re

p = Path("/home/theretardedelon/dogedev-winbuild/src/Makefile")
t = p.read_text()

# Drop leftover garbage from bad sed (lines starting with tab + node/utxo_snapshot.cpp'; then)
t = re.sub(r"\n\tnode/utxo_snapshot\.cpp'; then.*?\n", "\n", t)

snap = (
    "node/libdogecoin_server_a-snapshot_fetch.o: node/snapshot_fetch.cpp\n"
    "\t$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) "
    "$(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) "
    "-c -o node/libdogecoin_server_a-snapshot_fetch.o "
    "`test -f 'node/snapshot_fetch.cpp' || echo '$(srcdir)/'`node/snapshot_fetch.cpp\n"
    "\n"
    "node/libdogecoin_server_a-snapshot_fetch.obj: node/snapshot_fetch.cpp\n"
    "\t$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) "
    "$(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) "
    "-c -o node/libdogecoin_server_a-snapshot_fetch.obj "
    "`if test -f 'node/snapshot_fetch.cpp'; then $(CYGPATH_W) 'node/snapshot_fetch.cpp'; "
    "else $(CYGPATH_W) '$(srcdir)/node/snapshot_fetch.cpp'; fi`\n"
)

# Replace any existing snapshot_fetch rule block
t = re.sub(
    r"node/libdogecoin_server_a-snapshot_fetch\.o:.*?node/libdogecoin_server_a-snapshot_fetch\.obj:.*?\n(?:\t.*\n|\$\(AM_V_CXX\).*\n)*",
    snap + "\n",
    t,
    count=1,
    flags=re.S,
)
if "libdogecoin_server_a-snapshot_fetch.o: node/snapshot_fetch.cpp" not in t:
    t = t.replace(
        "node/libdogecoin_server_a-utxo_snapshot.o: node/utxo_snapshot.cpp",
        snap + "\nnode/libdogecoin_server_a-utxo_snapshot.o: node/utxo_snapshot.cpp",
        1,
    )

# Ensure recipe lines for utxo use tabs after #DEPDIR comment block
def tab_recipe(m):
    return m.group(1) + "\t" + m.group(2)

t = re.sub(
    r"(#DEPDIR=\$\(DEPDIR\) \$\(CXXDEPMODE\) \$\(depcomp\) \n)"
    r"(\$\(AM_V_CXX\)\$\(CXX\).*libdogecoin_server_a-utxo_snapshot\.(o|obj))",
    tab_recipe,
    t,
)

p.write_text(t)
print("ok")
# show snap rules
for i, line in enumerate(t.splitlines(), 1):
    if "snapshot_fetch" in line and i > 6300:
        print(i, repr(line[:100]))
