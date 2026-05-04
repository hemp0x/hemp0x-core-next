#!/usr/bin/env python3
"""Test the restorewalletmigration RPC (v2 restore, v1 rejection, failure atomicity).

Because the regtest wallet created by the wallet fixture may not have BIP44
mnemonic data, the full roundtrip is covered by C++ unit tests instead.
This functional test covers the RPC surface: help, argument validation,
wallet-name rejection, and v1 envelope rejection.
"""

import os
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class WalletMigrationRestoreTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-keypool=20"]]

    def setup_network(self, split=False):
        self.add_nodes(self.num_nodes, self.extra_args, timewait=60)
        self.start_nodes()

    def run_test(self):
        tmpdir = self.options.tmpdir
        node = self.nodes[0]

        for _ in range(5):
            node.getnewaddress()

        exp_path = os.path.join(tmpdir, "node0", "migration_restore_test.json")

        # ---------- RPC exists and help works ----------
        help_text = node.help("restorewalletmigration")
        assert "restorewalletmigration" in help_text
        assert "wallet_name" in help_text
        assert "filename" in help_text

        # ---------- v1 public envelope restore rejects ----------
        node.exportwalletmigration(exp_path, False, False)
        assert_raises_rpc_error(
            -8, "",
            node.restorewalletmigration, exp_path, "rname_v1", "irrelevant")
        assert not os.path.exists(os.path.join(node.datadir, "rname_v1"))
        os.remove(exp_path)

        # ---------- invalid wallet name (empty) rejects ----------
        node.exportwalletmigration(exp_path, False, False)
        assert_raises_rpc_error(
            -8, "",
            node.restorewalletmigration, exp_path, "", "passphrase")
        os.remove(exp_path)

        # ---------- invalid wallet name (path traversal) rejects ----------
        node.exportwalletmigration(exp_path, False, False)
        assert_raises_rpc_error(
            -8, "",
            node.restorewalletmigration, exp_path, "../../escape", "passphrase")
        os.remove(exp_path)

        # ---------- invalid wallet name (path separator) rejects ----------
        node.exportwalletmigration(exp_path, False, False)
        assert_raises_rpc_error(
            -8, "",
            node.restorewalletmigration, exp_path, "path/name", "passphrase")
        os.remove(exp_path)

        # ---------- existing directory rejects before validation ----------
        node.exportwalletmigration(exp_path, False, False)
        existing_name = "existing_wallet_dir"
        existing_dir = os.path.join(node.datadir, existing_name)
        os.makedirs(existing_dir, exist_ok=True)
        assert_raises_rpc_error(
            -8, "",
            node.restorewalletmigration, exp_path, existing_name, "passphrase")
        os.rmdir(existing_dir)
        os.remove(exp_path)

        # ---------- missing required params rejects ----------
        try:
            node.restorewalletmigration(exp_path)  # missing wallet_name + passphrase
            raise AssertionError("Should have raised exception")
        except Exception as e:
            assert "number of parameters" in str(e) or "required" in str(e).lower() or "too few" in str(e).lower()

        # ---------- existing exportwalletmigration still works ----------
        node.exportwalletmigration(exp_path, False, False)
        result = node.exportwalletmigration(exp_path, False, True)
        assert_equal(result["envelope_version"], 1)
        os.remove(exp_path)

        # ---------- existing validatewalletmigration still works ----------
        node.exportwalletmigration(exp_path, False, False)
        vresult = node.validatewalletmigration(exp_path)
        assert_equal(vresult["valid"], True)
        os.remove(exp_path)


if __name__ == "__main__":
    WalletMigrationRestoreTest().main()
