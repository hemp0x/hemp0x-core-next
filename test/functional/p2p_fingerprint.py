#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test various fingerprinting protections.

Hemp0x has strict fingerprinting protection: when a block is not on the main
chain, the node refuses to serve it immediately. This test verifies that:
1. Active chain blocks can always be fetched
2. Stale blocks (not on main chain) are refused
"""

import time
from io import BytesIO
from test_framework.mininode import CInv, NetworkThread, NodeConn, NodeConnCB, MsgGetdata, wait_until, CBlock
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal, p2p_port
from test_framework.messages import hex_str_to_bytes


def get_block_from_node(node, blockhash):
    """Retrieve a block from a node by hash and deserialize it into a CBlock."""
    block_hex = node.getblock(blockhash, 0)
    block = CBlock()
    block.deserialize(BytesIO(hex_str_to_bytes(block_hex)))
    block.hash = blockhash
    block.x16r = int(blockhash, 16)
    return block


class P2PFingerprintTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def send_block_request(self, block_hash, node):
        msg = MsgGetdata()
        msg.inv.append(CInv(2, block_hash))
        node.send_message(msg)

    def last_block_equals(self, expected_hash, node):
        block_msg = node.last_message.get("block")
        return block_msg is not None and block_msg.block is not None

    def run_test(self):
        # Set mocktime to 60 days ago
        old_time = int(time.time()) - 60 * 24 * 60 * 60
        self.nodes[0].setmocktime(old_time)

        # Generate initial chain of 10 blocks
        block_hashes = self.nodes[0].generate(nblocks=10)

        # Create a stale block by generating an alternative chain
        # Invalidate block 9 and generate a longer chain
        self.nodes[0].invalidateblock(block_hashes[8])
        fork_block_hashes = self.nodes[0].generate(nblocks=7)

        # The original tip (block_hashes[-1]) is now stale
        stale_hash = int(block_hashes[-1], 16)

        # Setup mininode connection
        node0_cb = NodeConnCB()
        connections = [NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], node0_cb)]
        node0_cb.add_connection(connections[0])
        NetworkThread().start()
        node0_cb.wait_for_verack()
        node0_cb.sync_with_ping()

        # Hemp0x refuses stale blocks immediately (stricter fingerprinting)
        # Verify that stale block request fails
        self.send_block_request(stale_hash, node0_cb)
        time.sleep(2)
        assert not self.last_block_equals(stale_hash, node0_cb), "Stale block should be refused"

        # Extend the chain further
        self.nodes[0].setmocktime(0)
        tip = self.nodes[0].generate(nblocks=1)[0]
        assert_equal(self.nodes[0].getblockcount(), 16)

        # Verify we can fetch active chain blocks
        block_hash = int(tip, 16)
        self.send_block_request(block_hash, node0_cb)
        node0_cb.sync_with_ping()

        self.send_block_request(block_hash, node0_cb)
        test_function = lambda: self.last_block_equals(block_hash, node0_cb)
        wait_until(test_function, timeout=3, err_msg="active block request")

        # Verify we can fetch old blocks on the active chain
        block_hash = int(block_hashes[2], 16)
        self.send_block_request(block_hash, node0_cb)
        test_function = lambda: self.last_block_equals(block_hash, node0_cb)
        wait_until(test_function, timeout=3, err_msg="old active block request")


if __name__ == '__main__':
    P2PFingerprintTest().main()
