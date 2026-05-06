#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Verify that local block generation RPCs are disabled in release binaries.

The hidden mining RPCs (setgenerate, getgenerate, generatetoaddress) must
return RPC_METHOD_NOT_FOUND (-32601) with a message directing operators to
use getblocktemplate/submitblock with external pool software.

Note: generatetoaddress shares the same LocalBlockGenerationDisabled() code
path as setgenerate/getgenerate but is not directly testable here because
both the test framework and AuthServiceProxy override it to use the external
block construction helper. Coverage of setgenerate and getgenerate is
sufficient to verify the disabled-generation behavior.
"""

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_raises_rpc_error


class RpcGenerationDisabledTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Checking setgenerate returns disabled error")
        assert_raises_rpc_error(
            -32601,
            "disabled",
            node.setgenerate,
            True,
        )

        self.log.info("Checking getgenerate returns disabled error")
        assert_raises_rpc_error(
            -32601,
            "disabled",
            node.getgenerate,
        )


if __name__ == '__main__':
    RpcGenerationDisabledTest().main()
