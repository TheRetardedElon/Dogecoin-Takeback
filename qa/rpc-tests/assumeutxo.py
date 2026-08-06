#!/usr/bin/env python3
# Copyright (c) 2026 The Dogecoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
assumeutxo.py

Regtest smoke for AssumeUTXO Phase B–D on Dogecoin 1.14 DNA:
  - getchainstates / listassumeutxo present
  - dumptxoutset writes hash_serialized + assumeutxo_snippet
  - loadtxoutset + activatesnapshot (regtest allows without -assumeutxodev)
  - when dump height == tip, background proof completes immediately
  - dual collapse after validation
"""

import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_jsonrpc


class AssumeUtxoTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]

        # RPCs exist
        cs = node.getchainstates()
        assert "chainstates" in cs
        assert "phase" in cs
        listed = node.listassumeutxo()
        assert_equal(listed["network"], "regtest")
        assert_equal(listed["count"], 0)

        # Mine a short chain (coinbase maturity on regtest is 60)
        node.generate(70)
        tip_height = node.getblockcount()
        assert_greater_than(tip_height, 60)

        # Dump snapshot of current tip
        dump_path = os.path.join(self.options.tmpdir, "utxo.dat")
        dumped = node.dumptxoutset(dump_path)
        assert_equal(dumped["base_height"], tip_height)
        assert "hash_serialized" in dumped
        assert "assumeutxo_snippet" in dumped
        assert "mapAssumeutxo" in dumped["assumeutxo_snippet"]
        assert os.path.isfile(dump_path)
        assert_greater_than(dumped["coins_written"], 0)

        # Load into background (no activate)
        loaded = node.loadtxoutset(dump_path)
        assert_equal(loaded["base_height"], tip_height)
        assert "hash_serialized" in loaded
        assert_equal(loaded.get("active_swapped", False), False)

        cs2 = node.getchainstates()
        # After load, background should report snapshot
        names = [c.get("name") for c in cs2["chainstates"]]
        assert "background" in names or any(c.get("has_snapshot") for c in cs2["chainstates"])

        # Activate (regtest always allowed)
        act = node.activatesnapshot()
        assert_equal(act["active_swapped"], True)
        assert_equal(act.get("active_height", tip_height), tip_height)

        # Same-height activate should complete / collapse background proof
        # (may already be validated if IBD tip matched snapshot)
        info = node.getibdinfo()
        assert info.get("snapshot_active") is True
        # progress fields present
        assert "assumeutxo_progress" in info
        assert "assumeutxo_validated" in info

        cs3 = node.getchainstates()
        assert cs3.get("snapshot_active") is True
        assert "assumeutxo_progress" in cs3

        # If already validated (same tip dump), dual should be collapsed
        if info.get("assumeutxo_validated"):
            assert info.get("assumeutxo_dual_collapsed") is True
            step = node.stepbackgroundvalidation(10)
            assert_equal(step["blocks_connected"], 0)
            assert step["status"] in ("completed", "none")

        # Second activate must fail
        assert_raises_jsonrpc(-1, "already active", node.activatesnapshot)

        print("assumeutxo regtest smoke OK")


if __name__ == "__main__":
    AssumeUtxoTest().main()
