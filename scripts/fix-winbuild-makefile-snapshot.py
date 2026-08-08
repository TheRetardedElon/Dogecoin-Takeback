#!/usr/bin/env python3
"""Repair winbuild src/Makefile after bad sed, add snapshot_fetch object rules."""
from pathlib import Path

p = Path("/home/theretardedelon/dogedev-winbuild/src/Makefile")
text = p.read_text()

# Fix corrupted multi-line utxo_snapshot rules: replace any broken block with clean rules
import re

clean_utxo = r'''node/libdogecoin_server_a-utxo_snapshot.o: node/utxo_snapshot.cpp
#$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -MT node/libdogecoin_server_a-utxo_snapshot.o -MD -MP -MF node/$(DEPDIR)/libdogecoin_server_a-utxo_snapshot.Tpo -c -o node/libdogecoin_server_a-utxo_snapshot.o `test -f 'node/utxo_snapshot.cpp' || echo '$(srcdir)/'`node/utxo_snapshot.cpp
#$(AM_V_at)$(am__mv) node/$(DEPDIR)/libdogecoin_server_a-utxo_snapshot.Tpo node/$(DEPDIR)/libdogecoin_server_a-utxo_snapshot.Po
#$(AM_V_CXX)source='node/utxo_snapshot.cpp' object='node/libdogecoin_server_a-utxo_snapshot.o' libtool=no 
#DEPDIR=$(DEPDIR) $(CXXDEPMODE) $(depcomp) 
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -c -o node/libdogecoin_server_a-utxo_snapshot.o `test -f 'node/utxo_snapshot.cpp' || echo '$(srcdir)/'`node/utxo_snapshot.cpp

node/libdogecoin_server_a-utxo_snapshot.obj: node/utxo_snapshot.cpp
#$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -MT node/libdogecoin_server_a-utxo_snapshot.obj -MD -MP -MF node/$(DEPDIR)/libdogecoin_server_a-utxo_snapshot.Tpo -c -o node/libdogecoin_server_a-utxo_snapshot.obj `if test -f 'node/utxo_snapshot.cpp'; then $(CYGPATH_W) 'node/utxo_snapshot.cpp'; else $(CYGPATH_W) '$(srcdir)/node/utxo_snapshot.cpp'; fi`
#$(AM_V_at)$(am__mv) node/$(DEPDIR)/libdogecoin_server_a-utxo_snapshot.Tpo node/$(DEPDIR)/libdogecoin_server_a-utxo_snapshot.Po
#$(AM_V_CXX)source='node/utxo_snapshot.cpp' object='node/libdogecoin_server_a-utxo_snapshot.obj' libtool=no 
#DEPDIR=$(DEPDIR) $(CXXDEPMODE) $(depcomp) 
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -c -o node/libdogecoin_server_a-utxo_snapshot.obj `if test -f 'node/utxo_snapshot.cpp'; then $(CYGPATH_W) 'node/utxo_snapshot.cpp'; else $(CYGPATH_W) '$(srcdir)/node/utxo_snapshot.cpp'; fi`
'''

# Remove from first broken utxo_snapshot.o through end of .obj rule
pat = re.compile(
    r"node/libdogecoin_server_a-utxo_snapshot\.o:.*?"
    r"node/libdogecoin_server_a-utxo_snapshot\.obj:.*?"
    r"\$\(AM_V_CXX\)\$\(CXX\).*?libdogecoin_server_a-utxo_snapshot\.obj.*?\n",
    re.S,
)
new_text, n = pat.subn(clean_utxo + "\n", text, count=1)
if n != 1:
    # try simpler: if already clean, leave
    if "node/libdogecoin_server_a-utxo_snapshot.o: node/utxo_snapshot.cpp\n" in text:
        new_text = text
        print("utxo rules already clean or different shape")
    else:
        raise SystemExit("Could not match broken utxo_snapshot rules (count=%d)" % n)

snap_rules = r'''
node/libdogecoin_server_a-snapshot_fetch.o: node/snapshot_fetch.cpp
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -c -o node/libdogecoin_server_a-snapshot_fetch.o `test -f 'node/snapshot_fetch.cpp' || echo '$(srcdir)/'`node/snapshot_fetch.cpp

node/libdogecoin_server_a-snapshot_fetch.obj: node/snapshot_fetch.cpp
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -c -o node/libdogecoin_server_a-snapshot_fetch.obj `if test -f 'node/snapshot_fetch.cpp'; then $(CYGPATH_W) 'node/snapshot_fetch.cpp'; else $(CYGPATH_W) '$(srcdir)/node/snapshot_fetch.cpp'; fi`
'''

if "libdogecoin_server_a-snapshot_fetch.o:" not in new_text:
    new_text = new_text.replace(
        "node/libdogecoin_server_a-utxo_snapshot.o: node/utxo_snapshot.cpp",
        snap_rules + "\nnode/libdogecoin_server_a-utxo_snapshot.o: node/utxo_snapshot.cpp",
        1,
    )

# Ensure object is in am_libdogecoin_server_a_OBJECTS list
if "libdogecoin_server_a-snapshot_fetch.$(OBJEXT)" not in new_text:
    new_text = new_text.replace(
        "node/libdogecoin_server_a-utxo_snapshot.$(OBJEXT)",
        "node/libdogecoin_server_a-snapshot_fetch.$(OBJEXT) \\\n\tnode/libdogecoin_server_a-utxo_snapshot.$(OBJEXT)",
        1,
    )

# SOURCES list
if "node/snapshot_fetch.cpp" not in new_text.split("libdogecoin_server_a_SOURCES")[1][:2000] if "libdogecoin_server_a_SOURCES" in new_text else True:
    pass  # may already be there from earlier sed

if "node/snapshot_fetch.cpp \\" not in new_text and "node/snapshot_fetch.cpp\n" not in new_text:
    new_text = new_text.replace(
        "node/utxo_snapshot.cpp",
        "node/snapshot_fetch.cpp \\\n  node/utxo_snapshot.cpp",
        1,
    )

p.write_text(new_text)
print("Makefile patched OK")
print("snapshot_fetch in objects:", "libdogecoin_server_a-snapshot_fetch" in new_text)
