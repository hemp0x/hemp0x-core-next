#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test purgesnapshot RPC argument handling and basic behavior."""

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal


class PurgeSnapshotTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-assetindex"]]

    def activate_assets(self):
        self.log.info("Activating assets...")
        n0 = self.nodes[0]
        n0.generate(1)
        n0.generate(431)
        assert_equal("active", n0.getblockchaininfo()["bip9_softforks"]["assets"]["status"])

    def run_test(self):
        n0 = self.nodes[0]

        self.activate_assets()

        self.log.info("Funding wallet and issuing PURGE1 asset")
        addr0 = n0.getnewaddress()
        n0.sendtoaddress(addr0, 500)
        n0.generate(10)
        n0.issue(asset_name="PURGE1", qty=1000, to_address=addr0, change_address="",
                 units=4, reissuable=True, has_ipfs=False)
        n0.generate(10)

        self.log.info("Determining snapshot target height")
        current_height = n0.getblockchaininfo()["blocks"]
        snap_height = current_height + 10

        self.log.info("Requesting snapshot of PURGE1 at height %d" % snap_height)
        n0.requestsnapshot(asset_name="PURGE1", block_height=snap_height)
        n0.generate(20)

        self.log.info("Verifying snapshot exists via getsnapshot")
        snapshot = n0.getsnapshot(asset_name="PURGE1", block_height=snap_height)
        assert_equal(snapshot["name"], "PURGE1")
        assert_equal(snapshot["height"], snap_height)
        assert isinstance(snapshot["owners"], list)
        assert len(snapshot["owners"]) > 0

        self.log.info("Calling purgesnapshot with correct args")
        result = n0.purgesnapshot(asset_name="PURGE1", block_height=snap_height)
        assert result is not None
        assert_equal(result["name"], "PURGE1")
        assert_equal(result["height"], snap_height)

        self.log.info("Verifying snapshot is gone after purge")
        result_after = n0.getsnapshot(asset_name="PURGE1", block_height=snap_height)
        assert result_after is None

        self.log.info("Calling purgesnapshot on already-purged snapshot (idempotent)")
        result_again = n0.purgesnapshot(asset_name="PURGE1", block_height=snap_height)
        assert result_again is not None
        assert_equal(result_again["name"], "PURGE1")
        assert_equal(result_again["height"], snap_height)

        self.log.info("Verifying purgesnapshot with non-existent asset does not crash")
        result_bad = n0.purgesnapshot(asset_name="NONEXISTENT", block_height=1)
        assert result_bad is not None
        assert_equal(result_bad["name"], "NONEXISTENT")
        assert_equal(result_bad["height"], 1)

        self.log.info("All purgesnapshot tests passed")


if __name__ == "__main__":
    PurgeSnapshotTest().main()
