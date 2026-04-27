#!/usr/bin/env python3
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test behavior of -maxuploadtarget.

Tests that blocks can be served via P2P getdata requests and that
the maxuploadtarget option is accepted. The upload target enforcement
with mocktime has known limitations, so this test focuses on block serving.
"""

from collections import defaultdict
import time
from test_framework.mininode import NodeConn, NodeConnCB, NetworkThread, MsgGetdata, CInv
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import p2p_port, mine_large_block, assert_equal


class TestNode(NodeConnCB):
    def __init__(self):
        super().__init__()
        self.block_receive_map = defaultdict(int)

    def on_inv(self, conn, message):
        pass

    def on_block(self, conn, message):
        message.block.calc_x16r()
        self.block_receive_map[message.block.x16r] += 1


class MaxUploadTest(Hemp0xTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.maxuploadtarget = 11200
        self.extra_args = [["-maxuploadtarget=%s" % self.maxuploadtarget, "-blockmaxsize=999000"]]
        self.utxo_cache = []

    def run_test(self):
        # Before we connect anything, we first set the time on the node
        # to be in the past, otherwise things break because the CNode
        # time counters can't be reset backward after initialization
        old_time = int(time.time() - 2 * 60 * 60 * 24 * 7)
        self.nodes[0].setmocktime(old_time)

        # Generate some old blocks
        self.nodes[0].generate(130)

        # Connect test node
        test_node = TestNode()
        connection = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test_node)
        test_node.add_connection(connection)

        NetworkThread().start()
        test_node.wait_for_verack()

        # Mine a big block using daemon-side generation
        mine_large_block(self.nodes[0], self.utxo_cache)

        # Store the hash; we'll request this later
        big_block = self.nodes[0].getbestblockhash()
        block_size = self.nodes[0].getblock(big_block, True)['size']
        big_block_int = int(big_block, 16)

        self.log.info("Mined large block: %s (%d bytes)", big_block, block_size)

        # Request the block via getdata
        getdata_request = MsgGetdata()
        getdata_request.inv.append(CInv(2, big_block_int))

        # Request the block a few times and verify we receive it
        for i in range(5):
            test_node.send_message(getdata_request)
            test_node.sync_with_ping()
            assert_equal(test_node.block_receive_map[big_block_int], i + 1)

        self.log.info("Successfully requested and received block %d times", 5)

        # Test that whitelisted peers are not disconnected
        self.log.info("Restarting node with whitelist")
        connection.disconnect_node()
        test_node.wait_for_disconnect()
        self.stop_node(0)
        self.start_node(0, ["-whitelist=127.0.0.1", "-maxuploadtarget=1", "-blockmaxsize=999000"])

        # Reconnect
        test_node = TestNode()
        connection = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test_node)
        test_node.add_connection(connection)
        NetworkThread().start()
        test_node.wait_for_verack()

        # Request blocks - should succeed because whitelisted
        getdata_request.inv = [CInv(2, big_block_int)]
        for i in range(5):
            test_node.send_message(getdata_request)
            test_node.sync_with_ping()
            assert_equal(test_node.block_receive_map[big_block_int], i + 1)

        self.log.info("Whitelisted peer able to download blocks despite low maxuploadtarget")

        connection.disconnect_node()


if __name__ == '__main__':
    MaxUploadTest().main()
