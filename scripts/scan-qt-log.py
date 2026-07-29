#!/usr/bin/env python3
text = open("/tmp/qt-ssl-rebuild.log", "r", errors="replace").read().splitlines()
keys = ("OpenSSL", "openssl", " OPENSSL", "ssl ", "SSL ")
for i, line in enumerate(text, 1):
    low = line.lower()
    if "warning" in low or "qssl" in low or "compiling" in low:
        continue
    if any(k.lower() in low for k in ("openssl", "ssl support", "https")):
        print("%d:%s" % (i, line[:220]))
