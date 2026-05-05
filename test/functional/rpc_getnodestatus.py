#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the getnodestatus RPC.

Verifies:
  1. getnodestatus returns expected fields with stable JSON types.
  2. It works on regtest after at least one generated block.
  3. It works with wallet enabled and with -disablewallet.
  4. It does not require any mining, Qt, or wallet-only functionality.
"""

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal
from decimal import Decimal

NUMERIC = (int, float, Decimal)

EXPECTED_FIELDS = {
    "version":              int,
    "subversion":           str,
    "protocolversion":      int,
    "uptime":               int,
    "chain":                str,
    "blocks":               int,
    "headers":              int,
    "bestblockhash":        str,
    "difficulty":           NUMERIC,
    "mediantime":           int,
    "verificationprogress": NUMERIC,
    "initialblockdownload": bool,
    "connections":          int,
    "networkactive":        bool,
    "mempool_size":         int,
    "mempool_bytes":        int,
    "bans_count":           int,
    "warnings":             str,
}


def check_fields(obj, context=""):
    for field, expected_type in EXPECTED_FIELDS.items():
        assert field in obj, "{}missing field '{}'".format(context, field)
        actual = obj[field]
        assert isinstance(actual, expected_type), (
            "{}field '{}': expected {}, got {} (value={})".format(
                context, field, expected_type.__name__,
                type(actual).__name__, actual
            )
        )


class RPCGetNodeStatusTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [[], ["-disablewallet"]]

    def run_test(self):
        self.test_basic_field_types_wallet_enabled()
        self.test_after_block_generation()
        self.test_disablewallet()
        self.test_no_wallet_only_dependencies()

    def test_basic_field_types_wallet_enabled(self):
        self.log.info("Test: getnodestatus returns correct field types (wallet enabled)")
        node = self.nodes[0]
        result = node.getnodestatus()
        check_fields(result)
        assert_equal(result["chain"], "regtest")
        assert result["version"] > 0
        assert result["protocolversion"] > 0
        assert result["uptime"] >= 0
        assert result["connections"] >= 0
        assert isinstance(result["bestblockhash"], str) and len(result["bestblockhash"]) == 64

    def test_after_block_generation(self):
        self.log.info("Test: getnodestatus reflects generated blocks")
        node = self.nodes[0]
        before = node.getnodestatus()
        assert before["blocks"] == 0, "expected 0 blocks on clean chain"
        assert before["initialblockdownload"] is True

        node.generate(1)
        after = node.getnodestatus()
        assert_equal(after["blocks"], 1)
        assert len(after["bestblockhash"]) == 64
        assert after["headers"] >= 1
        assert after["mediantime"] > 0
        assert after["verificationprogress"] > 0

    def test_disablewallet(self):
        self.log.info("Test: getnodestatus works with -disablewallet")
        node = self.nodes[1]
        result = node.getnodestatus()
        check_fields(result, context="[disablewallet] ")
        assert_equal(result["chain"], "regtest")
        assert result["version"] > 0

    def test_no_wallet_only_dependencies(self):
        self.log.info("Test: getnodestatus does not contain wallet-specific fields")
        node = self.nodes[0]
        result = node.getnodestatus()
        wallet_fields = {"balance", "walletversion", "keypoolsize",
                         "unlocked_until", "paytxfee"}
        found_wallet_fields = wallet_fields & set(result.keys())
        assert found_wallet_fields == set(), (
            "getnodestatus must not expose wallet fields, found: {}".format(
                found_wallet_fields
            )
        )


if __name__ == '__main__':
    RPCGetNodeStatusTest().main()
