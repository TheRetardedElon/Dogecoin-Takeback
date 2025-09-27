#!/bin/bash

echo "=== Fixing test file includes ==="

cd /mnt/d/dogecoin-master

echo "1. Fixing test_bitcoin.h includes in all test files..."
find src/test -name '*.cpp' -exec sed -i 's/#include "test\/test_bitcoin.h"/#include "test\/test_dogecoin.h"/g' {} \;

echo "2. Fixing any remaining test_bitcoin.h references..."
find src/test -name '*.cpp' -exec sed -i 's/test_bitcoin\.h/test_dogecoin.h/g' {} \;

echo "3. Fixing bench_bitcoin.cpp references in Makefiles..."
find . -name 'Makefile*' -exec sed -i 's/bench_bitcoin\.cpp/bench_dogecoin.cpp/g' {} \;

echo "4. Fixing test_bitcoin references in Makefiles..."
find . -name 'Makefile*' -exec sed -i 's/test_bitcoin\.cpp/test_dogecoin.cpp/g' {} \;
find . -name 'Makefile*' -exec sed -i 's/test_bitcoin\.h/test_dogecoin.h/g' {} \;

echo "=== Test includes fixed ==="
