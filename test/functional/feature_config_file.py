#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test default configuration file creation and fallback handling."""

import os
import re
import subprocess

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import wait_until


class ConfigFileTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.num_nodes = 0
        self.setup_clean_chain = True

    def setup_network(self):
        pass

    def _start_node(self, datadir):
        binary = os.path.join(self.options.srcdir, "hemp0xd")
        return subprocess.Popen([
            binary,
            "-regtest",
            "-datadir={}".format(datadir),
            "-daemon=0",
            "-listen=0",
            "-dnsseed=0",
            "-connect=0",
            "-disablewallet",
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def _stop_node(self, process):
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=10)

    def _debug_log(self, datadir):
        path = os.path.join(datadir, "regtest", "debug.log")
        if not os.path.exists(path):
            return ""
        with open(path, encoding="utf-8") as f:
            return f.read()

    def test_default_config_created(self):
        self.log.info("Test: default hemp.conf is created on first launch")
        datadir = os.path.join(self.options.tmpdir, "default_config")
        os.makedirs(datadir)
        process = self._start_node(datadir)
        try:
            path = os.path.join(datadir, "hemp.conf")
            wait_until(lambda: os.path.exists(path), timeout=10, err_msg="hemp.conf was not created")
            with open(path, encoding="utf-8") as f:
                config_text = f.read()
            assert "Hemp0x Core configuration file" in config_text
            assert "server=1" in config_text
            assert "rpcbind=127.0.0.1" in config_text
            assert "rpcallowip=127.0.0.1" in config_text
            assert re.search(r"^#rpcuser=hemp0xrpc_[0-9a-f]{8}$", config_text, re.MULTILINE)
            assert re.search(r"^#rpcpassword=[0-9a-f]{64}$", config_text, re.MULTILINE)
            assert "#addnode=154.38.164.123:42069" in config_text
            assert "#addnode=147.93.185.184:42069" in config_text
            wait_until(
                lambda: "Created default config file" in self._debug_log(datadir),
                timeout=10,
                err_msg="config creation was not logged",
            )
            wait_until(
                lambda: os.path.exists(os.path.join(datadir, "regtest", ".cookie")),
                timeout=10,
                err_msg="RPC cookie was not created for first-run local RPC",
            )
        finally:
            self._stop_node(process)

    def test_legacy_fallback_not_overwritten(self):
        self.log.info("Test: legacy hemp0x.conf fallback is preserved")
        datadir = os.path.join(self.options.tmpdir, "fallback_config")
        os.makedirs(datadir)
        fallback_path = os.path.join(datadir, "hemp0x.conf")
        with open(fallback_path, "w", encoding="utf-8") as f:
            f.write("# existing fallback config\n")

        process = self._start_node(datadir)
        try:
            wait_until(
                lambda: os.path.exists(os.path.join(datadir, "regtest", "debug.log")),
                timeout=10,
                err_msg="debug.log was not created",
            )
            assert not os.path.exists(os.path.join(datadir, "hemp.conf"))
            log_text = self._debug_log(datadir)
            assert "Using config file {}".format(fallback_path) in log_text
            assert "Created default config file" not in log_text
        finally:
            self._stop_node(process)

    def run_test(self):
        self.test_default_config_created()
        self.test_legacy_fallback_not_overwritten()


if __name__ == "__main__":
    ConfigFileTest().main()
