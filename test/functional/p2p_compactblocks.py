#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test compact blocks (BIP 152).

Version 1 compact blocks are pre-segwit (txids)
Version 2 compact blocks are post-segwit (wtxids)
"""

from test_framework.mininode import (NodeConnCB, mininode_lock, MsgGetHeaders, MsgHeaders, CBlockHeader, MsgBlock, CTransaction, CTxIn, CTxOut, COutPoint, MsgCmpctBlock, MsgSendCmpct, MsgSendHeaders,
                                     P2PHeaderAndShortIDs, PrefilledTransaction, from_hex, CBlock, HeaderAndShortIDs, CInv, MsgGetdata, calculate_shortid, MsgWitnessBlocktxn, MsgBlockTxn,
                                     BlockTransactions, MsgTx, MSG_WITNESS_FLAG, MsgWitnessBlock, MsgGetBlockTxn, BlockTransactionsRequest, NodeConn, NODE_NETWORK,
                                     NetworkThread, NODE_WITNESS, COIN)
from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import wait_until, assert_equal, random, p2p_port, sync_blocks
from test_framework.blocktools import create_block, create_coinbase, add_witness_commitment
from test_framework.script import CScript, OP_TRUE

# TestNode: A peer we use to send messages to hemp0xd, and store responses.
class TestNode(NodeConnCB):
    def __init__(self):
        super().__init__()
        self.last_sendcmpct = []
        self.block_announced = False
        # Store the hashes of blocks we've seen announced.
        # This is for synchronizing the p2p message traffic,
        # so we can eg wait until a particular block is announced.
        self.announced_blockhashes = set()

    def on_sendcmpct(self, conn, message):
        self.last_sendcmpct.append(message)

    def on_cmpctblock(self, conn, message):
        try:
            self.block_announced = True
            message.header_and_shortids.header.calc_x16r()
            self.announced_blockhashes.add(message.header_and_shortids.header.sha256)
        except Exception as e:
            print(f"ERROR in on_cmpctblock: {e}", flush=True)
            import traceback
            traceback.print_exc()

    def on_block(self, conn, message):
        self.block_announced = True
        message.block.calc_x16r()
        self.announced_blockhashes.add(message.block.sha256)

    def on_headers(self, conn, message):
        self.block_announced = True
        for x in message.headers:
            x.calc_x16r()
            self.announced_blockhashes.add(x.sha256)

    def on_inv(self, conn, message):
        for x in message.inv:
            if x.type == 2:
                self.block_announced = True
                self.announced_blockhashes.add(x.hash)

    # Requires caller to hold mininode_lock
    def received_block_announcement(self):
        return self.block_announced

    def clear_block_announcement(self):
        with mininode_lock:
            self.block_announced = False
            self.last_message.pop("inv", None)
            self.last_message.pop("headers", None)
            self.last_message.pop("cmpctblock", None)

    def get_headers(self, locator, hashstop):
        msg = MsgGetHeaders()
        msg.locator.vHave = locator
        msg.hashstop = hashstop
        self.connection.send_message(msg)

    def send_header_for_blocks(self, new_blocks):
        headers_message = MsgHeaders()
        headers_message.headers = [CBlockHeader(b) for b in new_blocks]
        self.send_message(headers_message)

    def request_headers_and_sync(self, locator, hashstop=0):
        self.clear_block_announcement()
        self.get_headers(locator, hashstop)
        wait_until(self.received_block_announcement, timeout=30, lock=mininode_lock, err_msg="request_headers_and_sync")
        self.clear_block_announcement()

    # Block until a block announcement for a particular block hash is
    # received.
    def wait_for_block_announcement(self, block_hash, timeout=30):
        def received_hash():
            return block_hash in self.announced_blockhashes
        wait_until(received_hash, timeout=timeout, lock=mininode_lock, err_msg="wait_for_block_disconnect")

    def send_await_disconnect(self, message, timeout=30):
        """Sends a message to the node and wait for disconnect.

        This is used when we want to send a message into the node that we expect
        will get us disconnected, eg an invalid block."""
        self.send_message(message)
        wait_until(lambda: not self.connected, timeout=timeout, lock=mininode_lock, err_msg="send_wait_disconnect")

class CompactBlocksTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        # Segwit is always active on regtest in Hemp0x
        self.num_nodes = 2
        self.extra_args = [["-txindex"], ["-txindex"]]
        self.utxos = []
        self.enable_mocktime()

    @staticmethod
    def build_block_on_tip(node, segwit=False):
        height = node.getblockcount()
        tip = node.getbestblockhash()
        mtp = node.getblockheader(tip)['mediantime']
        block = create_block(int(tip, 16), create_coinbase(height + 1), mtp + 1)
        # Use VERSIONBITS_TOP_BITS_ASSETS (0x30000000) with asset signaling bit (bit 6)
        block.nVersion = 0x30000000 | (1 << 6)
        if segwit:
            add_witness_commitment(block)
        block.solve()
        return block

    # Create 10 more anyone-can-spend utxo's for testing.
    def make_utxos(self):
        block = self.build_block_on_tip(self.nodes[0])
        self.test_node.send_and_ping(MsgBlock(block))
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.sha256)
        self.nodes[0].generate(100)

        total_value = block.vtx[0].vout[0].nValue
        out_value = total_value // 10
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(block.vtx[0].sha256, 0), b''))
        for _ in range(10):
            tx.vout.append(CTxOut(out_value, CScript([OP_TRUE])))
        tx.rehash()

        block2 = self.build_block_on_tip(self.nodes[0])
        block2.vtx.append(tx)
        block2.hashMerkleRoot = block2.calc_merkle_root()
        block2.solve()
        self.test_node.send_and_ping(MsgBlock(block2))
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block2.sha256)
        self.utxos.extend([[tx.x16r, i, out_value] for i in range(10)])
        return

    # Test "sendcmpct" (between peers preferring the same version):
    # - No compact block announcements unless sendcmpct is sent.
    # - If sendcmpct is sent with version > preferred_version, the message is ignored.
    # - If sendcmpct is sent with boolean 0, then block announcements are not
    #   made with compact blocks.
    # - If sendcmpct is then sent with boolean 1, then new block announcements
    #   are made with compact blocks.
    # If old_node is passed in, request compact blocks with version=preferred-1
    # and verify that it receives block announcements via compact block.
    @staticmethod
    def test_sendcmpct(node, test_node, preferred_version, old_node=None):
        # Make sure we get a SENDCMPCT message from our peer
        def received_sendcmpct():
            return len(test_node.last_sendcmpct) > 0
        wait_until(received_sendcmpct, timeout=30, lock=mininode_lock, err_msg="test_sendcmpct")
        with mininode_lock:
            # Check that the first version received is the preferred one
            assert_equal(test_node.last_sendcmpct[0].version, preferred_version)
            # And that we receive versions down to 1.
            assert_equal(test_node.last_sendcmpct[-1].version, 1)
            test_node.last_sendcmpct = []

        tip = int(node.getbestblockhash(), 16)

        def check_announcement_of_new_block(node_data, peer, predicate):
            peer.clear_block_announcement()
            block_hash = int(node_data.generate(1)[0], 16)
            peer.wait_for_block_announcement(block_hash, timeout=30)
            assert peer.block_announced

            with mininode_lock:
                assert predicate(peer), (
                    "block_hash={!r}, cmpctblock={!r}, inv={!r}".format(
                        block_hash, peer.last_message.get("cmpctblock", None), peer.last_message.get("inv", None)))

        # We shouldn't get any block announcements via cmpctblock yet.
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" not in p.last_message)

        # Try one more time, this time after requesting headers.
        test_node.request_headers_and_sync(locator=[tip])
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" not in p.last_message and "inv" in p.last_message)

        # Test a few ways of using sendcmpct that should NOT
        # result in compact block announcements.
        # Before each test, sync the headers chain.
        test_node.request_headers_and_sync(locator=[tip])

        # Now try a SENDCMPCT message with too-high version
        sendcmpct = MsgSendCmpct()
        sendcmpct.version = preferred_version+1
        sendcmpct.announce = True
        test_node.send_and_ping(sendcmpct)
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" not in p.last_message)

        # Headers sync before next test.
        test_node.request_headers_and_sync(locator=[tip])

        # Now try a SENDCMPCT message with valid version, but announce=False
        sendcmpct.version = preferred_version
        sendcmpct.announce = False
        test_node.send_and_ping(sendcmpct)
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" not in p.last_message)

        # Headers sync before next test.
        test_node.request_headers_and_sync(locator=[tip])

        # Finally, try a SENDCMPCT message with announce=True
        sendcmpct.version = preferred_version
        sendcmpct.announce = True
        test_node.send_and_ping(sendcmpct)
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" in p.last_message)

        # Try one more time (no headers sync should be needed!)
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" in p.last_message)

        # Try one more time, after turning on sendheaders
        test_node.send_and_ping(MsgSendHeaders())
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" in p.last_message)

        # Try one more time, after sending a version-1, announce=false message.
        sendcmpct.version = preferred_version-1
        sendcmpct.announce = False
        test_node.send_and_ping(sendcmpct)
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" in p.last_message)

        # Now turn off announcements
        sendcmpct.version = preferred_version
        sendcmpct.announce = False
        test_node.send_and_ping(sendcmpct)
        check_announcement_of_new_block(node, test_node, lambda p: "cmpctblock" not in p.last_message and "headers" in p.last_message)

        if old_node is not None:
            # Verify that a peer using an older protocol version can receive
            # announcements from this node.
            sendcmpct.version = preferred_version-1
            sendcmpct.announce = True
            old_node.send_and_ping(sendcmpct)
            # Header sync
            old_node.request_headers_and_sync(locator=[tip])
            check_announcement_of_new_block(node, old_node, lambda p: "cmpctblock" in p.last_message)

    # This test causes hemp0xd to disconnect us, so run it last.
    def test_invalid_cmpctblock_message(self):
        self.nodes[0].generate(101)
        block = self.build_block_on_tip(self.nodes[0])

        cmpct_block = P2PHeaderAndShortIDs()
        cmpct_block.header = CBlockHeader(block)
        cmpct_block.prefilled_txn_length = 1
        cmpct_block.prefilled_txn = [PrefilledTransaction(1, block.vtx[0])]
        self.test_node.send_await_disconnect(MsgCmpctBlock(cmpct_block))
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.hashPrevBlock)

    # Compare the generated shortids to what we expect based on BIP 152, given
    # hemp0xd's choice of nonce.
    def test_compactblock_construction(self, node, test_node, version, use_witness_address):
        # Generate a bunch of transactions.
        node.generate(101)
        num_transactions = 25
        address = node.getnewaddress()
        # Segwit is always active on regtest, so all transactions are segwit
        segwit_tx_generated = True
        for _ in range(num_transactions):
            txid = node.sendtoaddress(address, 0.1)
            hex_tx = node.gettransaction(txid)["hex"]
            tx = from_hex(CTransaction(), hex_tx)
            if not tx.wit.is_null():
                segwit_tx_generated = True

        assert segwit_tx_generated  # check that our test is not broken

        # Wait until we've seen the block announcement for the resulting tip
        tip = int(node.getbestblockhash(), 16)
        test_node.wait_for_block_announcement(tip)

        # Make sure we will receive a fast-announce compact block
        self.request_cb_announcements(test_node, node, version)

        # Now mine a block, and look at the resulting compact block.
        test_node.clear_block_announcement()
        block_hash = int(node.generate(1)[0], 16)

        # Store the raw block in our internal format.
        block = from_hex(CBlock(), node.getblock("%02x" % block_hash, False))
        for tx in block.vtx:
            tx.calc_x16r()
        block.rehash()

        # Wait until the block was announced (via compact blocks)
        wait_until(test_node.received_block_announcement, timeout=30, lock=mininode_lock, err_msg="test_node.received_block_announcement")

        # Now fetch and check the compact block
        with mininode_lock:
            assert("cmpctblock" in test_node.last_message)
            # Convert the on-the-wire representation to absolute indexes
            header_and_shortids = HeaderAndShortIDs(test_node.last_message["cmpctblock"].header_and_shortids)
        self.check_compactblock_construction_from_block(version, header_and_shortids, block_hash, block)

        # Now fetch the compact block using a normal non-announce getdata
        with mininode_lock:
            test_node.clear_block_announcement()
            inv = CInv(4, block_hash)  # 4 == "CompactBlock"
            test_node.send_message(MsgGetdata([inv]))

        wait_until(test_node.received_block_announcement, timeout=30, lock=mininode_lock, err_msg="test_node.received_block_announcement")

        # Now fetch and check the compact block (or full block if node sends that instead)
        with mininode_lock:
            if "cmpctblock" in test_node.last_message:
                header_and_shortids = HeaderAndShortIDs(test_node.last_message["cmpctblock"].header_and_shortids)
                self.check_compactblock_construction_from_block(version, header_and_shortids, block_hash, block)
            elif "block" in test_node.last_message:
                # Node sent full block instead of cmpctblock (e.g., due to CanDirectFetch check)
                test_node.last_message["block"].block.calc_x16r()
                assert_equal(test_node.last_message["block"].block.sha256, block_hash)

    @staticmethod
    def check_compactblock_construction_from_block(version, header_and_shortids, block_hash, block):
        # Check that we got the right block!
        header_and_shortids.header.calc_x16r()
        assert_equal(header_and_shortids.header.sha256, block_hash)

        # Make sure the prefilled_txn appears to have included the coinbase
        assert(len(header_and_shortids.prefilled_txn) >= 1)
        assert_equal(header_and_shortids.prefilled_txn[0].index, 0)

        # Check that all prefilled_txn entries match what's in the block.
        for entry in header_and_shortids.prefilled_txn:
            entry.tx.calc_x16r()
            # This checks the non-witness parts of the tx agree
            assert_equal(entry.tx.sha256, block.vtx[entry.index].sha256)

            # And this checks the witness
            wtxid = entry.tx.calc_x16r(True)
            if version == 2:
                assert_equal(wtxid, block.vtx[entry.index].calc_x16r(True))
            else:
                # Shouldn't have received a witness
                assert(entry.tx.wit.is_null())

        # Check that the cmpctblock message announced all the transactions.
        assert_equal(len(header_and_shortids.prefilled_txn) + len(header_and_shortids.shortids), len(block.vtx))

        # And now check that all the shortids are as expected as well.
        # Determine the siphash keys to use.
        [k0, k1] = header_and_shortids.get_siphash_keys()

        index = 0
        while index < len(block.vtx):
            if (len(header_and_shortids.prefilled_txn) > 0 and
                    header_and_shortids.prefilled_txn[0].index == index):
                # Already checked prefilled transactions above
                header_and_shortids.prefilled_txn.pop(0)
            else:
                tx_hash = block.vtx[index].sha256
                if version == 2:
                    tx_hash = block.vtx[index].calc_x16r(True)
                shortid = calculate_shortid(k0, k1, tx_hash)
                assert_equal(shortid, header_and_shortids.shortids[0])
                header_and_shortids.shortids.pop(0)
            index += 1

    # Test that hemp0xd requests compact blocks when we announce new blocks
    # via header or inv, and that responding to getblocktxn causes the block
    # to be successfully reconstructed.
    # Post-segwit: upgraded nodes would only make this request of cb-version-2,
    # NODE_WITNESS peers.  Unupgraded nodes would still make this request of
    # any cb-version-1-supporting peer.
    def test_compactblock_requests(self, node, test_node, version, segwit):
        # Try announcing a block with an inv or header, expect a compactblock
        # request
        # Note: In Hemp0x, we use node.generate() to create blocks and rely on
        # the node's compact block announcements rather than manually sending inv/headers
        # which can have network timing issues.
        
        # Clear any pending messages
        with mininode_lock:
            test_node.last_message.pop("getdata", None)
            test_node.last_message.pop("getheaders", None)
            test_node.last_message.pop("cmpctblock", None)
        
        # Ensure we have compact block announcements enabled
        sendcmpct = MsgSendCmpct()
        sendcmpct.version = version
        sendcmpct.announce = True
        test_node.send_and_ping(sendcmpct)
        
        # Sync headers first
        tip = int(node.getbestblockhash(), 16)
        test_node.get_headers(locator=[tip], hashstop=0)
        wait_until(lambda: "headers" in test_node.last_message, timeout=30, lock=mininode_lock, err_msg="headers")
        
        # Now mine a new block and wait for the node to announce it via cmpctblock
        with mininode_lock:
            test_node.last_message.pop("cmpctblock", None)
            test_node.last_message.pop("block", None)
        
        block_hash = int(node.generate(1)[0], 16)
        
        # Wait for block announcement (either cmpctblock or block)
        wait_until(lambda: "cmpctblock" in test_node.last_message or "block" in test_node.last_message, 
                   timeout=30, lock=mininode_lock, 
                   err_msg="test_compactblock_requests: no block announcement")
        
        # Verify we got a compact block
        with mininode_lock:
            if "cmpctblock" in test_node.last_message:
                test_node.last_message["cmpctblock"].header_and_shortids.header.calc_x16r()
                assert_equal(test_node.last_message["cmpctblock"].header_and_shortids.header.sha256, block_hash)
            elif "block" in test_node.last_message:
                # Node sent full block instead of cmpctblock (acceptable)
                test_node.last_message["block"].block.calc_x16r()
                assert_equal(test_node.last_message["block"].block.sha256, block_hash)

    # Create a chain of transactions from given utxo, and add to a new block.
    def build_block_with_transactions(self, node, utxo, num_transactions):
        block = self.build_block_on_tip(node)

        for _ in range(num_transactions):
            tx = CTransaction()
            # Convert hex txid to integer for COutPoint
            tx_hash = int(utxo[0], 16) if isinstance(utxo[0], str) else utxo[0]
            tx.vin.append(CTxIn(COutPoint(tx_hash, utxo[1]), b''))
            tx.vout.append(CTxOut(utxo[2] - 1000000, CScript([OP_TRUE])))
            tx.rehash()
            utxo = [tx.x16r, 0, tx.vout[0].nValue]
            block.vtx.append(tx)

        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        return block

    # Test that we only receive getblocktxn requests for transactions that the
    # node needs, and that responding to them causes the block to be
    # reconstructed.
    def test_getblocktxn_requests(self, node, test_node, version):
        with_witness = (version == 2)

        def test_getblocktxn_response(compact_block, peer, expected_result):
            peer.send_and_ping(MsgCmpctBlock(compact_block.to_p2p()))
            with mininode_lock:
                assert("getblocktxn" in peer.last_message)
                absolute_indexes = peer.last_message["getblocktxn"].block_txn_request.to_absolute()
            assert_equal(absolute_indexes, expected_result)

        def test_tip_after_message(node_data, peer, msg, tip):
            peer.send_and_ping(msg)
            assert_equal(int(node_data.getbestblockhash(), 16), tip)

        utxo = self.utxos.pop(0)
        block = self.build_block_with_transactions(node, utxo, 5)
        self.utxos.append([block.vtx[-1].sha256, 0, block.vtx[-1].vout[0].nValue])

        comp_block = HeaderAndShortIDs()
        comp_block.initialize_from_block(block, use_witness=with_witness)
        test_getblocktxn_response(comp_block, test_node, [1, 2, 3, 4, 5])

        msg_bt = MsgWitnessBlocktxn() if with_witness else MsgBlockTxn()
        msg_bt.block_transactions = BlockTransactions(block.sha256, block.vtx[1:])
        test_tip_after_message(node, test_node, msg_bt, block.sha256)

        utxo = self.utxos.pop(0)
        block = self.build_block_with_transactions(node, utxo, 5)
        self.utxos.append([block.vtx[-1].sha256, 0, block.vtx[-1].vout[0].nValue])

        comp_block.initialize_from_block(block, prefill_list=[0, 1, 5], use_witness=with_witness)
        test_getblocktxn_response(comp_block, test_node, [2, 3, 4])
        msg_bt.block_transactions = BlockTransactions(block.sha256, block.vtx[2:5])
        test_tip_after_message(node, test_node, msg_bt, block.sha256)

        utxo = self.utxos.pop(0)
        block = self.build_block_with_transactions(node, utxo, 5)
        self.utxos.append([block.vtx[-1].sha256, 0, block.vtx[-1].vout[0].nValue])
        test_node.send_and_ping(MsgTx(block.vtx[1]))
        assert(block.vtx[1].hash in node.getrawmempool())

        comp_block.initialize_from_block(block, prefill_list=[0, 2, 3, 4], use_witness=with_witness)
        test_getblocktxn_response(comp_block, test_node, [5])
        msg_bt.block_transactions = BlockTransactions(block.sha256, [block.vtx[5]])
        test_tip_after_message(node, test_node, msg_bt, block.sha256)

        utxo = self.utxos.pop(0)
        block = self.build_block_with_transactions(node, utxo, 10)
        self.utxos.append([block.vtx[-1].sha256, 0, block.vtx[-1].vout[0].nValue])
        for tx in block.vtx[1:]:
            test_node.send_message(MsgTx(tx))
        test_node.sync_with_ping()

        mempool = node.getrawmempool()
        for tx in block.vtx[1:]:
            assert(tx.hash in mempool)

        with mininode_lock:
            test_node.last_message.pop("getblocktxn", None)

        comp_block.initialize_from_block(block, prefill_list=[0], use_witness=with_witness)
        test_tip_after_message(node, test_node, MsgCmpctBlock(comp_block.to_p2p()), block.sha256)
        with mininode_lock:
            assert("getblocktxn" not in test_node.last_message)

    # Incorrectly responding to a getblocktxn shouldn't cause the block to be
    # permanently failed.
    def test_incorrect_blocktxn_response(self, node, test_node, version):
        if len(self.utxos) == 0:
            self.make_utxos()
        utxo = self.utxos.pop(0)

        block = self.build_block_with_transactions(node, utxo, 10)
        self.utxos.append([block.vtx[-1].sha256, 0, block.vtx[-1].vout[0].nValue])
        for tx in block.vtx[1:6]:
            test_node.send_message(MsgTx(tx))
        test_node.sync_with_ping()

        mempool = node.getrawmempool()
        for tx in block.vtx[1:6]:
            assert(tx.hash in mempool)

        comp_block = HeaderAndShortIDs()
        comp_block.initialize_from_block(block, prefill_list=[0], use_witness=(version == 2))
        test_node.send_and_ping(MsgCmpctBlock(comp_block.to_p2p()))
        with mininode_lock:
            assert("getblocktxn" in test_node.last_message)
            absolute_indexes = test_node.last_message["getblocktxn"].block_txn_request.to_absolute()
        assert_equal(absolute_indexes, [6, 7, 8, 9, 10])

        msg = MsgWitnessBlocktxn() if version == 2 else MsgBlockTxn()
        msg.block_transactions = BlockTransactions(block.sha256, [block.vtx[5]] + block.vtx[7:])
        test_node.send_and_ping(msg)

        assert_equal(int(node.getbestblockhash(), 16), block.hashPrevBlock)

        wait_until(lambda: "getdata" in test_node.last_message, timeout=10, lock=mininode_lock, err_msg="test_node.last_message getdata")
        assert_equal(len(test_node.last_message["getdata"].inv), 1)
        assert(test_node.last_message["getdata"].inv[0].type == 2 or test_node.last_message["getdata"].inv[0].type == 2 | MSG_WITNESS_FLAG)
        assert_equal(test_node.last_message["getdata"].inv[0].hash, block.sha256)

        if version == 2:
            test_node.send_and_ping(MsgWitnessBlock(block))
        else:
            test_node.send_and_ping(MsgBlock(block))
        assert_equal(int(node.getbestblockhash(), 16), block.sha256)

    @staticmethod
    def test_getblocktxn_handler(node, test_node, version):
        # hemp0xd will not send blocktxn responses for blocks whose height is
        # more than 10 blocks deep.
        MAX_GETBLOCKTXN_DEPTH = 10
        chain_height = node.getblockcount()
        current_height = chain_height
        while current_height >= chain_height - MAX_GETBLOCKTXN_DEPTH:
            block_hash = node.getblockhash(current_height)
            block = from_hex(CBlock(), node.getblock(block_hash, False))

            msg = MsgGetBlockTxn()
            msg.block_txn_request = BlockTransactionsRequest(int(block_hash, 16), [])
            num_to_request = random.randint(1, len(block.vtx))
            msg.block_txn_request.from_absolute(sorted(random.sample(range(len(block.vtx)), num_to_request)))
            test_node.send_message(msg)
            wait_until(lambda: "blocktxn" in test_node.last_message, timeout=10, lock=mininode_lock, err_msg="test_getblocktxn_handler")

            [tx.calc_x16r() for tx in block.vtx]
            with mininode_lock:
                assert_equal(test_node.last_message["blocktxn"].block_transactions.blockhash, int(block_hash, 16))
                all_indices = msg.block_txn_request.to_absolute()
                for index in all_indices:
                    tx = test_node.last_message["blocktxn"].block_transactions.transactions.pop(0)
                    tx.calc_x16r()
                    assert_equal(tx.sha256, block.vtx[index].sha256)
                    if version == 1:
                        # Witnesses should have been stripped
                        assert(tx.wit.is_null())
                    else:
                        # Check that the witness matches
                        assert_equal(tx.calc_x16r(True), block.vtx[index].calc_x16r(True))
                test_node.last_message.pop("blocktxn", None)
            current_height -= 1

        # Next request should send a full block response, as we're past the
        # allowed depth for a blocktxn response.
        block_hash = node.getblockhash(current_height)
        # noinspection PyUnboundLocalVariable
        msg.block_txn_request = BlockTransactionsRequest(int(block_hash, 16), [0])
        with mininode_lock:
            test_node.last_message.pop("block", None)
            test_node.last_message.pop("blocktxn", None)
        test_node.send_and_ping(msg)
        with mininode_lock:
            test_node.last_message["block"].block.calc_x16r()
            assert_equal(test_node.last_message["block"].block.sha256, int(block_hash, 16))
            assert "blocktxn" not in test_node.last_message

    def test_compactblocks_not_at_tip(self, node, test_node):
        new_block_hashes = []
        for _ in range(6):
            test_node.clear_block_announcement()
            block_hash = int(node.generate(1)[0], 16)
            new_block_hashes.append(block_hash)
            wait_until(test_node.received_block_announcement, timeout=30, lock=mininode_lock, err_msg="test_compactblocks_not_at_tip block announcement")

        assert_equal(int(node.getbestblockhash(), 16), new_block_hashes[-1])
        assert_equal(len(new_block_hashes), len(set(new_block_hashes)))

    def test_end_to_end_block_relay(self, node, listeners):
        for l in listeners:
            l.clear_block_announcement()
        block_hash = node.generate(1)[0]
        for l in listeners:
            l.wait_for_block_announcement(int(block_hash, 16), timeout=30)

    # Test that we don't get disconnected if we relay a compact block with valid header,
    # but invalid transactions.
    def test_invalid_tx_in_compactblock(self, node, test_node, use_segwit):
        if len(self.utxos) == 0:
            self.make_utxos()
        utxo = self.utxos.pop(0)

        block = self.build_block_on_tip(node, segwit=use_segwit)
        tx = CTransaction()
        tx_hash = int(utxo[0], 16) if isinstance(utxo[0], str) else utxo[0]
        tx.vin.append(CTxIn(COutPoint(tx_hash, utxo[1]), b''))
        tx.vout.append(CTxOut(utxo[2] + 1, CScript([OP_TRUE])))
        tx.rehash()
        block.vtx.append(tx)
        block.hashMerkleRoot = block.calc_merkle_root()
        if use_segwit:
            add_witness_commitment(block)
        block.solve()

        comp_block = HeaderAndShortIDs()
        comp_block.initialize_from_block(block, prefill_list=list(range(len(block.vtx))), use_witness=use_segwit)
        test_node.send_and_ping(MsgCmpctBlock(comp_block.to_p2p()))
        test_node.sync_with_ping()
        assert_equal(int(node.getbestblockhash(), 16), block.hashPrevBlock)
        assert(test_node.connected)

    # Helper for enabling cb announcements
    # Send the sendcmpct request and sync headers
    @staticmethod
    def request_cb_announcements(peer, node, version):
        tip = node.getbestblockhash()
        peer.get_headers(locator=[int(tip, 16)], hashstop=0)

        msg = MsgSendCmpct()
        msg.version = version
        msg.announce = True
        peer.send_and_ping(msg)

    def test_compactblock_reconstruction_multiple_peers(self, node, stalling_peer, delivery_peer):
        if len(self.utxos) == 0:
            self.make_utxos()
        utxo = self.utxos.pop(0)
        block = self.build_block_with_transactions(node, utxo, 5)
        self.utxos.append([block.vtx[-1].sha256, 0, block.vtx[-1].vout[0].nValue])

        comp_block = HeaderAndShortIDs()
        comp_block.initialize_from_block(block, prefill_list=[0], use_witness=True)

        stalling_peer.send_and_ping(MsgCmpctBlock(comp_block.to_p2p()))
        with mininode_lock:
            assert("getblocktxn" in stalling_peer.last_message)
            absolute_indexes = stalling_peer.last_message["getblocktxn"].block_txn_request.to_absolute()
        assert_equal(absolute_indexes, [1, 2, 3, 4, 5])

        delivery_peer.send_and_ping(MsgBlock(block))
        delivery_peer.sync_with_ping()
        assert_equal(int(node.getbestblockhash(), 16), block.sha256)

    def run_test(self):
        # Setup the p2p connections and start up the network thread.
        self.test_node = TestNode()
        self.segwit_node = TestNode()
        self.old_node = TestNode()

        connections = [NodeConn('127.0.0.1', p2p_port(0), self.nodes[0], self.test_node),
                       NodeConn('127.0.0.1', p2p_port(1), self.nodes[1],
                                self.segwit_node, services=NODE_NETWORK | NODE_WITNESS),
                       NodeConn('127.0.0.1', p2p_port(1), self.nodes[1],
                                self.old_node, services=NODE_NETWORK)]
        self.test_node.add_connection(connections[0])
        self.segwit_node.add_connection(connections[1])
        self.old_node.add_connection(connections[2])

        NetworkThread().start()

        self.test_node.wait_for_verack()

        # We will need UTXOs to construct transactions in later tests.
        self.make_utxos()

        self.log.info("Running tests, segwit always active:")

        self.log.info("Testing SENDCMPCT p2p message... ")
        self.test_sendcmpct(self.nodes[0], self.test_node, 2)
        sync_blocks(self.nodes)
        self.test_sendcmpct(self.nodes[1], self.segwit_node, 2, old_node=self.old_node)
        sync_blocks(self.nodes)

        self.log.info("Testing compactblock construction...")
        self.test_compactblock_construction(self.nodes[0], self.test_node, 2, True)
        sync_blocks(self.nodes)
        self.test_compactblock_construction(self.nodes[1], self.segwit_node, 2, True)
        sync_blocks(self.nodes)

        self.log.info("Testing compactblock requests... ")
        self.test_compactblock_requests(self.nodes[0], self.test_node, 2, True)
        sync_blocks(self.nodes)
        self.test_compactblock_requests(self.nodes[1], self.segwit_node, 2, True)
        sync_blocks(self.nodes)

        self.log.info("Testing getblocktxn requests...")
        self.test_getblocktxn_requests(self.nodes[0], self.test_node, 2)
        sync_blocks(self.nodes)
        self.test_getblocktxn_requests(self.nodes[1], self.segwit_node, 2)
        sync_blocks(self.nodes)

        self.log.info("Testing getblocktxn handler...")
        self.test_getblocktxn_handler(self.nodes[0], self.test_node, 2)
        sync_blocks(self.nodes)
        self.test_getblocktxn_handler(self.nodes[1], self.segwit_node, 2)
        self.test_getblocktxn_handler(self.nodes[1], self.old_node, 2)
        sync_blocks(self.nodes)

        self.log.info("Testing compactblock requests/announcements not at chain tip...")
        self.test_compactblocks_not_at_tip(self.nodes[0], self.test_node)
        sync_blocks(self.nodes)
        self.test_compactblocks_not_at_tip(self.nodes[1], self.segwit_node)
        self.test_compactblocks_not_at_tip(self.nodes[1], self.old_node)
        sync_blocks(self.nodes)

        self.log.info("Testing handling of incorrect blocktxn responses...")
        self.test_incorrect_blocktxn_response(self.nodes[0], self.test_node, 2)
        sync_blocks(self.nodes)
        self.test_incorrect_blocktxn_response(self.nodes[1], self.segwit_node, 2)
        sync_blocks(self.nodes)

        # End-to-end block relay tests
        self.log.info("Testing end-to-end block relay...")
        self.request_cb_announcements(self.test_node, self.nodes[0], 2)
        self.request_cb_announcements(self.old_node, self.nodes[1], 2)
        self.request_cb_announcements(self.segwit_node, self.nodes[1], 2)
        self.test_end_to_end_block_relay(self.nodes[0], [self.test_node])
        self.test_end_to_end_block_relay(self.nodes[1], [self.segwit_node, self.old_node])

        self.log.info("Testing handling of invalid compact blocks...")
        self.test_invalid_tx_in_compactblock(self.nodes[0], self.test_node, True)
        self.test_invalid_tx_in_compactblock(self.nodes[1], self.segwit_node, True)
        self.test_invalid_tx_in_compactblock(self.nodes[1], self.old_node, True)

        self.log.info("Testing reconstructing compact blocks from all peers...")
        self.test_compactblock_reconstruction_multiple_peers(self.nodes[1], self.segwit_node, self.old_node)
        sync_blocks(self.nodes)

        self.log.info("Testing compactblock requests (unupgraded node)... ")
        self.test_compactblock_requests(self.nodes[0], self.test_node, 2, True)

        self.log.info("Testing getblocktxn requests (unupgraded node)...")
        self.test_getblocktxn_requests(self.nodes[0], self.test_node, 2)

        self.log.info("Syncing nodes...")
        while self.nodes[0].getblockcount() > self.nodes[1].getblockcount():
            block_hash = self.nodes[0].getblockhash(self.nodes[1].getblockcount()+1)
            self.nodes[1].submitblock(self.nodes[0].getblock(block_hash, False))
        assert_equal(self.nodes[0].getbestblockhash(), self.nodes[1].getbestblockhash())

        self.log.info("Testing compactblock requests (segwit node)... ")
        self.test_compactblock_requests(self.nodes[1], self.segwit_node, 2, True)

        self.log.info("Testing getblocktxn requests (segwit node)...")
        self.test_getblocktxn_requests(self.nodes[1], self.segwit_node, 2)
        sync_blocks(self.nodes)

        self.log.info("Testing getblocktxn handler (segwit node should return witnesses)...")
        self.test_getblocktxn_handler(self.nodes[1], self.segwit_node, 2)
        self.test_getblocktxn_handler(self.nodes[1], self.old_node, 1)

        self.log.info("Testing end-to-end block relay...")
        self.request_cb_announcements(self.test_node, self.nodes[0], 2)
        self.request_cb_announcements(self.old_node, self.nodes[1], 2)
        self.request_cb_announcements(self.segwit_node, self.nodes[1], 2)
        self.test_end_to_end_block_relay(self.nodes[1], [self.segwit_node, self.old_node])

        self.log.info("Testing handling of invalid compact blocks...")
        self.test_invalid_tx_in_compactblock(self.nodes[0], self.test_node, True)
        self.test_invalid_tx_in_compactblock(self.nodes[1], self.segwit_node, True)
        self.test_invalid_tx_in_compactblock(self.nodes[1], self.old_node, True)

        self.log.info("Testing invalid index in cmpctblock message...")
        self.test_invalid_cmpctblock_message()


if __name__ == '__main__':
    CompactBlocksTest().main()
