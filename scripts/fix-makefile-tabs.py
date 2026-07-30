#!/usr/bin/env python3
from pathlib import Path
p = Path("/home/theretardedelon/dogedev-winbuild/src/Makefile")
lines = p.read_text().splitlines(True)
out = []
i = 0
targets = {
    "qt/libdogecoinqt_a-peermapwidget.o:",
    "qt/libdogecoinqt_a-peermapwidget.obj:",
    "qt/moc_peermapwidget.cpp:",
    "qt/libdogecoinqt_a-moc_peermapwidget.o:",
    "qt/libdogecoinqt_a-moc_peermapwidget.obj:",
}
while i < len(lines):
    line = lines[i]
    out.append(line)
    stripped = line.rstrip("\n")
    if stripped in targets or any(stripped.startswith(t) for t in targets):
        i += 1
        while i < len(lines):
            l = lines[i]
            if l.strip() == "":
                out.append(l)
                i += 1
                break
            # next target
            if l[0] not in " \t#" and l.rstrip().endswith(":") and not l.startswith("\t"):
                break
            if l.startswith("\t") or l.startswith("#"):
                out.append(l)
            else:
                # recipe missing tab
                out.append("\t" + l.lstrip())
            i += 1
        continue
    i += 1
p.write_text("".join(out))
print("fixed tabs")
# verify make can parse
import subprocess
r = subprocess.run(["make", "-n", "qt/libdogecoinqt_a-networkpage.o"], cwd=str(p.parent), capture_output=True, text=True)
print("make -n exit", r.returncode)
print((r.stdout + r.stderr)[-500:])
