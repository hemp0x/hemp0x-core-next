#!/usr/bin/env python3
"""Test the exportwalletmigration RPC (public-only export foundation)."""

import os
import json
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error

SECRET_FIELD_SUBSTRINGS = ["mnemonic", "xprv", "wif", "passphrase", "seed"]


def check_response_no_secrets(result, safe_fields=None):
    """Verify the RPC response object contains no secret-like field keys."""
    if safe_fields is None:
        safe_fields = {"mnemonic_available", "private_keys_present",
                       "private_keys_included"}
    for key in result:
        for sub in SECRET_FIELD_SUBSTRINGS:
            if sub in key and key not in safe_fields:
                raise AssertionError(
                    "Unexpected secret-like field in RPC response: {}".format(key))


def _object_has_secret_value(obj, parent_key=None):
    """Recursively check if any string *value* in obj contains secret substrings.
    Skips warnings text and safe metadata field keys."""
    safe_keys = {
        "schema_identifier",
        "mnemonic_available",
    }
    if isinstance(obj, dict):
        for k, v in obj.items():
            if isinstance(k, str) and k not in safe_keys:
                for sub in SECRET_FIELD_SUBSTRINGS:
                    if sub in k:
                        return True, k
            if k not in ("warnings",):
                result = _object_has_secret_value(v, parent_key=k)
                if result[0]:
                    return result
    elif isinstance(obj, list):
        if parent_key not in ("warnings",):
            for item in obj:
                result = _object_has_secret_value(item, parent_key=parent_key)
                if result[0]:
                    return result
    elif isinstance(obj, str):
        if parent_key not in ("warnings",):
            for sub in SECRET_FIELD_SUBSTRINGS:
                if sub in obj:
                    return True, obj[:80]
    return False, None


def check_envelope_no_secrets(envelope):
    """Verify the JSON envelope file contains no secret material."""
    found, detail = _object_has_secret_value(envelope)
    if found:
        raise AssertionError(
            "Unexpected secret-like value in envelope: {}".format(detail))

    for forbidden in ("private", "mnemonic", "seed"):
        if forbidden in envelope:
            raise AssertionError(
                "Forbidden top-level key in envelope: {}".format(forbidden))

    for key_entry in envelope.get("keys", []):
        if "private" in key_entry and key_entry["private"] is not None:
            raise AssertionError(
                "Key entry contains non-null private: {}".format(key_entry))


class WalletMigrationExportTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [["-keypool=20"]]

    def setup_network(self, split=False):
        self.add_nodes(self.num_nodes, self.extra_args, timewait=60)
        self.start_nodes()

    def run_test(self):
        tmpdir = self.options.tmpdir
        node = self.nodes[0]

        # Generate some addresses so the wallet has key material
        for _ in range(10):
            node.getnewaddress()

        export_path = os.path.join(tmpdir, "node0", "migration_public.json")

        # ---------- Test: RPC exists and help works ----------
        help_text = node.help("exportwalletmigration")
        assert "exportwalletmigration" in help_text
        assert "include_private" in help_text
        assert "PUBLIC-ONLY" in help_text or "public" in help_text.lower()

        # ---------- Test: public-only export creates valid JSON ----------
        result = node.exportwalletmigration(export_path, False, False)
        assert_equal(result["filename"], os.path.abspath(export_path))
        assert_equal(result["envelope_version"], 1)
        assert_equal(result["private_keys_included"], False)
        assert "encrypted" in result
        assert "locked" in result
        assert "hd_enabled" in result
        assert isinstance(result["warnings"], list)
        check_response_no_secrets(result)

        # Read and verify envelope file
        with open(export_path, "r") as f:
            envelope = json.load(f)

        assert_equal(envelope["envelope_version"], 1)
        assert_equal(envelope["schema_identifier"],
                     "hemp0x-core.migration-envelope.v1")
        assert "exported_at" in envelope
        assert_equal(envelope["source_client"], "hemp0x-core")
        assert "source_client_version" in envelope
        assert "chain" in envelope
        assert "network" in envelope["chain"]
        assert "coin_type_bip44" in envelope["chain"]
        assert "wallet_summary" in envelope
        assert_equal(envelope["wallet_summary"]["private_keys_included"], False)
        assert "derivation" in envelope
        for profile in envelope["derivation"]:
            assert "master_xpub" not in profile
            assert "account_xpub" not in profile
        assert "keys" in envelope
        assert isinstance(envelope["keys"], list)
        assert "watch_only_entries" in envelope
        assert "unsupported_records" in envelope
        assert "metadata" in envelope
        assert "warnings" in envelope
        check_envelope_no_secrets(envelope)

        os.remove(export_path)

        # ---------- Test: include_private=true is rejected ----------
        assert_raises_rpc_error(
            -4, "not implemented",
            node.exportwalletmigration, export_path, True, False)
        assert not os.path.exists(export_path)

        # ---------- Test: existing destination rejected by default ----------
        with open(export_path, "w") as f:
            f.write("{}")
        assert_raises_rpc_error(
            -8, "already exists",
            node.exportwalletmigration, export_path, False, False)
        os.remove(export_path)

        # ---------- Test: allow_overwrite=true works ----------
        with open(export_path, "w") as f:
            f.write("{}")
        result = node.exportwalletmigration(export_path, False, True)
        assert_equal(result["private_keys_included"], False)
        assert os.path.exists(export_path)
        os.remove(export_path)

        # ---------- Test: export with unlocked encrypted wallet (public-only) ----------
        node.encryptwallet("testpass")
        self.restart_node(0)
        node.walletpassphrase("testpass", 60)

        enc_export = os.path.join(tmpdir, "node0", "migration_enc_public.json")
        result = node.exportwalletmigration(enc_export, False, False)
        assert_equal(result["encrypted"], True)
        assert_equal(result["locked"], False)
        assert_equal(result["private_keys_included"], False)

        with open(enc_export, "r") as f:
            envelope = json.load(f)
        check_envelope_no_secrets(envelope)
        os.remove(enc_export)

        # lock wallet
        node.walletlock()

        # ---------- Test: public-only export with locked encrypted wallet ----------
        lock_export = os.path.join(tmpdir, "node0", "migration_locked_public.json")
        result = node.exportwalletmigration(lock_export, False, False)
        assert_equal(result["locked"], True)
        assert_equal(result["private_keys_included"], False)

        with open(lock_export, "r") as f:
            envelope = json.load(f)
        check_envelope_no_secrets(envelope)
        os.remove(lock_export)

        # ---------- Test: wallet.dat basename rejected ----------
        bad_path = os.path.join(tmpdir, "node0", "wallet.dat")
        assert_raises_rpc_error(
            -8, "wallet.dat",
            node.exportwalletmigration, bad_path, False, False)

        # ---------- Test: empty filename rejected ----------
        assert_raises_rpc_error(
            -8, "empty",
            node.exportwalletmigration, "", False, False)

        # ---------- Test: existing getwalletmigrationinfo still works ----------
        info = node.getwalletmigrationinfo()
        assert_equal(info["storage_backend"], "bdb")
        assert "wallet_name" in info


if __name__ == "__main__":
    WalletMigrationExportTest().main()
