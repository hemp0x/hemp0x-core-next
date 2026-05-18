#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test importwallet input-size guardrails."""

import os

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_raises_rpc_error


MAX_IMPORTWALLET_FILE_SIZE = 64 * 1024 * 1024
MAX_IMPORTWALLET_LINE_SIZE = 16 * 1024


class WalletImportWalletLimitsTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]

        self.log.info("Reject oversized importwallet dump files")
        oversized_file = os.path.join(self.options.tmpdir, "oversized-wallet.dump")
        with open(oversized_file, "wb") as f:
            f.truncate(MAX_IMPORTWALLET_FILE_SIZE + 1)

        assert_raises_rpc_error(
            -8,
            "Wallet dump file exceeds maximum import size",
            node.importwallet,
            oversized_file,
        )

        self.log.info("Reject oversized importwallet dump lines")
        oversized_line = os.path.join(self.options.tmpdir, "oversized-line-wallet.dump")
        with open(oversized_line, "w", encoding="utf8") as f:
            f.write("R" * (MAX_IMPORTWALLET_LINE_SIZE + 1))
            f.write("\n")

        assert_raises_rpc_error(
            -8,
            "Wallet dump file contains an oversized line",
            node.importwallet,
            oversized_line,
        )


if __name__ == '__main__':
    WalletImportWalletLimitsTest().main()
