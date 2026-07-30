#!/usr/bin/env python3
"""Inject peermapwidget into generated src/Makefile if automake hasn't re-run."""
from pathlib import Path
import re

path = Path("/home/theretardedelon/dogedev-winbuild/src/Makefile")
t = path.read_text()
if "peermapwidget.cpp" in t and "libdogecoinqt_a-peermapwidget" in t:
    print("already present")
    raise SystemExit(0)

# Insert cpp next to networkpage.cpp in source lists
t2 = t.replace(
    "qt/networkpage.cpp qt/openuridialog.cpp",
    "qt/networkpage.cpp qt/peermapwidget.cpp qt/openuridialog.cpp",
)
if t2 == t:
    t2 = t.replace(
        "qt/networkpage.cpp \\\n",
        "qt/networkpage.cpp \\\n  qt/peermapwidget.cpp \\\n",
    )
t = t2

# headers list
t = t.replace(
    "qt/networkpage.h \\\n",
    "qt/networkpage.h \\\n  qt/peermapwidget.h \\\n",
)
t = t.replace(
    "qt/memestreamrail.h qt/networkpage.h \\",
    "qt/memestreamrail.h qt/networkpage.h qt/peermapwidget.h \\",
)
t = t.replace(
    "qt/networkpage.h \n",
    "qt/networkpage.h qt/peermapwidget.h \n",
)

# moc list
t = t.replace(
    "qt/moc_networkpage.cpp",
    "qt/moc_networkpage.cpp qt/moc_peermapwidget.cpp",
)

# object lists near networkpage
for old, new in [
    (
        "qt/libdogecoinqt_a-networkpage.$(OBJEXT) \\",
        "qt/libdogecoinqt_a-networkpage.$(OBJEXT) \\\nqt/libdogecoinqt_a-peermapwidget.$(OBJEXT) \\",
    ),
    (
        "qt/libdogecoinqt_a-moc_networkpage.$(OBJEXT)",
        "qt/libdogecoinqt_a-moc_networkpage.$(OBJEXT) \\\nqt/libdogecoinqt_a-moc_peermapwidget.$(OBJEXT)",
    ),
]:
    if new.split("\n")[0] not in t or "peermapwidget.$(OBJEXT)" not in t:
        t = t.replace(old, new, 1)

# Build rules: clone networkpage rules for peermapwidget
rule_src = """
qt/libdogecoinqt_a-peermapwidget.o: qt/peermapwidget.cpp
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-peermapwidget.o `test -f 'qt/peermapwidget.cpp' || echo '$(srcdir)/'`qt/peermapwidget.cpp

qt/libdogecoinqt_a-peermapwidget.obj: qt/peermapwidget.cpp
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-peermapwidget.obj `if test -f 'qt/peermapwidget.cpp'; then $(CYGPATH_W) 'qt/peermapwidget.cpp'; else $(CYGPATH_W) '$(srcdir)/qt/peermapwidget.cpp'; fi`
"""

moc_rule = """
qt/moc_peermapwidget.cpp: $(srcdir)/qt/peermapwidget.h
	@test -f moc || echo "need moc"
	$(AM_V_MOC)$(MOC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) -o qt/moc_peermapwidget.cpp `test -f 'qt/peermapwidget.h' || echo '$(srcdir)/'`qt/peermapwidget.h

qt/libdogecoinqt_a-moc_peermapwidget.o: qt/moc_peermapwidget.cpp
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-moc_peermapwidget.o `test -f 'qt/moc_peermapwidget.cpp' || echo '$(srcdir)/'`qt/moc_peermapwidget.cpp

qt/libdogecoinqt_a-moc_peermapwidget.obj: qt/moc_peermapwidget.cpp
$(AM_V_CXX)$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(qt_libdogecoinqt_a_CPPFLAGS) $(CPPFLAGS) $(qt_libdogecoinqt_a_CXXFLAGS) $(CXXFLAGS) -c -o qt/libdogecoinqt_a-moc_peermapwidget.obj `if test -f 'qt/moc_peermapwidget.cpp'; then $(CYGPATH_W) 'qt/moc_peermapwidget.cpp'; else $(CYGPATH_W) '$(srcdir)/qt/moc_peermapwidget.cpp'; fi`
"""

if "libdogecoinqt_a-peermapwidget.o:" not in t:
    # Append after networkpage rules
    anchor = "qt/libdogecoinqt_a-networkpage.obj: qt/networkpage.cpp"
    idx = t.find(anchor)
    if idx >= 0:
        # find end of networkpage.obj rule block (next blank line after a few lines)
        end = t.find("\n\n", idx)
        if end < 0:
            end = idx + 500
        t = t[:end] + "\n" + rule_src + t[end:]

if "moc_peermapwidget.cpp:" not in t:
    # Find moc_networkpage rule
    anchor = "qt/moc_networkpage.cpp:"
    idx = t.find(anchor)
    if idx >= 0:
        end = t.find("\n\n", idx)
        if end < 0:
            end = idx + 400
        t = t[:end] + "\n" + moc_rule + t[end:]

# Ensure object is in libdogecoinqt_a_OBJECTS
if "libdogecoinqt_a-peermapwidget.$(OBJEXT)" not in t:
    t = t.replace(
        "qt/libdogecoinqt_a-networkpage.$(OBJEXT)",
        "qt/libdogecoinqt_a-networkpage.$(OBJEXT) qt/libdogecoinqt_a-peermapwidget.$(OBJEXT)",
        1,
    )
if "libdogecoinqt_a-moc_peermapwidget.$(OBJEXT)" not in t:
    t = t.replace(
        "qt/libdogecoinqt_a-moc_networkpage.$(OBJEXT)",
        "qt/libdogecoinqt_a-moc_networkpage.$(OBJEXT) qt/libdogecoinqt_a-moc_peermapwidget.$(OBJEXT)",
        1,
    )

path.write_text(t)
print("patched", path)
print("peermapwidget.cpp count", t.count("peermapwidget.cpp"))
print("peermapwidget.o count", t.count("peermapwidget"))
