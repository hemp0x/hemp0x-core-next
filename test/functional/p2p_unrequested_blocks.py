#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test processing of unrequested blocks.

Tests that nodes can receive and process blocks sent via P2P that were not
explicitly requested via getdata. Uses daemon-side block generation to ensure
blocks have valid KAWPOW hashes.
"""

import time
from io import BytesIO
from test_framework.mininode import NodeConn, NodeConnCB, NetworkThread, MsgBlock, MsgHeaders, CBlock, CBlockHeader, mininode_lock
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import os, p2p_port, assert_equal, connect_nodes, sync_blocks, disconnect_nodes
from test_framework.messages import hex_str_to_bytes


def get_block_from_node(node, blockhash):
    """Retrieve a block from a node by hash and deserialize it into a CBlock."""
    block_hex = node.getblock(blockhash, 0)
    block = CBlock()
    block.deserialize(BytesIO(hex_str_to_bytes(block_hex)))
    block.hash = blockhash
    block.calc_x16r()
    return block


class AcceptBlockTest(Hemp0xTestFramework):
    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("HEMP0XD", "hemp0xd"),
                          help="hemp0xd binary to test")

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], ["-whitelist=127.0.0.1"]]

    def setup_network(self):
        self.setup_nodes()
        # Connect nodes to establish shared chain, then disconnect
        connect_nodes(self.nodes[0], 1)
        sync_blocks([self.nodes[0], self.nodes[1]])

    def run_test(self):
        # Generate initial blocks and sync
        self.nodes[0].generate(1)
        sync_blocks([self.nodes[0], self.nodes[1]])
        assert_equal(self.nodes[0].getblockcount(), 1)
        assert_equal(self.nodes[1].getblockcount(), 1)

        # Disconnect node-to-node connection
        disconnect_nodes(self.nodes[0], 1)
        disconnect_nodes(self.nodes[1], 0)

        # Setup mininode connections
        test_node = NodeConnCB()
        white_node = NodeConnCB()

        connections = [NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test_node),
                       NodeConn('127.0.0.1', p2p_port(1), self.nodes[1], white_node)]
        test_node.add_connection(connections[0])
        white_node.add_connection(connections[1])

        NetworkThread().start()

        test_node.wait_for_verack()
        white_node.wait_for_verack()

        # Generate blocks on node0 and send them to node1 via P2P
        blocks = []
        for i in range(5):
            block_hashes = self.nodes[0].generate(1)
            blocks.append(get_block_from_node(self.nodes[0], block_hashes[0]))

        for block in blocks:
            white_node.send_message(MsgBlock(block))
        white_node.sync_with_ping()

        # Node1 should have accepted the unrequested blocks
        assert_equal(self.nodes[1].getblockcount(), 6)
        self.log.info("5 unrequested blocks accepted from whitelisted peer")

        # Generate blocks on node1 and send to node0 via non-whitelisted connection
        blocks2 = []
        for i in range(3):
            block_hashes = self.nodes[1].generate(1)
            blocks2.append(get_block_from_node(self.nodes[1], block_hashes[0]))

        for block in blocks2:
            test_node.send_message(MsgBlock(block))
        test_node.sync_with_ping()

        # Node0 should also accept unrequested blocks
        assert_equal(self.nodes[0].getblockcount(), 9)
        self.log.info("3 unrequested blocks accepted from non-whitelisted peer")

        # Verify nodes can sync via RPC after P2P block delivery
        connect_nodes(self.nodes[0], 1)
        sync_blocks([self.nodes[0], self.nodes[1]])
        assert_equal(self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        self.log.info("Nodes synced successfully after P2P block delivery")

        [c.disconnect_node() for c in connections]


if __name__ == '__main__':
    AcceptBlockTest().main()
