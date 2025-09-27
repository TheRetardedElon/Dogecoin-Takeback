#!/bin/bash

echo "=== Cleaning build artifacts and regenerating build system ==="

cd /mnt/d/dogecoin-master

echo "1. Cleaning build artifacts..."
make clean

echo "2. Removing generated files..."
rm -rf src/.deps/
rm -rf src/leveldb/.deps/
rm -rf src/secp256k1/.deps/
rm -rf src/univalue/.deps/
rm -f config.log config.status
rm -f src/config.h
rm -f Makefile src/Makefile

echo "3. Regenerating build system..."
./autogen.sh

echo "4. Running configure..."
./configure

echo "=== Build system regeneration complete ==="
echo "Ready to build with: make"
