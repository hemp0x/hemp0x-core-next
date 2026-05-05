#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test RPC auth-failure throttling.

Verifies per-source auth-failure cooldown behavior:
  - Below threshold: 401 on wrong auth
  - At/after threshold: 403 during cooldown
  - Successful auth clears failure counter
  - Cooldown expires after configured ban duration
  - Disabling with -rpcmaxauthfailures=0 preserves legacy 401 behavior
  - Credentials never appear in debug.log
"""

import os
import time
import http.client
import urllib.parse
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import str_to_b64str, assert_equal


class RPCAuthThrottlingTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def setup_network(self):
        # Nodes are started per-test with custom args; nothing to do here.
        pass

    def _make_rpc_request(self, url, auth_pair, expect_status):
        """Make an RPC request and assert the HTTP status code."""
        headers = {"Authorization": "Basic " + str_to_b64str(auth_pair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        resp = conn.getresponse()
        status = resp.status
        resp.read()  # consume response
        conn.close()
        assert_equal(status, expect_status)
        return status

    def _kill_node(self):
        """Kill a node that is in cooldown (cannot use RPC stop)."""
        self.nodes[0].process.kill()
        self.nodes[0].process.wait()
        self.nodes[0].cleanup_on_exit = False
        self.nodes = []

    def run_test(self):
        self.test_below_threshold_returns_401()
        self.test_threshold_triggers_403_cooldown()
        self.test_success_clears_failures()
        self.test_cooldown_blocks_correct_auth()
        self.test_cooldown_expires()
        self.test_disable_throttling()
        self.test_no_credentials_in_log()

    # -----------------------------------------------------------------
    # Test 1: Below threshold, wrong auth returns 401
    # -----------------------------------------------------------------
    def test_below_threshold_returns_401(self):
        self.log.info("Test: below threshold, wrong auth returns 401")
        self.add_nodes(1)
        self.start_node(0, [
            '-disablewallet', '-nolisten',
            '-rpcmaxauthfailures=5', '-rpcauthfailurewindow=60', '-rpcauthfailureban=2',
        ])
        url = urllib.parse.urlparse(self.nodes[0].url)
        correct_auth = url.username + ':' + url.password
        wrong_auth = url.username + ':wrongpassword'

        # Send 4 wrong requests (below threshold of 5)
        for _ in range(4):
            self._make_rpc_request(url, wrong_auth, 401)

        # Correct auth should still work
        self._make_rpc_request(url, correct_auth, 200)

        self.stop_nodes()
        self.nodes = []

    # -----------------------------------------------------------------
    # Test 2: At threshold, cooldown triggers 403
    # -----------------------------------------------------------------
    def test_threshold_triggers_403_cooldown(self):
        self.log.info("Test: at threshold, cooldown triggers 403")
        self.add_nodes(1)
        self.start_node(0, [
            '-disablewallet', '-nolisten',
            '-rpcmaxauthfailures=3', '-rpcauthfailurewindow=60', '-rpcauthfailureban=60',
        ])
        url = urllib.parse.urlparse(self.nodes[0].url)
        wrong_auth = url.username + ':wrongpassword'

        # Send 3 wrong requests to hit threshold
        for _ in range(3):
            self._make_rpc_request(url, wrong_auth, 401)

        # Next request should be 403 (cooldown)
        self._make_rpc_request(url, wrong_auth, 403)

        # Node is in cooldown, kill it directly
        self._kill_node()

    # -----------------------------------------------------------------
    # Test 3: Successful auth clears failure counter
    # -----------------------------------------------------------------
    def test_success_clears_failures(self):
        self.log.info("Test: successful auth clears failure counter")
        self.add_nodes(1)
        self.start_node(0, [
            '-disablewallet', '-nolisten',
            '-rpcmaxauthfailures=3', '-rpcauthfailurewindow=60', '-rpcauthfailureban=2',
        ])
        url = urllib.parse.urlparse(self.nodes[0].url)
        correct_auth = url.username + ':' + url.password
        wrong_auth = url.username + ':wrongpassword'

        # Send 2 wrong requests (just below threshold)
        for _ in range(2):
            self._make_rpc_request(url, wrong_auth, 401)

        # Successful auth should clear the counter
        self._make_rpc_request(url, correct_auth, 200)

        # Now 2 more wrong requests should still be 401 (counter was reset)
        for _ in range(2):
            self._make_rpc_request(url, wrong_auth, 401)

        # Correct auth should still work (count=2, below threshold)
        self._make_rpc_request(url, correct_auth, 200)

        self.stop_nodes()
        self.nodes = []

    # -----------------------------------------------------------------
    # Test 4: Cooldown blocks correct credentials
    # -----------------------------------------------------------------
    def test_cooldown_blocks_correct_auth(self):
        self.log.info("Test: cooldown blocks correct credentials")
        self.add_nodes(1)
        self.start_node(0, [
            '-disablewallet', '-nolisten',
            '-rpcmaxauthfailures=3', '-rpcauthfailurewindow=60', '-rpcauthfailureban=60',
        ])
        url = urllib.parse.urlparse(self.nodes[0].url)
        correct_auth = url.username + ':' + url.password
        wrong_auth = url.username + ':wrongpassword'

        # Trigger cooldown
        for _ in range(3):
            self._make_rpc_request(url, wrong_auth, 401)

        # Even correct credentials should get 403 during cooldown
        self._make_rpc_request(url, correct_auth, 403)

        # Node is in cooldown, kill it directly
        self._kill_node()

    # -----------------------------------------------------------------
    # Test 5: Cooldown expires after configured duration
    # -----------------------------------------------------------------
    def test_cooldown_expires(self):
        self.log.info("Test: cooldown expires after configured duration")
        self.add_nodes(1)
        self.start_node(0, [
            '-disablewallet', '-nolisten',
            '-rpcmaxauthfailures=3', '-rpcauthfailurewindow=60', '-rpcauthfailureban=2',
        ])
        url = urllib.parse.urlparse(self.nodes[0].url)
        correct_auth = url.username + ':' + url.password
        wrong_auth = url.username + ':wrongpassword'

        # Trigger cooldown
        for _ in range(3):
            self._make_rpc_request(url, wrong_auth, 401)
        self._make_rpc_request(url, wrong_auth, 403)

        # Wait for cooldown to expire (ban=2s, add margin)
        time.sleep(3)

        # Should be able to authenticate again
        self._make_rpc_request(url, correct_auth, 200)

        self.stop_nodes()
        self.nodes = []

    # -----------------------------------------------------------------
    # Test 6: Disabling with -rpcmaxauthfailures=0 preserves 401 behavior
    # -----------------------------------------------------------------
    def test_disable_throttling(self):
        self.log.info("Test: -rpcmaxauthfailures=0 disables throttling")
        self.add_nodes(1)
        self.start_node(0, [
            '-disablewallet', '-nolisten',
            '-rpcmaxauthfailures=0', '-rpcauthfailurewindow=60', '-rpcauthfailureban=2',
        ])
        url = urllib.parse.urlparse(self.nodes[0].url)
        wrong_auth = url.username + ':wrongpassword'
        correct_auth = url.username + ':' + url.password

        # Send many wrong requests - should never get 403
        for _ in range(10):
            self._make_rpc_request(url, wrong_auth, 401)

        # Correct auth should still work
        self._make_rpc_request(url, correct_auth, 200)

        self.stop_nodes()
        self.nodes = []

    # -----------------------------------------------------------------
    # Test 7: No credentials appear in debug.log
    # -----------------------------------------------------------------
    def test_no_credentials_in_log(self):
        self.log.info("Test: no credentials in debug.log")
        self.add_nodes(1)
        self.start_node(0, [
            '-disablewallet', '-nolisten',
            '-rpcmaxauthfailures=3', '-rpcauthfailurewindow=60', '-rpcauthfailureban=60',
        ])
        url = urllib.parse.urlparse(self.nodes[0].url)
        wrong_password = "SuperSecretPassword123!"
        wrong_auth = url.username + ':' + wrong_password

        # Send wrong requests
        for _ in range(3):
            self._make_rpc_request(url, wrong_auth, 401)

        # Check debug.log does not contain the password
        debug_log = os.path.join(self.nodes[0].datadir, "regtest", "debug.log")
        with open(debug_log, encoding='utf-8') as dl:
            log_content = dl.read()
        assert wrong_password not in log_content, "Leaked password in debug log"

        # Node is in cooldown, kill it directly
        self._kill_node()


if __name__ == '__main__':
    RPCAuthThrottlingTest().main()
