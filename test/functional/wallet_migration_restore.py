#!/usr/bin/env python3
"""Test the restorewalletmigration RPC: parameter validation, v2 roundtrip, restart, and leak checks.

The regtest node is started with -bip44=1 (test_node.py:56) so it
auto-creates a BIP39/BIP44 coin420 wallet with mnemonic data. This lets us
exercise the full export-v2-private > restore roundtrip from a functional
test without a separate mnemonic-import RPC.
"""

import json
import os
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

SECRET_FIELD_SUBSTRINGS = ["mnemonic", "xprv", "wif", "passphrase", "seed"]


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
            node.restorewalletmigration(exp_path)
            raise AssertionError("Should have raised exception")
        except Exception as e:
            assert ("number of parameters" in str(e)
                    or "required" in str(e).lower()
                    or "too few" in str(e).lower())

        # ---------- existing RPCs still work ----------
        node.exportwalletmigration(exp_path, False, False)
        result = node.exportwalletmigration(exp_path, False, True)
        assert_equal(result["envelope_version"], 1)
        os.remove(exp_path)

        node.exportwalletmigration(exp_path, False, False)
        vresult = node.validatewalletmigration(exp_path)
        assert_equal(vresult["valid"], True)
        os.remove(exp_path)

        # ============================================================
        # FULL ROUNDTRIP: export v2 private > restore > verify > restart
        # ============================================================

        source_addr = node.getnewaddress()
        source_validation = node.validateaddress(source_addr)
        assert_equal(source_validation["isvalid"], True)
        assert_equal(source_validation["ismine"], True)
        assert "hdkeypath" in source_validation, \
            "source wallet must have HD key path"
        assert "hdseedid" in source_validation, \
            "source wallet must have hdseedid"

        exp_roundtrip_path = os.path.join(tmpdir, "node0",
                                          "migration_roundtrip.json")
        restore_wallet_name = "roundtrip_restored"

        export_pass = "my export passphrase"
        exp_result = node.exportwalletmigration(
            exp_roundtrip_path, True, False, export_pass)
        assert_equal(exp_result["envelope_version"], 2)
        assert_equal(exp_result["private_keys_included"], True)

        restore_result = node.restorewalletmigration(
            exp_roundtrip_path, restore_wallet_name, export_pass)

        assert_equal(restore_result["wallet_name"], restore_wallet_name)
        assert restore_result["wallet_file"].startswith(node.datadir)
        assert_equal(restore_result["wallet_arg"], restore_wallet_name)
        assert isinstance(restore_result["coin_type"], int)
        assert_equal(restore_result["coin_type"], 420)
        assert "account" in restore_result
        assert "keypool_size_after_restore" in restore_result

        safe_restore_keys = {"mnemonic_language", "mnemonic_word_count"}
        for key in restore_result:
            if key in safe_restore_keys:
                continue
            for sub in SECRET_FIELD_SUBSTRINGS:
                assert sub not in key, \
                    "secret-like field {} in restore response".format(key)

        restored = node.get_wallet_rpc(restore_wallet_name)
        info = restored.getwalletinfo()
        assert_equal(info["walletname"], restore_wallet_name)
        assert info["hdseedid"] is not None, "restored wallet missing hdseedid"
        assert len(str(info["hdseedid"])) > 0

        restored_new_addr = restored.getnewaddress()
        new_validation = restored.validateaddress(restored_new_addr)
        assert_equal(new_validation["isvalid"], True)
        assert new_validation["ismine"], \
            "restored wallet does not own its own getnewaddress: {}".format(
                new_validation)
        assert "hdkeypath" in new_validation, \
            "restored wallet address missing hdkeypath"

        # ---------- Restart with -wallet=<wallet_arg> ----------
        self.restart_node(0, extra_args=[
            "-keypool=20",
            "-wallet=" + restore_wallet_name,
        ])

        post_restart = node.get_wallet_rpc(restore_wallet_name)
        post_info = post_restart.getwalletinfo()
        assert_equal(post_info["walletname"], restore_wallet_name)

        # ---------- Invalid mnemonic errors do not leak BIP39 words ----------
        with open(exp_roundtrip_path, "r") as f:
            envelope = json.load(f)
        tampered_path = os.path.join(tmpdir, "node0",
                                     "migration_tampered.json")
        envelope["private"]["ciphertext"] = "deadbeefcafebabe" + \
            envelope["private"]["ciphertext"][len("deadbeefcafebabe"):]
        with open(tampered_path, "w") as f:
            json.dump(envelope, f)

        try:
            node.restorewalletmigration(tampered_path, "leak_test_wallet",
                                        export_pass)
            raise AssertionError("Tampered ciphertext should have been rejected")
        except Exception as e:
            error_str = str(e).lower()
            for word in ["abandon", "zoo", "legal", "winner"]:
                assert word.lower() not in error_str, \
                    "Error message leaked BIP39 word '{}': {}".format(
                        word, str(e)[:200])

        os.remove(tampered_path)
        os.remove(exp_roundtrip_path)


if __name__ == "__main__":
    WalletMigrationRestoreTest().main()
