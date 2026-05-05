#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test RPC exposure warning messages.

Verifies that the daemon logs clear operator-facing warnings when:
  - RPC is configured with -rpcallowip (external exposure),
  - REST is enabled alongside -rpcallowip (unauthenticated endpoints),
  - RPC binds specific addresses (address logging at normal level).
"""

import os
from test_framework.test_framework import Hemp0xTestFramework


class RPCExposureWarningsTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def setup_network(self):
        # Nodes are started per-test with custom args; nothing to do here.
        pass

    def _read_debug_log(self):
        """Read the full debug.log for node 0."""
        debug_log = os.path.join(self.nodes[0].datadir, "regtest", "debug.log")
        with open(debug_log, encoding='utf-8') as f:
            return f.read()

    def _assert_log_contains(self, expected_msg):
        log_content = self._read_debug_log()
        assert expected_msg in log_content, (
            'Expected "{}" in debug.log but not found.\nLog tail:\n{}'.format(
                expected_msg, log_content[-2000:]
            )
        )

    def run_test(self):
        self.test_loopback_default_warning()
        self.test_rpcallowip_binds_all_warning()
        self.test_bound_address_logging()
        self.test_rest_rpcallowip_warning()

    # -----------------------------------------------------------------
    # Test: default loopback binding message
    # -----------------------------------------------------------------
    def test_loopback_default_warning(self):
        self.log.info("Test: default config logs loopback-only message")
        self.add_nodes(1)
        self.start_node(0, ['-disablewallet', '-nolisten'])
        self._assert_log_contains("RPC/HTTP interface is limited to loopback addresses by default")
        self.stop_nodes()
        self.nodes = []

    # -----------------------------------------------------------------
    # Test: -rpcallowip without -rpcbind warns about binding all interfaces
    # -----------------------------------------------------------------
    def test_rpcallowip_binds_all_warning(self):
        self.log.info("Test: -rpcallowip without -rpcbind warns about all interfaces")
        self.add_nodes(1)
        self.start_node(0, ['-disablewallet', '-nolisten', '-rpcallowip=0.0.0.0/0'])
        self._assert_log_contains("will bind on all network interfaces; use -rpcbind to narrow exposure")
        self.stop_nodes()
        self.nodes = []

    # -----------------------------------------------------------------
    # Test: bound addresses logged at normal (not debug-only) level
    # -----------------------------------------------------------------
    def test_bound_address_logging(self):
        self.log.info("Test: bound RPC addresses are logged at normal level")
        self.add_nodes(1)
        self.start_node(0, ['-disablewallet', '-nolisten', '-rpcallowip=127.0.0.1', '-rpcbind=127.0.0.1'])
        self._assert_log_contains("Binding RPC on address 127.0.0.1 port")
        self.stop_nodes()
        self.nodes = []

    # -----------------------------------------------------------------
    # Test: -rest + -rpcallowip warns about unauthenticated REST
    # -----------------------------------------------------------------
    def test_rest_rpcallowip_warning(self):
        self.log.info("Test: -rest with -rpcallowip warns about unauthenticated REST")
        self.add_nodes(1)
        self.start_node(0, ['-disablewallet', '-nolisten', '-rest', '-rpcallowip=0.0.0.0/0'])
        self._assert_log_contains("REST endpoints are unauthenticated and are protected only by the -rpcallowip HTTP allow-list")
        self.stop_nodes()
        self.nodes = []


if __name__ == '__main__':
    RPCExposureWarningsTest().main()
