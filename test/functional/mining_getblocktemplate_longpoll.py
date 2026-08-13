#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test longpolling with getblocktemplate."""

import threading
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import get_rpc_proxy, Decimal

class LongpollThread(threading.Thread):
    def __init__(self, node):
        threading.Thread.__init__(self)
        # query current longpollid
        template = node.getblocktemplate()
        self.longpollid = template['longpollid']
        # create a new connection to the node, we can't use the same
        # connection from two threads
        self.node = get_rpc_proxy(node.url, 1, timeout=600, coverage_dir=node.coverage_dir)

    def run(self):
        self.node.getblocktemplate({'longpollid':self.longpollid})

class GetBlockTemplateLPTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [["-bypassdownload=1"], ["-bypassdownload=1"]]

    def run_test(self):
        self.log.info("Warning: this test will take about 70 seconds in the best case. Be patient.")
        template = self.nodes[0].getblocktemplate()
        longpollid = template['longpollid']
        # longpollid should not change between successive invocations if nothing else happens
        template_2 = self.nodes[0].getblocktemplate()
        assert(template_2['longpollid'] == longpollid)

        # Test 1: test that the longpolling wait if we do nothing
        thr = LongpollThread(self.nodes[0])
        thr.start()
        # check that thread still lives
        thr.join(5)  # wait 5 seconds or until thread exits
        assert(thr.is_alive())

        # Test 2: test that longpoll will terminate if the active tip changes
        best_hash = self.nodes[0].getbestblockhash()
        self.nodes[0].invalidateblock(best_hash)
        # check that thread will exit now that the best block changed
        thr.join(5)  # wait 5 seconds or until thread exits
        assert(not thr.is_alive())
        self.nodes[0].reconsiderblock(best_hash)

        # Test 3: test that longpoll will terminate if we change the tip ourselves
        thr = LongpollThread(self.nodes[0])
        thr.start()
        best_hash = self.nodes[0].getbestblockhash()
        self.nodes[0].invalidateblock(best_hash)
        thr.join(5)  # wait 5 seconds or until thread exits
        assert(not thr.is_alive())
        self.nodes[0].reconsiderblock(best_hash)

        # Test 4: test that introducing a new transaction into the mempool will terminate the longpoll
        thr = LongpollThread(self.nodes[0])
        thr.start()
        self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), Decimal("1.1"))
        # after one minute, every 10 seconds the mempool is probed, so in 80 seconds it should have returned
        thr.join(60 + 20)
        assert(not thr.is_alive())

if __name__ == '__main__':
    GetBlockTemplateLPTest().main()
