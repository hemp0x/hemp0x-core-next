#!/usr/bin/env python3
# Copyright (c) 2017-2019 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test loadblock option

Test the option to start a node with the option loadblock which loads
a serialized blockchain from a file (usually called bootstrap.dat).
To generate that file this test uses the helper scripts available
in contrib/linearize.
"""

import os
import struct
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal, wait_until

class LoadblockTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def run_test(self):
        self.nodes[1].setnetworkactive(state=False)
        self.nodes[0].generate(100)

        bootstrap_file = os.path.join(self.options.tmpdir, "bootstrap.dat")

        self.log.info("Create bootstrap file from raw regtest blocks")
        regtest_magic = bytes.fromhex("48524547")
        with open(bootstrap_file, "wb") as bootstrap:
            for height in range(101):
                block_hash = self.nodes[0].getblockhash(height)
                raw_block = bytes.fromhex(self.nodes[0].getblock(block_hash, 0))
                bootstrap.write(regtest_magic)
                bootstrap.write(struct.pack("<I", len(raw_block)))
                bootstrap.write(raw_block)

        self.log.info("Restart second, unsynced node with bootstrap file")
        self.stop_node(1)
        self.start_node(1, ["-loadblock=" + bootstrap_file])
        wait_until(lambda: self.nodes[1].getblockcount() == 100, err_msg="Wait for block count == 100")

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 100)
        assert_equal(self.nodes[0].getbestblockhash(), self.nodes[1].getbestblockhash())


if __name__ == '__main__':
    LoadblockTest().main()
