#!/bin/bash

echo "=== Fixing locale file references in Makefiles ==="

cd /mnt/d/dogecoin-master

echo "1. Fixing all bitcoin locale file references to dogecoin..."
find . -name 'Makefile*' -exec sed -i 's/bitcoin_\([a-z_]*\)\.ts/dogecoin_\1.ts/g' {} \;
find . -name 'Makefile*' -exec sed -i 's/bitcoin_\([a-z_]*\)\.qm/dogecoin_\1.qm/g' {} \;

echo "2. Fixing any remaining bitcoin locale patterns..."
find . -name 'Makefile*' -exec sed -i 's/qt\/locale\/bitcoin_/qt\/locale\/dogecoin_/g' {} \;

echo "=== Locale references fixed ==="
