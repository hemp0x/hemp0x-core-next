#!/usr/bin/env python3
# Copyright (c) 2020 The Hemp0x Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test wallet RPC argument validation for sendtoaddress, sendmany,
sendfromaddress, and walletpassphrase."""

import time
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


class WalletRpcArgsTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def run_test(self):
        self.nodes[0].generate(101)
        self.sync_all()

        self.test_sendtoaddress_args()
        self.test_sendmany_args()
        self.test_sendfromaddress_args()
        self.test_walletpassphrase_args()

    def test_sendtoaddress_args(self):
        self.log.info("Testing sendtoaddress argument parsing...")
        address = self.nodes[1].getnewaddress()

        self.nodes[0].sendtoaddress(address, 0.5)
        self.nodes[0].sendtoaddress(address, 0.5, "comment")
        self.nodes[0].sendtoaddress(address, 0.5, "comment", "comment_to")
        self.nodes[0].sendtoaddress(address, 0.5, "", "", False)
        self.nodes[0].sendtoaddress(address, 0.5, "", "", False, 6)
        self.nodes[0].sendtoaddress(address, 0.5, "", "", False, 6, "ECONOMICAL")
        self.nodes[0].sendtoaddress(address, 0.5, "", "", False, 6, "CONSERVATIVE")

        assert_raises_rpc_error(-8, "Invalid estimate_mode parameter",
                                self.nodes[0].sendtoaddress, address, 0.5, "", "", False, 6, "INVALID")

        self.nodes[0].generate(1)
        self.sync_all()
        self.log.info("sendtoaddress argument parsing OK")

    def test_sendmany_args(self):
        self.log.info("Testing sendmany argument parsing...")
        address1 = self.nodes[1].getnewaddress()
        address2 = self.nodes[1].getnewaddress()

        self.nodes[0].sendmany("", {address1: 0.1, address2: 0.2})
        self.nodes[0].sendmany("", {address1: 0.1}, 1)
        self.nodes[0].sendmany("", {address1: 0.1}, 1, "comment")
        self.nodes[0].sendmany("", {address1: 0.1}, 1, "", [address1])
        self.nodes[0].sendmany("", {address1: 0.1}, 1, "", [], 6)
        self.nodes[0].sendmany("", {address1: 0.1}, 1, "", [], 6, "ECONOMICAL")

        assert_raises_rpc_error(-8, "Invalid estimate_mode parameter",
                                self.nodes[0].sendmany, "", {address1: 0.1}, 1, "", [], 6, "INVALID")

        self.nodes[0].generate(1)
        self.sync_all()
        self.log.info("sendmany argument parsing OK")

    def test_sendfromaddress_args(self):
        self.log.info("Testing sendfromaddress argument parsing...")
        from_address = self.nodes[0].getnewaddress()
        to_address = self.nodes[1].getnewaddress()

        self.nodes[0].sendtoaddress(from_address, 5.0)
        self.nodes[0].generate(1)

        self.nodes[0].sendfromaddress(from_address, to_address, 0.5)
        self.nodes[0].sendfromaddress(from_address, to_address, 0.5, "comment")
        self.nodes[0].sendfromaddress(from_address, to_address, 0.5, "comment", "to_comment")
        self.nodes[0].sendfromaddress(from_address, to_address, 0.5, "", "", False)
        self.nodes[0].sendfromaddress(from_address, to_address, 0.5, "", "", False, 6)
        self.nodes[0].sendfromaddress(from_address, to_address, 0.5, "", "", False, 6, "ECONOMICAL")

        assert_raises_rpc_error(-8, "Invalid estimate_mode parameter",
                                self.nodes[0].sendfromaddress, from_address, to_address, 0.5, "", "", False, 6, "INVALID")

        self.nodes[0].generate(1)
        self.sync_all()
        self.log.info("sendfromaddress argument parsing OK")

    def test_walletpassphrase_args(self):
        self.log.info("Testing walletpassphrase timeout bounds...")
        passphrase = "testpassphrase"
        self.nodes[0].node_encrypt_wallet(passphrase)
        self.start_node(0)
        locked_address = self.nodes[0].getnewaddress()

        assert_raises_rpc_error(-8, "Timeout must be a positive value",
                                self.nodes[0].walletpassphrase, passphrase, 0)
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase",
                                self.nodes[0].dumpprivkey, locked_address)
        assert_raises_rpc_error(-8, "Timeout must be a positive value",
                                self.nodes[0].walletpassphrase, passphrase, -1)
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase",
                                self.nodes[0].dumpprivkey, locked_address)
        assert_raises_rpc_error(-8, "Timeout must not exceed 86400",
                                self.nodes[0].walletpassphrase, passphrase, 86401)
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase",
                                self.nodes[0].dumpprivkey, locked_address)
        assert_raises_rpc_error(-8, "Timeout must not exceed 86400",
                                self.nodes[0].walletpassphrase, passphrase, 999999)
        assert_raises_rpc_error(-13, "Please enter the wallet passphrase",
                                self.nodes[0].dumpprivkey, locked_address)

        self.nodes[0].walletpassphrase(passphrase, 60)
        self.nodes[0].walletlock()

        self.nodes[0].walletpassphrase(passphrase, 86400)
        self.nodes[0].walletlock()

        self.log.info("walletpassphrase timeout bounds OK")


if __name__ == '__main__':
    WalletRpcArgsTest().main()
