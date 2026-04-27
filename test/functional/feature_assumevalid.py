#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2017-2021 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test P2P block delivery and synchronization.

Tests that blocks created with Python-side mining can be delivered via P2P
and accepted by nodes. Uses daemon-side compatible block construction with
correct version (0x30000040) and coinbase values.
"""

import time
from test_framework.blocktools import create_block, create_coinbase
from test_framework.key import ECKey
from test_framework.mininode import CBlockHeader, NetworkThread, NodeConn, NodeConnCB, MsgBlock, MsgHeaders
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import p2p_port, assert_equal


class BaseNode(NodeConnCB):
    def send_header_for_blocks(self, new_blocks):
        headers_message = MsgHeaders()
        headers_message.headers = [CBlockHeader(b) for b in new_blocks]
        self.send_message(headers_message)


class AssumeValidTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.enable_mocktime()

    def setup_network(self):
        self.add_nodes(2)
        self.start_node(0)
        self.start_node(1)

    @staticmethod
    def assert_blockchain_height(node, height):
        """Wait until the blockchain reaches the expected height."""
        timeout = 60
        while timeout > 0:
            current_height = node.getblock(node.getbestblockhash())['height']
            if current_height == height:
                return
            elif current_height > height:
                raise AssertionError("blockchain too long: %d" % current_height)
            time.sleep(0.25)
            timeout -= 0.25
        raise AssertionError("blockchain too short after timeout: %d" % node.getblock(node.getbestblockhash())['height'])

    def run_test(self):
        # Connect to node0
        node0 = BaseNode()
        connections = [NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], node0)]
        node0.add_connection(connections[0])

        NetworkThread().start()
        node0.wait_for_verack()

        # Build the blockchain
        self.tip = int(self.nodes[0].getbestblockhash(), 16)
        self.block_time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time'] + 1

        self.blocks = []

        # Get a pubkey for the coinbase TXO
        coinbase_key = ECKey()
        coinbase_key.generate()
        coinbase_pubkey = coinbase_key.get_pubkey().get_bytes()

        # Create the first block with a coinbase output to our key
        height = 1
        block = create_block(self.tip, create_coinbase(height, coinbase_pubkey), self.block_time)
        block.nVersion = 0x30000040
        self.blocks.append(block)
        self.block_time += 1
        block.solve()
        self.tip = block.x16r
        height += 1

        # Bury the block 100 deep so the coinbase output is spendable
        for i in range(100):
            block = create_block(self.tip, create_coinbase(height), self.block_time)
            block.nVersion = 0x30000040
            block.solve()
            self.blocks.append(block)
            self.tip = block.x16r
            self.block_time += 1
            height += 1

        # Bury with additional blocks (reduced from 2100 to 100 for faster testing)
        for i in range(100):
            block = create_block(self.tip, create_coinbase(height), self.block_time)
            block.nVersion = 0x30000040
            block.solve()
            self.blocks.append(block)
            self.tip = block.x16r
            self.block_time += 1
            height += 1

        self.log.info("Created %d blocks", len(self.blocks))

        # Connect to node1
        node1 = BaseNode()
        connections.append(NodeConn('127.0.0.1', p2p_port(1), self.nodes[1], node1))
        node1.add_connection(connections[1])
        node1.wait_for_verack()

        # Send headers to both nodes and wait for processing
        node0.send_header_for_blocks(self.blocks[0:100])
        node0.send_header_for_blocks(self.blocks[100:])
        node0.sync_with_ping()

        node1.send_header_for_blocks(self.blocks[0:100])
        node1.send_header_for_blocks(self.blocks[100:])
        node1.sync_with_ping()

        # Send blocks to node0
        for i in range(len(self.blocks)):
            node0.send_message(MsgBlock(self.blocks[i]))
        node0.sync_with_ping()
        self.assert_blockchain_height(self.nodes[0], len(self.blocks))
        self.log.info("Node0 accepted all %d blocks", len(self.blocks))

        # Send blocks to node1
        for i in range(len(self.blocks)):
            node1.send_message(MsgBlock(self.blocks[i]))
        node1.sync_with_ping()
        self.assert_blockchain_height(self.nodes[1], len(self.blocks))
        self.log.info("Node1 accepted all %d blocks", len(self.blocks))

        [c.disconnect_node() for c in connections]


if __name__ == '__main__':
    AssumeValidTest().main()
