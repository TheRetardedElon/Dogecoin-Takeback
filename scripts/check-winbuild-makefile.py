#!/usr/bin/env python3
"""Inspect winbuild Makefile for snapshot_fetch / utxo_snapshot corruption."""
from pathlib import Path

p = Path("/home/theretardedelon/dogedev-winbuild/src/Makefile")
t = p.read_text(encoding="utf-8", errors="replace")
print("snapshot_fetch object refs:", t.count("libdogecoin_server_a-snapshot_fetch"))
print("winhttp in LIBS:", "-lwinhttp" in t)
if "node/snapshot_fetch.cpp \\\nnode/utxo_snapshot.cpp" in t:
    print("CORRUPTION: multi-file jam in SOURCES-like recipe")
# show rules mentioning both
lines = t.splitlines()
for i, line in enumerate(lines, 1):
    if "utxo_snapshot.o:" in line and "snapshot_fetch" in line:
        print(f"BAD rule line {i}: {line[:140]}")
    if 6368 <= i <= 6385:
        print(f"{i}:{line[:140]}")
