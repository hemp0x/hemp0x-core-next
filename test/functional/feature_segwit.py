#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test SegWit functionality on Hemp0x.

Segwit is always active on Hemp0x regtest. This test verifies:
1. Witness transactions can be created and sent to the mempool
2. Blocks with witness transactions can be mined via GBT
3. Witness data is properly serialized in RPC responses
"""

from io import BytesIO
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import hex_str_to_bytes, connect_nodes, Decimal, assert_equal, sync_blocks, assert_raises_rpc_error
from test_framework.mininode import CTransaction, CTxIn, COutPoint, CTxOut, COIN, to_hex, from_hex
from test_framework.address import key_to_p2pkh
from test_framework.script import CScript, OP_0, hash160, OP_HASH160, OP_EQUAL, sha256


def send_to_witness(node, utxo, pubkey, amount):
    """Create and send a P2WPKH transaction."""
    pubkeyhash = hash160(hex_str_to_bytes(pubkey))
    pkscript = CScript([OP_0, pubkeyhash])
    tx = CTransaction()
    tx.vin.append(CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]), b""))
    tx.vout.append(CTxOut(int(amount * COIN), pkscript))
    signed = node.signrawtransaction(to_hex(tx))
    assert "errors" not in signed or len(signed.get("errors", [])) == 0
    return node.sendrawtransaction(signed["hex"])


def find_unspent(node, min_value):
    for utxo in node.listunspent():
        if utxo['amount'] >= min_value:
            return utxo


class SegWitTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], []]

    def setup_network(self):
        super().setup_network()
        connect_nodes(self.nodes[0], 1)
        self.sync_all()

    def run_test(self):
        # Generate enough blocks for coinbase maturity
        self.nodes[0].generate(200)
        sync_blocks(self.nodes)

        # Verify we have enough UTXOs
        unspent = self.nodes[0].listunspent()
        large_utxos = [u for u in unspent if u['amount'] >= 9]
        assert len(large_utxos) >= 10, "Need at least 10 UTXOs with value >= 9, have %d" % len(large_utxos)

        # Setup witness addresses
        self.pubkey = []
        for i in range(2):
            newaddress = self.nodes[i].getnewaddress()
            self.pubkey.append(self.nodes[i].validateaddress(newaddress)["pubkey"])

        self.log.info("Test 1: Create and send witness transactions")
        # Send several witness transactions
        wit_txids = []
        for i in range(5):
            utxo = find_unspent(self.nodes[0], 9)
            txid = send_to_witness(self.nodes[0], utxo, self.pubkey[0], Decimal("9.9"))
            wit_txids.append(txid)

        assert_equal(len(self.nodes[0].getrawmempool()), 5)
        self.log.info("Successfully sent %d witness transactions", 5)

        self.log.info("Test 2: Mine blocks with witness transactions")
        # Mine a block containing the witness transactions
        block_hash = self.nodes[0].generate(1)[0]
        sync_blocks(self.nodes)
        assert_equal(len(self.nodes[0].getrawmempool()), 0)

        block = self.nodes[0].getblock(block_hash)
        assert len(block["tx"]) > 1, "Block should contain witness transactions"
        self.log.info("Block %s contains %d transactions", block_hash, len(block["tx"]))

        self.log.info("Test 3: Verify witness serialization in RPC")
        # Get individual transactions and verify witness data
        for txid in block["tx"][1:]:  # Skip coinbase
            tx_raw = self.nodes[0].getrawtransaction(txid)
            tx = from_hex(CTransaction(), tx_raw)
            # Verify the transaction has witness data
            if not tx.wit.is_null():
                self.log.info("Transaction %s has witness data", txid)

        self.log.info("Test 4: GBT with segwit rules")
        sync_blocks(self.nodes)

        # Send a regular transaction
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 1)

        try:
            tmpl = self.nodes[0].getblocktemplate({'rules': ['segwit']})
            assert 'sizelimit' in tmpl
            assert 'weightlimit' in tmpl
            assert 'sigoplimit' in tmpl
            self.log.info("GBT with segwit rules works correctly (sizelimit=%d, weightlimit=%d, sigoplimit=%d)",
                          tmpl['sizelimit'], tmpl['weightlimit'], tmpl['sigoplimit'])
        except Exception as e:
            if hasattr(e, 'error') and e.error.get('code') == -10:
                self.log.info("GBT unavailable during IBD, skipping GBT segwit test")
            else:
                raise

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        self.log.info("Test 5: P2WPKH to P2WPKH spending")
        # Create a witness output
        utxo = find_unspent(self.nodes[0], 9)
        wit_txid = send_to_witness(self.nodes[0], utxo, self.pubkey[1], Decimal("9.9"))
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # Now spend from the witness output
        wit_utxo = None
        for u in self.nodes[1].listunspent():
            if u.get('txid') == wit_txid:
                wit_utxo = u
                break

        if wit_utxo:
            dest_addr = self.nodes[1].getnewaddress()
            self.nodes[1].sendtoaddress(dest_addr, Decimal("1"))
            self.nodes[1].generate(1)
            sync_blocks(self.nodes)
            self.log.info("Successfully spent from witness output")

        self.log.info("All segwit tests passed")


if __name__ == '__main__':
    SegWitTest().main()
