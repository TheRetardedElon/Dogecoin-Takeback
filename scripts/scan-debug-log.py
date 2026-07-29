#!/usr/bin/env python3
from pathlib import Path
p = Path("/mnt/c/Users/Jeramiah/AppData/Roaming/Dogecoin/debug.log")
lines = p.read_text(errors="replace").splitlines()
keys = ("keypool", "error", "ERROR", "exception", "assert", "AddressBook", "crash", "Fatal", "runtime")
print("=== matching lines (last 12000) ===")
start = max(0, len(lines) - 12000)
for i, l in enumerate(lines[start:], start=start + 1):
    if "UpdateTip" in l:
        continue
    low = l.lower()
    if any(k.lower() in low for k in keys):
        print("%d:%s" % (i, l[:240]))
print("=== last non-UpdateTip lines ===")
count = 0
for l in reversed(lines):
    if "UpdateTip" in l:
        continue
    print(l[:240])
    count += 1
    if count >= 50:
        break
