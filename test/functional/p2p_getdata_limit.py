#!/usr/bin/env python3
# Copyright (c) 2017-2026 The Hemp0x Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test that oversized getdata queues remain bounded and responsive.

The daemon caps both the accumulated getdata queue and per-call getdata
processing. This test sends a large getdata message and verifies the node
remains responsive instead of hanging or crashing.
"""

from test_framework.messages import CInv, MsgGetdata
from test_framework.mininode import NodeConn, NodeConnCB, NetworkThread
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import p2p_port, assert_equal

# CInv type constants
MSG_BLOCK = 2

class P2PGetDataLimitTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        self.log.info("Test oversized getdata queue processing limit")

        # Generate some blocks so the node has data
        self.nodes[0].generate(10)

        # Connect a mininode
        test_node = NodeConnCB()
        conn = NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], test_node)
        test_node.add_connection(conn)
        NetworkThread().start()
        test_node.wait_for_verack()

        # Build a getdata message with many items (more than the 1000 per-call cap)
        # Use dummy hashes - the node will process them and respond with notfound
        inv_items = []
        for i in range(5000):
            # Create a dummy 32-byte hash
            h = i.to_bytes(32, byteorder='little')
            inv_items.append(CInv(MSG_BLOCK, int.from_bytes(h, byteorder='little')))

        self.log.info("Sending getdata with %d items", len(inv_items))
        test_node.send_message(MsgGetdata(inv_items))
        test_node.sync_with_ping(timeout=30)

        # The node should still be responsive - check we can get peer info
        peer_info = self.nodes[0].getpeerinfo()
        assert_equal(len(peer_info), 1)

        # The node should have responded with notfound for some items
        # (at least the first 1000 that were processed in the first call)
        self.log.info("Node remained responsive after oversized getdata")
        self.log.info("Peer count: %d", len(peer_info))

if __name__ == '__main__':
    P2PGetDataLimitTest().main()
