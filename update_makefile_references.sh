#!/bin/bash

echo "=== Updating Makefile references from bitcoin to dogecoin ==="

cd /mnt/d/dogecoin-master

echo "1. Updating Makefile.am files..."

# Update src/Makefile.am
if [ -f "src/Makefile.am" ]; then
    sed -i 's/bitcoin-util-test\.json/dogecoin-util-test.json/g' src/Makefile.am
    sed -i 's/bitcoin-util-test\.py/dogecoin-util-test.py/g' src/Makefile.am
    sed -i 's/bench_bitcoin\.cpp/bench_dogecoin.cpp/g' src/Makefile.am
    sed -i 's/test_bitcoin\.cpp/test_dogecoin.cpp/g' src/Makefile.am
    sed -i 's/test_bitcoin\.h/test_dogecoin.h/g' src/Makefile.am
    sed -i 's/test_bitcoin_fuzzy\.cpp/test_dogecoin_fuzzy.cpp/g' src/Makefile.am
    sed -i 's/bitcoinconsensus\.cpp/dogecoinconsensus.cpp/g' src/Makefile.am
    sed -i 's/bitcoinconsensus\.h/dogecoinconsensus.h/g' src/Makefile.am
    sed -i 's/bitcoin-cli-res\.rc/dogecoin-cli-res.rc/g' src/Makefile.am
    sed -i 's/bitcoin-tx-res\.rc/dogecoin-tx-res.rc/g' src/Makefile.am
    sed -i 's/bitcoind-res\.rc/dogecoind-res.rc/g' src/Makefile.am
fi

# Update src/script/Makefile.am
if [ -f "src/script/Makefile.am" ]; then
    sed -i 's/bitcoinconsensus\.cpp/dogecoinconsensus.cpp/g' src/script/Makefile.am
    sed -i 's/bitcoinconsensus\.h/dogecoinconsensus.h/g' src/script/Makefile.am
fi

# Update src/bench/Makefile.am
if [ -f "src/bench/Makefile.am" ]; then
    sed -i 's/bench_bitcoin\.cpp/bench_dogecoin.cpp/g' src/bench/Makefile.am
fi

# Update src/test/Makefile.am
if [ -f "src/test/Makefile.am" ]; then
    sed -i 's/test_bitcoin\.cpp/test_dogecoin.cpp/g' src/test/Makefile.am
    sed -i 's/test_bitcoin\.h/test_dogecoin.h/g' src/test/Makefile.am
    sed -i 's/test_bitcoin_fuzzy\.cpp/test_dogecoin_fuzzy.cpp/g' src/test/Makefile.am
fi

echo "2. Updating configure.ac..."
if [ -f "configure.ac" ]; then
    sed -i 's/bitcoin_qt\.m4/dogecoin_qt.m4/g' configure.ac
    sed -i 's/bitcoin_subdir_to_include\.m4/dogecoin_subdir_to_include.m4/g' configure.ac
    sed -i 's/bitcoin_find_bdb48\.m4/dogecoin_find_bdb48.m4/g' configure.ac
    sed -i 's/bitcoin_secp\.m4/dogecoin_secp.m4/g' configure.ac
    sed -i 's/libbitcoinconsensus\.pc/libdogecoinconsensus.pc/g' configure.ac
    sed -i 's/bitcoin-qt\.pro/dogecoin-qt.pro/g' configure.ac
fi

echo "3. Updating Makefile.in files..."
find . -name "Makefile.in" -exec sed -i 's/bitcoin-util-test\.json/dogecoin-util-test.json/g' {} \;
find . -name "Makefile.in" -exec sed -i 's/bitcoin-util-test\.py/dogecoin-util-test.py/g' {} \;
find . -name "Makefile.in" -exec sed -i 's/bench_bitcoin\.cpp/bench_dogecoin.cpp/g' {} \;
find . -name "Makefile.in" -exec sed -i 's/test_bitcoin\.cpp/test_dogecoin.cpp/g' {} \;
find . -name "Makefile.in" -exec sed -i 's/test_bitcoin\.h/test_dogecoin.h/g' {} \;
find . -name "Makefile.in" -exec sed -i 's/test_bitcoin_fuzzy\.cpp/test_dogecoin_fuzzy.cpp/g' {} \;
find . -name "Makefile.in" -exec sed -i 's/bitcoinconsensus\.cpp/dogecoinconsensus.cpp/g' {} \;
find . -name "Makefile.in" -exec sed -i 's/bitcoinconsensus\.h/dogecoinconsensus.h/g' {} \;

echo "=== Makefile references updated ==="
