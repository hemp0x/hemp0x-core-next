#!/usr/bin/env python3
# Copyright (c) 2017-2020 The Raven Core developers
# Copyright (c) 2021-2026 The Hemp0x Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Testing the optional full message index (-messageindex=1), rescanmessages
backfill/recovery, getmessaginginfo index state, bounded reads, indexed
channel discovery, and reorg/disconnect handling.

Uses regtest/temp datadirs only. Does not touch real chain data, send real
funds, or submit real blocks beyond regtest generate().
"""

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    assert_contains,
    assert_does_not_contain,
    assert_contains_pair,
    connect_all_nodes_bi,
    disconnect_all_nodes,
    get_rpc_proxy,
    rpc_url,
)
import threading
import time as _time


def _msg_channels(msgs):
    return sorted([m.get("channel") for m in msgs])


class MessageIndexTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [
            [],                     # node0: sender (default subscription mode)
            ['-messageindex=1'],    # node1: full message indexer
            [],                     # node2: default subscription mode (unchanged behavior)
        ]

    def activate_messaging(self):
        self.log.info("Activating messaging on regtest...")
        n0 = self.nodes[0]
        n0.generate(1)
        self.sync_all()
        n0.generate(431)
        self.sync_all()
        assert_equal("active", n0.getblockchaininfo()['bip9_softforks']['messaging_restricted']['status'])

    def test_index_info_fields(self):
        self.log.info("Testing getmessaginginfo index/rescan fields...")
        info = self.nodes[1].getmessaginginfo()
        assert_equal(True, info["messageindex"])
        assert_equal("all", info["message_index_mode"])
        assert "message_index_activation_height" in info
        assert "message_index_synced_height" in info
        assert "message_index_best_height" in info
        assert "message_index_synced" in info
        assert "message_index_needs_rescan" in info
        assert "pruned" in info
        # Indexer is caught up after activation (no message history yet).
        assert_equal(info["message_index_best_height"], info["message_index_synced_height"])
        assert_equal(True, info["message_index_synced"])
        assert_equal(False, info["message_index_needs_rescan"])

        # Default node reports subscription mode.
        info0 = self.nodes[2].getmessaginginfo()
        assert_equal(False, info0["messageindex"])
        assert_equal("subscription", info0["message_index_mode"])

    def test_full_index_and_default_unchanged(self):
        self.log.info("Testing -messageindex=1 stores unsubscribed messages; default does not...")
        n0, n1, n2 = self.nodes[0], self.nodes[1], self.nodes[2]

        owner = "MSGIDX"
        owner_channel = "MSGIDX!"
        chan_a = "MSGIDX~A"
        chan_b = "MSGIDX~B"
        ipfs = "QmZPGfJojdTzaqCWJu2m3krark38X1rqEHBo4SjeqHKB26"

        n0.issue(owner, 100)
        n0.issue(chan_a)
        n0.issue(chan_b)
        n0.generate(1)
        self.sync_all()

        # n1 (full indexer) has an empty wallet and no subscriptions.
        assert_equal([], n1.viewallmessagechannels())
        # n2 (default) has an empty wallet and no subscriptions.
        assert_equal([], n2.viewallmessagechannels())

        # Send one message on the owner channel and one on each sub-channel,
        # across multiple blocks so heights differ.
        n0.sendmessage(owner_channel, ipfs)
        n0.generate(1)
        self.sync_all()

        n0.sendmessage(chan_a, ipfs)
        n0.generate(1)
        self.sync_all()

        n0.sendmessage(chan_b, ipfs)
        n0.generate(1)
        self.sync_all()

        # n1 (messageindex=1) indexed ALL messages with no subscriptions.
        n1_msgs = n1.viewallmessages()
        assert_equal(3, len(n1_msgs))
        assert_contains(owner_channel, _msg_channels(n1_msgs))
        assert_contains(chan_a, _msg_channels(n1_msgs))
        assert_contains(chan_b, _msg_channels(n1_msgs))

        # n2 (default, no subscriptions) indexed NONE.
        assert_equal(0, len(n2.viewallmessages()))

        # New blocks are indexed automatically (no rescan needed): synced height
        # tracks the tip and needs_rescan stays false.
        info = n1.getmessaginginfo()
        assert_equal(info["message_index_best_height"], info["message_index_synced_height"])
        assert_equal(True, info["message_index_synced"])
        assert_equal(False, info["message_index_needs_rescan"])

        # n2 default behavior unchanged: subscribing to a channel only shows
        # NEW messages on that channel (no backfill).
        n2.subscribetochannel(chan_a)
        assert_equal(0, len(n2.viewallmessages()))  # no backfill of history
        n0.sendmessage(chan_a, ipfs)
        n0.generate(1)
        self.sync_all()
        n2_msgs = n2.viewallmessages()
        assert_equal(1, len(n2_msgs))
        assert_contains_pair("Asset Name", chan_a, n2_msgs[0])
        n2.unsubscribefromchannel(chan_a)
        n2.clearmessages()

        return {"owner": owner, "owner_channel": owner_channel, "chan_a": chan_a, "chan_b": chan_b, "ipfs": ipfs}

    def test_bounded_reads(self, ctx):
        self.log.info("Testing bounded/paginated viewallmessages and viewchannelmessages...")
        n1 = self.nodes[1]

        all_msgs = n1.viewallmessages()
        total = len(all_msgs)
        assert_greater_than(total, 2)

        # viewallmessages with count/offset is ordered by (height, outpoint).
        page1 = n1.viewallmessages(2, 0)
        assert_equal(2, len(page1))
        heights = [int(m["Block Height"]) for m in page1]
        assert_equal(heights, sorted(heights))
        # vout field is present for the paginated path
        assert "vout" in page1[0]

        page2 = n1.viewallmessages(2, 2)
        assert_equal(min(2, total - 2), len(page2))

        # offset beyond range returns empty
        assert_equal([], n1.viewallmessages(10, total + 100))

        # channel pattern filter with glob
        glob_msgs = n1.viewallmessages(100, 0, "MSGIDX~*")
        assert_greater_than(len(glob_msgs), 0)
        for m in glob_msgs:
            assert m["channel"].startswith("MSGIDX~")

        # exact channel (bare name normalized to owner form)
        owner_msgs = n1.viewallmessages(100, 0, ctx["owner"])
        for m in owner_msgs:
            assert_equal(ctx["owner_channel"], m["channel"])

        # height-range filter
        first_height = min(int(m["Block Height"]) for m in all_msgs)
        last_height = max(int(m["Block Height"]) for m in all_msgs)
        ranged = n1.viewallmessages(100, 0, "*", first_height, last_height - 1)
        for m in ranged:
            assert int(m["Block Height"]) <= last_height - 1

        # viewchannelmessages with count/offset
        chan_a_msgs = n1.viewchannelmessages(ctx["chan_a"], 100, 0)
        assert_greater_than(len(chan_a_msgs), 0)
        for m in chan_a_msgs:
            assert_equal(ctx["chan_a"], m["channel"])
        # bounded
        assert_equal(0, len(n1.viewchannelmessages(ctx["chan_a"], 0, 100)))
        # one page
        assert_equal(1, len(n1.viewchannelmessages(ctx["chan_a"], 1, 0)))

        # no-arg viewallmessages still works (backward compatible) and returns all
        assert_equal(total, len(n1.viewallmessages()))

    def test_indexed_channel_discovery(self, ctx):
        self.log.info("Testing viewindexedmessagechannels...")
        n1 = self.nodes[1]

        channels = n1.viewindexedmessagechannels()
        names = [c["channel"] for c in channels]
        assert_contains(ctx["owner_channel"], names)
        assert_contains(ctx["chan_a"], names)
        assert_contains(ctx["chan_b"], names)
        for c in channels:
            assert_greater_than(c["message_count"], 0)
            assert "first_height" in c
            assert "last_height" in c
            assert "last_time" in c

        # pattern filter
        filtered = n1.viewindexedmessagechannels("MSGIDX~*")
        for c in filtered:
            assert c["channel"].startswith("MSGIDX~")
        assert_does_not_contain(ctx["owner_channel"], [c["channel"] for c in filtered])

        # count/offset bounding
        one = n1.viewindexedmessagechannels("", 1, 0)
        assert_equal(1, len(one))
        rest = n1.viewindexedmessagechannels("", 100, 1)
        assert_equal(len(channels) - 1, len(rest))

    def test_rescan_after_clear(self, ctx):
        self.log.info("Testing rescanmessages backfill after clearmessages, dedup, and needs_rescan...")
        n1 = self.nodes[1]

        total = len(n1.viewallmessages())
        assert_greater_than(total, 0)

        n1.clearmessages()
        assert_equal(0, len(n1.viewallmessages()))

        # After clearing, the full-index marker is reset so needs_rescan is true.
        info = n1.getmessaginginfo()
        assert_equal(True, info["message_index_needs_rescan"])
        assert_equal(False, info["message_index_synced"])

        # Full backfill (no channel filter, messageindex=1 -> all channels).
        res = n1.rescanmessages()
        assert_equal(res["messages_added"], res["messages_found"])
        assert_equal(0, res["messages_skipped"])
        assert_equal(True, res["messageindex"])
        assert_equal(total, len(n1.viewallmessages()))

        # After backfill, index is synced and needs_rescan is false.
        info = n1.getmessaginginfo()
        assert_equal(False, info["message_index_needs_rescan"])
        assert_equal(True, info["message_index_synced"])
        assert_equal(info["message_index_best_height"], info["message_index_synced_height"])

        # Running rescan again must not duplicate: everything is skipped.
        res2 = n1.rescanmessages()
        assert_equal(res2["messages_found"], res2["messages_skipped"])
        assert_equal(0, res2["messages_added"])
        assert_equal(total, len(n1.viewallmessages()))

        # Last-scan metadata recorded.
        info = n1.getmessaginginfo()
        assert info["message_index_last_scan_start_height"] is not None
        assert info["message_index_last_scan_stop_height"] is not None
        assert_equal(True, info["message_index_last_scan_completed"])

    def test_rescan_channel_filter(self, ctx):
        self.log.info("Testing rescanmessages with a channel filter (does not mark full index synced)...")
        n1 = self.nodes[1]

        n1.clearmessages()
        assert_equal(0, len(n1.viewallmessages()))

        # After clearing, needs_rescan is true and synced is false.
        info = n1.getmessaginginfo()
        assert_equal(True, info["message_index_needs_rescan"])
        assert_equal(False, info["message_index_synced"])

        # Backfill only chan_a (bare name normalized to MSGIDX~A form, which is a
        # message-channel asset and used as-is).
        res = n1.rescanmessages(None, None, ctx["chan_a"])
        assert_greater_than(res["messages_found"], 0)
        assert_equal(res["messages_added"], res["messages_found"])
        assert_equal(ctx["chan_a"], res["channel"])
        assert_equal(True, res["completed"])

        msgs = n1.viewallmessages()
        assert_greater_than(len(msgs), 0)
        for m in msgs:
            assert_equal(ctx["chan_a"], m["channel"])

        # R2 FIX: A channel-filtered rescan must NOT mark the full all-message
        # index as synced. needs_rescan must remain true and synced must remain
        # false, because only one channel was backfilled — the rest of history
        # is still missing.
        info = n1.getmessaginginfo()
        assert_equal(True, info["message_index_needs_rescan"])
        assert_equal(False, info["message_index_synced"])

    def test_rescan_partial_height(self, ctx):
        self.log.info("Testing partial-height rescan does not mark full index synced...")
        n1 = self.nodes[1]

        n1.clearmessages()
        assert_equal(0, len(n1.viewallmessages()))

        activation = n1.getmessaginginfo()["message_index_activation_height"]
        best = n1.getblockcount()
        # Start well after activation so earlier history is skipped.
        partial_start = activation + 50
        if partial_start >= best:
            partial_start = activation + 1

        res = n1.rescanmessages(partial_start, None)
        assert_equal(True, res["completed"])

        # R2 FIX: A partial-height rescan that starts after activation must NOT
        # mark the full index as synced because [activation, partial_start-1] is
        # missing. needs_rescan must remain true.
        info = n1.getmessaginginfo()
        assert_equal(True, info["message_index_needs_rescan"])
        assert_equal(False, info["message_index_synced"])

        # Now do a full unfiltered rescan from activation — this SHOULD clear
        # needs_rescan and mark the index synced.
        res2 = n1.rescanmessages()
        assert_equal(True, res2["completed"])
        info = n1.getmessaginginfo()
        assert_equal(False, info["message_index_needs_rescan"])
        assert_equal(True, info["message_index_synced"])
        assert_equal(info["message_index_best_height"], info["message_index_synced_height"])

    def test_rescan_messageindex0_no_synced_metadata(self, ctx):
        self.log.info("Testing -messageindex=0 rescans do not update full-index synced metadata...")
        n2 = self.nodes[2]

        # node2 is in default (subscription) mode. It should not track or update
        # the full-index synced height.
        info = n2.getmessaginginfo()
        assert_equal(False, info["messageindex"])
        assert_equal("subscription", info["message_index_mode"])
        # synced_height is null (not tracked) under subscription mode.
        assert info["message_index_synced_height"] is None

        # A rescan under -messageindex=0 should work (store subscribed-channel
        # messages) but must not update full-index synced metadata.
        res = n2.rescanmessages()
        assert_equal(False, res["messageindex"])

        info = n2.getmessaginginfo()
        assert_equal(False, info["messageindex"])
        assert info["message_index_synced_height"] is None

    def test_undo_extraction_consistency(self, ctx):
        self.log.info("Testing rescan extraction matches live indexing (undo-based ownership filter)...")
        n0, n1 = self.nodes[0], self.nodes[1]

        owner = "OWNFILT"
        owner_channel = "OWNFILT!"
        ipfs = "QmZPGfJojdTzaqCWJu2m3krark38X1rqEHBo4SjeqHKB26"

        # Issue the owner asset so node0 owns the channel.
        n0.issue(owner, 100)
        n0.generate(1)
        self.sync_all()

        # 1. Normal message: sendmessage sends from the address that holds the
        #    owner token to the SAME address. The ownership filter passes, so
        #    live indexing stores this message.
        n0.sendmessage(owner_channel, ipfs)
        n0.generate(1)
        self.sync_all()

        live_msgs = [m for m in n1.viewallmessages() if m["channel"] == owner_channel]
        assert_equal(1, len(live_msgs))

        # 2. Transfer the owner token to a DIFFERENT address with a message
        #    payload. The ownership filter in CheckTxAssets checks that the input
        #    asset address matches the output address. Since the owner token
        #    input is at address A but the transfer goes to address B, the filter
        #    rejects the message — live indexing does NOT store it.
        new_addr = n0.getnewaddress()
        n0.transfer(owner_channel, 1, new_addr, ipfs)
        n0.generate(1)
        self.sync_all()

        # Live indexing should still show only 1 message (the normal one).
        # The transfer-with-message to a different address was filtered out.
        live_msgs = [m for m in n1.viewallmessages() if m["channel"] == owner_channel]
        assert_equal(1, len(live_msgs))

        # 3. Now clear and rescan. The undo-based extraction should apply the
        #    same ownership filter and also produce only 1 message (not 2).
        n1.clearmessages()
        assert_equal(0, len(n1.viewallmessages()))

        res = n1.rescanmessages()
        assert_equal(True, res["completed"])

        rescan_msgs = [m for m in n1.viewallmessages() if m["channel"] == owner_channel]
        assert_equal(1, len(rescan_msgs))
        # The rescan result should match live indexing exactly.
        assert_equal(len(live_msgs), len(rescan_msgs))

    def test_contiguous_sync_enable_then_block(self, ctx):
        self.log.info("Testing enable -messageindex=1 over history, then new block before rescan...")
        n2 = self.nodes[2]

        # node2 was synced without -messageindex. Restart with -messageindex=1.
        self.restart_node(2, ['-messageindex=1'])
        connect_all_nodes_bi(self.nodes)
        self.sync_all()

        info = n2.getmessaginginfo()
        assert_equal(True, info["messageindex"])
        assert_equal(True, info["message_index_needs_rescan"])
        assert_equal(False, info["message_index_synced"])
        synced_before = info["message_index_synced_height"]
        assert synced_before is None or synced_before < info["message_index_best_height"]

        # Mine a new block BEFORE running rescan. R7: the synced height must NOT
        # jump to the new tip because historical coverage is missing.
        self.nodes[0].generate(1)
        self.sync_all()

        info = n2.getmessaginginfo()
        assert_equal(True, info["message_index_needs_rescan"])
        assert_equal(False, info["message_index_synced"])
        # Synced height must not have jumped to the new best height.
        if info["message_index_synced_height"] is not None:
            assert info["message_index_synced_height"] < info["message_index_best_height"], \
                "synced height must not jump to tip after enabling over history without rescan"

        # Now run the full rescan to close the gap.
        res = n2.rescanmessages()
        assert_equal(True, res["completed"])
        assert_equal(False, n2.getmessaginginfo()["message_index_needs_rescan"])
        assert_equal(True, n2.getmessaginginfo()["message_index_synced"])

    def test_contiguous_sync_clear_then_block(self, ctx):
        self.log.info("Testing clearmessages then new block before rescan...")
        n1 = self.nodes[1]

        # Ensure the index is currently synced.
        info = n1.getmessaginginfo()
        assert_equal(False, info["message_index_needs_rescan"])

        # Clear messages — resets the synced height to 0, creating a gap.
        n1.clearmessages()
        assert_equal(0, len(n1.viewallmessages()))
        assert_equal(True, n1.getmessaginginfo()["message_index_needs_rescan"])

        # Mine a new block BEFORE rescan. R7: the synced height must NOT advance
        # because coverage from activation through the current tip is missing.
        self.nodes[0].generate(1)
        self.sync_all()

        info = n1.getmessaginginfo()
        assert_equal(True, info["message_index_needs_rescan"])
        assert_equal(False, info["message_index_synced"])

        # Full rescan closes the gap.
        res = n1.rescanmessages()
        assert_equal(True, res["completed"])
        assert_equal(False, n1.getmessaginginfo()["message_index_needs_rescan"])

    def test_contiguous_sync_rescan_then_block(self, ctx):
        self.log.info("Testing successful full rescan then new block advances normally...")
        n1 = self.nodes[1]

        # Ensure the index is synced (from prior tests).
        assert_equal(False, n1.getmessaginginfo()["message_index_needs_rescan"])

        synced_before = n1.getmessaginginfo()["message_index_synced_height"]

        # Mine a new block. R7: since the index is contiguous, the synced
        # height should advance to the new tip and needs_rescan stays false.
        self.nodes[0].generate(1)
        self.sync_all()

        info = n1.getmessaginginfo()
        assert_equal(False, info["message_index_needs_rescan"])
        assert_equal(True, info["message_index_synced"])
        assert info["message_index_synced_height"] > synced_before, "synced height should have advanced"

    def test_enable_after_history(self, ctx):
        self.log.info("Testing enable -messageindex=1 after history was already synced...")
        n2 = self.nodes[2]

        # node2 already restarted with -messageindex=1 in
        # test_contiguous_sync_enable_then_block and fully rescanned. Verify it
        # has the full index including never-subscribed channels.
        info = n2.getmessaginginfo()
        assert_equal(True, info["messageindex"])
        assert_equal(False, info["message_index_needs_rescan"])

        recovered = _msg_channels(n2.viewallmessages())
        assert_contains(ctx["owner_channel"], recovered)
        assert_contains(ctx["chan_a"], recovered)
        assert_contains(ctx["chan_b"], recovered)

    def test_reorg_messageindex(self, ctx):
        self.log.info("Testing reorg/disconnect orphans indexed messages under -messageindex=1...")
        n0, n1 = self.nodes[0], self.nodes[1]

        # Fund node1 with an owner asset so it can send a message on its own
        # channel and be the messageindex node that reorgs.
        owner = "REORGIDX"
        owner_channel = "REORGIDX!"
        ipfs = "QmYwAPJzv5CZsnA625s3Xf2nemtYgPCpHXhJjQcsnA625s"

        n0.issue(owner, 100)
        n0.generate(1)
        self.sync_all()

        n1_addr = n1.getnewaddress()

        # node1 needs HEMP to pay the sendmessage fee.
        n0.sendtoaddress(n1_addr, 10)
        n0.generate(1)
        self.sync_all()

        # Transfer the owner token to node1 so it owns the channel.
        n0.transfer(owner_channel, 1, n1_addr)
        n0.generate(1)
        self.sync_all()

        # Confirm node1 indexed nothing yet for this channel and is synced.
        assert_equal([], [m for m in n1.viewallmessages() if m["channel"] == owner_channel])

        # Both synced at height G. Disconnect, then:
        #  - node1 sends a message and mines 1 block (message block M at G+1)
        #  - node0 mines 2 empty blocks (G+1, G+2) -> longer chain
        disconnect_all_nodes(self.nodes)
        base_height = n1.getblockcount()

        n1.sendmessage(owner_channel, ipfs)
        n1.generate(1)
        assert_equal(base_height + 1, n1.getblockcount())

        n0.generate(2)
        assert_equal(base_height + 2, n0.getblockcount())

        # node1 (messageindex=1) indexed the message from its block M.
        msgs = n1.viewallmessages()
        reorg_msgs = [m for m in msgs if m["channel"] == owner_channel]
        assert_equal(1, len(reorg_msgs))
        assert_contains_pair("Status", "UNREAD", reorg_msgs[0])
        reorg_txid = reorg_msgs[0]["txid"]

        # Reconnect: node1 reorgs to node0's longer chain, disconnecting block M.
        # Use sync_all (which relays blocks by RPC) to force the reorg promptly
        # instead of wait_for_block_sync, which would time out on divergent heights.
        connect_all_nodes_bi(self.nodes)
        self.sync_all()
        assert_equal(base_height + 2, n1.getblockcount())
        assert_equal(n0.getbestblockhash(), n1.getbestblockhash())

        # The message from the disconnected block must be orphaned/removed.
        msgs_after = n1.viewallmessages()
        reorg_after = [m for m in msgs_after if m["channel"] == owner_channel and m["txid"] == reorg_txid]
        assert_equal(1, len(reorg_after))
        assert_equal("ORPHAN", reorg_after[0]["Status"])

    def test_concurrent_rescan_rejection(self, ctx):
        self.log.info("Testing concurrent rescanmessages rejection...")
        n1 = self.nodes[1]

        # Ensure no rescan is in progress and the DB is in a known state.
        info = n1.getmessaginginfo()
        assert_equal(False, info["message_rescan_in_progress"])

        n1.clearmessages()
        # Generate enough blocks that a full rescan takes a measurable amount of
        # time, so the in-progress flag is observable from a second RPC thread.
        n1.generate(1500)
        self.sync_all()

        # The default node RPC proxy (n1) shares one HTTP connection, so a
        # background rescan must use its own AuthServiceProxy to avoid
        # CannotSendRequest on the shared connection.
        bg_proxy = get_rpc_proxy(rpc_url(n1.datadir, n1.index, n1.rpchost), n1.index, timeout=120)

        finished = threading.Event()
        first_result = {}

        def slow_rescan():
            try:
                first_result["res"] = bg_proxy.rescanmessages()
            except Exception as e:
                first_result["err"] = e
            finally:
                finished.set()

        t = threading.Thread(target=slow_rescan)
        t.daemon = True
        t.start()

        # Give the background rescan a moment to set the in-progress flag.
        # Poll getmessaginginfo (on the shared proxy) until the flag is true.
        deadline = _time.time() + 10
        seen_in_progress = False
        while _time.time() < deadline:
            info = n1.getmessaginginfo()
            if info["message_rescan_in_progress"]:
                seen_in_progress = True
                break
            _time.sleep(0.05)

        if not seen_in_progress:
            # The rescan may have already finished (fast machine). In that case we
            # cannot test concurrent rejection on this run; verify it completed
            # cleanly and skip the concurrent assertion.
            finished.wait(timeout=30)
            t.join(timeout=5)
            assert "res" in first_result, "background rescan should have completed: %s" % first_result
            self.log.info("  (rescan completed too fast to test concurrency on this chain; verified clean completion)")
            return

        # While the first rescan is in progress, a second rescan must be rejected.
        self.log.info("  (observing rescan in progress; testing concurrent rejection)")
        assert_raises_rpc_error(-1, "already in progress", n1.rescanmessages)

        # Wait for the first rescan to finish.
        finished.wait(timeout=60)
        t.join(timeout=5)

        # The first rescan should have completed successfully.
        assert "res" in first_result, "background rescan failed unexpectedly: %s" % first_result
        assert_equal(True, first_result["res"]["completed"])

        # After completion, in-progress is cleared.
        info = n1.getmessaginginfo()
        assert_equal(False, info["message_rescan_in_progress"])

    def run_test(self):
        self.activate_messaging()
        self.test_index_info_fields()
        ctx = self.test_full_index_and_default_unchanged()
        self.test_bounded_reads(ctx)
        self.test_indexed_channel_discovery(ctx)
        self.test_rescan_after_clear(ctx)
        self.test_rescan_channel_filter(ctx)
        self.test_rescan_partial_height(ctx)
        self.test_rescan_messageindex0_no_synced_metadata(ctx)
        self.test_undo_extraction_consistency(ctx)
        self.test_concurrent_rescan_rejection(ctx)
        self.test_contiguous_sync_enable_then_block(ctx)
        self.test_contiguous_sync_clear_then_block(ctx)
        self.test_contiguous_sync_rescan_then_block(ctx)
        self.test_enable_after_history(ctx)
        self.test_reorg_messageindex(ctx)


if __name__ == '__main__':
    MessageIndexTest().main()
