#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
cd /home/theretardedelon/dogedev-winbuild/src

# Force remake by deleting and using a private rule file
cat > /tmp/build-blockchain.mk <<'EOF'
include Makefile
.PHONY: force_blockchain
force_blockchain: rpc/blockchain.cpp
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libdogecoin_server_a_CPPFLAGS) $(CPPFLAGS) $(libdogecoin_server_a_CXXFLAGS) $(CXXFLAGS) -c -o rpc/libdogecoin_server_a-blockchain.o `test -f 'rpc/blockchain.cpp' || echo '$(srcdir)/'`rpc/blockchain.cpp
EOF

make -f /tmp/build-blockchain.mk force_blockchain 2>&1 | tail -60
ls -la rpc/libdogecoin_server_a-blockchain.o
echo OK
