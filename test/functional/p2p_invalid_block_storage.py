#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Regression coverage for invalid-block storage behavior.

Verifies that blocks failing cheap structural/contextual validation
(CheckBlock / ContextualCheckBlock) are rejected before disk write,
and that blocks passing those checks but failing ConnectBlock trigger
peer banning while the node continues to accept valid blocks.
"""

import os
import time

from test_framework.blocktools import create_block, create_coinbase
from test_framework.mininode import (
    COIN,
    MsgBlock,
    NetworkThread,
    NodeConn,
    NodeConnCB,
)
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    p2p_port,
    sync_blocks,
)


class InvalidBlockStorageTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], ["-whitelist=127.0.0.1"]]

    def setup_network(self):
        self.setup_nodes()
        connect_nodes(self.nodes[0], 1)
        sync_blocks([self.nodes[0], self.nodes[1]])

    def run_test(self):
        # Bootstrap: generate one block and sync
        self.nodes[0].generate(1)
        sync_blocks([self.nodes[0], self.nodes[1]])
        assert_equal(self.nodes[0].getblockcount(), 1)
        assert_equal(self.nodes[1].getblockcount(), 1)

        # Disconnect node-to-node so P2P tests are isolated
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 0)

        self.test_cheap_invalid_merkle()
        self.test_cheap_invalid_duplicate_coinbase()
        self.test_cheap_invalid_nonfinal_tx()
        self.test_valid_block_still_accepted()
        self.test_connection_invalid_overamount()

    # -----------------------------------------------------------------
    # Helpers
    # -----------------------------------------------------------------

    def make_valid_block(self, node, extra_txns=None):
        """Build and solve a valid block on top of node's tip via RPC."""
        tip_hash = node.getbestblockhash()
        tip = node.getblockheader(tip_hash)
        height = tip["height"] + 1
        tip_time = tip["time"]

        coinbase = create_coinbase(height)
        block = create_block(
            int(tip_hash, 16), coinbase, n_time=tip_time + 1
        )
        if extra_txns:
            for tx in extra_txns:
                block.vtx.append(tx)
            block.hashMerkleRoot = block.calc_merkle_root()
            block.calc_x16r()
        block.nVersion = 0x30000040
        block.solve()
        return block

    # -----------------------------------------------------------------
    # Cheap-invalid tests: blocks that fail CheckBlock or
    # ContextualCheckBlock. These must NOT be written to disk.
    # -----------------------------------------------------------------

    def test_cheap_invalid_merkle(self):
        self.log.info("Test: block with invalid merkle root is rejected and not stored")
        node = self.nodes[0]
        height_before = node.getblockcount()

        block = self.make_valid_block(node)
        # Corrupt the merkle root and re-solve so PoW is valid for the
        # corrupted header but the merkle root no longer matches vtx.
        block.hashMerkleRoot ^= 1
        block.calc_x16r()
        block.solve()

        result = node.submitblock(block.serialize().hex())
        assert_equal(result, "bad-txnmrklroot")
        assert_equal(node.getblockcount(), height_before)
        assert_raises_rpc_error(
            -5, "Block not found", node.getblock, block.hash
        )
        self.log.info("  rejected with: %s (not stored)", result)

    def test_cheap_invalid_duplicate_coinbase(self):
        self.log.info("Test: block with duplicate coinbase is rejected and not stored")
        node = self.nodes[0]
        height_before = node.getblockcount()

        block = self.make_valid_block(node)
        block.vtx.append(block.vtx[0])
        block.hashMerkleRoot = block.calc_merkle_root()
        block.calc_x16r()
        block.solve()

        result = node.submitblock(block.serialize().hex())
        assert_equal(result, "bad-txns-duplicate")
        assert_equal(node.getblockcount(), height_before)
        assert_raises_rpc_error(
            -5, "Block not found", node.getblock, block.hash
        )
        self.log.info("  rejected with: %s (not stored)", result)

    def test_cheap_invalid_nonfinal_tx(self):
        self.log.info("Test: block with non-final tx is rejected and not stored on disk")
        node = self.nodes[0]
        height_before = node.getblockcount()

        block = self.make_valid_block(node)
        block.vtx[0].nLockTime = 0xFFFFFFFF
        block.vtx[0].vin[0].nSequence = 0
        block.vtx[0].rehash()
        block.hashMerkleRoot = block.calc_merkle_root()
        block.calc_x16r()
        block.solve()

        result = node.submitblock(block.serialize().hex())
        assert_equal(result, "bad-txns-nonfinal")
        assert_equal(node.getblockcount(), height_before)

        # ContextualCheckBlock rejects after AcceptBlockHeader already
        # created a CBlockIndex entry, so the header is in the index
        # but the block body was never written to disk. getblock
        # returns -1 "not found on disk" (header present, no data)
        # rather than -5 "Block not found" (no index entry at all).
        assert_raises_rpc_error(
            -1, "not found on disk", node.getblock, block.hash
        )
        self.log.info("  rejected with: %s (header indexed but not on disk)", result)

    # -----------------------------------------------------------------
    # Positive control: a valid block is still accepted after rejections.
    # -----------------------------------------------------------------

    def test_valid_block_still_accepted(self):
        self.log.info("Test: valid block accepted after invalid-block rejections")
        node = self.nodes[0]
        height_before = node.getblockcount()

        block = self.make_valid_block(node)
        result = node.submitblock(block.serialize().hex())
        assert result is None, "Expected valid block to be accepted, got: %s" % result
        assert_equal(node.getblockcount(), height_before + 1)
        node.getblock(block.hash)
        self.log.info("  valid block accepted at height %d", height_before + 1)

    # -----------------------------------------------------------------
    # Connection-invalid test: block passes CheckBlock / ContextualCheckBlock
    # but fails ConnectBlock (coinbase pays more than subsidy + fees).
    # Expected behavior: block IS written to disk (validation-before-write
    # only covers cheap checks), and the sending peer is banned (DoS=100).
    # -----------------------------------------------------------------

    def test_connection_invalid_overamount(self):
        self.log.info("Test: connection-invalid block bans peer (coinbase overpayment)")

        # Use node0 which is NOT whitelisted — whitelisted peers are
        # exempt from ban/disconnect (net_processing.cpp:2975).
        node = self.nodes[0]

        # Set up a dedicated P2P connection for this test
        peer = NodeConnCB()
        conn = NodeConn("127.0.0.1", p2p_port(0), node, peer)
        peer.add_connection(conn)
        NetworkThread().start()
        peer.wait_for_verack()

        tip_hash = node.getbestblockhash()
        tip = node.getblockheader(tip_hash)
        height = tip["height"] + 1
        tip_time = tip["time"]

        # Build a block with coinbase paying 2x the allowed subsidy.
        # This passes CheckBlock and ContextualCheckBlock (structure,
        # timestamps, difficulty, finality all valid) but fails
        # ConnectBlock on the reward check.
        overvalue = 20 * COIN  # subsidy at low height is 10 COIN
        coinbase = create_coinbase(height, value=overvalue)
        bad_block = create_block(
            int(tip_hash, 16), coinbase, n_time=tip_time + 1
        )
        bad_block.nVersion = 0x30000040
        bad_block.solve()

        height_before = node.getblockcount()

        # Record the current log position before sending the block
        debug_log = os.path.join(
            node.datadir, "regtest", "debug.log"
        )
        with open(debug_log, encoding="utf-8") as dl:
            dl.seek(0, 2)
            log_start = dl.tell()

        peer.send_message(MsgBlock(bad_block))

        # Cannot use sync_with_ping here — the node disconnects the
        # peer after ban, so the pong never arrives.  Wait for the
        # ban log line instead.
        ban_timeout = time.time() + 30
        ban_detected = False
        while time.time() < ban_timeout:
            with open(debug_log, encoding="utf-8") as dl:
                dl.seek(log_start)
                log_content = dl.read()
            if "BAN THRESHOLD EXCEEDED" in log_content:
                ban_detected = True
                break
            time.sleep(0.1)

        assert ban_detected, "Expected BAN THRESHOLD EXCEEDED in debug log"
        assert "coinbase pays too much" in log_content, (
            "Expected 'coinbase pays too much' in debug log"
        )

        # Verify: block count unchanged
        assert_equal(node.getblockcount(), height_before)

        # This block passed the cheap checks in AcceptBlock(), so it is stored
        # before failing the full connection checks in ConnectBlock().
        stored_block = node.getblock(bad_block.hash)
        assert_equal(stored_block["hash"], bad_block.hash)
        self.log.info("  peer banned; block count unchanged at %d", height_before)

        # Verify the node still accepts valid blocks from another source
        node.generate(1)
        assert_equal(node.getblockcount(), height_before + 1)
        self.log.info("  node still accepts valid blocks after ban")

        conn.disconnect_node()


if __name__ == "__main__":
    InvalidBlockStorageTest().main()
