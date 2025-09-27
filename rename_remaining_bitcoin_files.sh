#!/bin/bash

echo "=== Renaming remaining bitcoin files to dogecoin equivalents ==="

# Change to the project directory
cd /mnt/d/dogecoin-master

echo "1. Renaming contrib files..."
if [ -f "contrib/rpm/bitcoin.fc" ]; then
    mv contrib/rpm/bitcoin.fc contrib/rpm/dogecoin.fc
fi
if [ -f "contrib/rpm/bitcoin.if" ]; then
    mv contrib/rpm/bitcoin.if contrib/rpm/dogecoin.if
fi
if [ -f "contrib/rpm/bitcoin.spec" ]; then
    mv contrib/rpm/bitcoin.spec contrib/rpm/dogecoin.spec
fi
if [ -f "contrib/rpm/bitcoin.te" ]; then
    mv contrib/rpm/bitcoin.te contrib/rpm/dogecoin.te
fi
if [ -f "contrib/rpm/bitcoin-0.12.0-libressl.patch" ]; then
    mv contrib/rpm/bitcoin-0.12.0-libressl.patch contrib/rpm/dogecoin-0.12.0-libressl.patch
fi

echo "2. Renaming build-aux files..."
if [ -f "build-aux/m4/bitcoin_qt.m4" ]; then
    mv build-aux/m4/bitcoin_qt.m4 build-aux/m4/dogecoin_qt.m4
fi
if [ -f "build-aux/m4/bitcoin_subdir_to_include.m4" ]; then
    mv build-aux/m4/bitcoin_subdir_to_include.m4 build-aux/m4/dogecoin_subdir_to_include.m4
fi
if [ -f "build-aux/m4/bitcoin_find_bdb48.m4" ]; then
    mv build-aux/m4/bitcoin_find_bdb48.m4 build-aux/m4/dogecoin_find_bdb48.m4
fi
if [ -f "build-aux/m4/bitcoin_secp.m4" ]; then
    mv build-aux/m4/bitcoin_secp.m4 build-aux/m4/dogecoin_secp.m4
fi

echo "3. Renaming test files..."
if [ -f "src/bitcoin-util-test.json" ]; then
    mv src/bitcoin-util-test.json src/dogecoin-util-test.json
fi
if [ -f "src/bitcoin-util-test.py" ]; then
    mv src/bitcoin-util-test.py src/dogecoin-util-test.py
fi

echo "4. Cleaning up backup files..."
if [ -f "src/config/bitcoin-config.h.in~" ]; then
    rm src/config/bitcoin-config.h.in~
fi

echo "5. Removing bitcoin folder if it exists..."
if [ -d "bitcoin" ]; then
    rm -rf bitcoin/
fi

echo "=== File renaming complete ==="
echo "Remaining bitcoin files:"
find . -name '*bitcoin*' -type f | grep -v '.git' | wc -l
