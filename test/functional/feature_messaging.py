#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Raven Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Testing messaging
"""

from test_framework.test_framework import Hemp0xTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error, assert_contains, assert_does_not_contain, assert_contains_pair

class MessagingTest(Hemp0xTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.extra_args = [
            ['-assetindex'],
            ['-assetindex'],
            ['-assetindex'],
            ['-assetindex', '-disablemessaging=1'],
        ]

    def activate_messaging(self):
        self.log.info("Generating HEMP for node[0] and activating messaging...")
        n0 = self.nodes[0]

        n0.generate(1)
        self.sync_all()
        n0.generate(431)
        self.sync_all()
        assert_equal("active", n0.getblockchaininfo()['bip9_softforks']['messaging_restricted']['status'])

    def test_getmessaginginfo(self):
        self.log.info("Testing getmessaginginfo()...")
        n0 = self.nodes[0]
        info = n0.getmessaginginfo()

        assert_equal(True, info["enabled"])
        assert_equal(True, info["messaging_active"])
        assert_equal(True, info["restricted_active"])
        assert_equal(1, info["activation_block"])
        assert_equal(True, info["databases_available"])
        assert_equal(True, info["caches_available"])
        assert_equal(True, info["wallet_available"])
        assert isinstance(info["message_count"], int)
        assert isinstance(info["channel_count"], int)
        assert_greater_than(info["dirty_cache_size_bytes"], -1)
        assert isinstance(info["warnings"], list)

    def test_disabled_messaging_node(self):
        self.log.info("Testing disabled-messaging node behavior...")
        n3 = self.nodes[3]

        info = n3.getmessaginginfo()
        assert_equal(False, info["enabled"])
        assert isinstance(info["warnings"], list)
        assert "Messaging is disabled via -disablemessaging" in info["warnings"]

        msgs = n3.viewallmessages()
        assert isinstance(msgs, list)
        assert_equal(0, len(msgs))

        channels = n3.viewallmessagechannels()
        assert isinstance(channels, list)
        assert_equal(0, len(channels))

        assert_raises_rpc_error(-20, "Messaging is disabled", n3.subscribetochannel, "CHANNEL!")
        assert_raises_rpc_error(-20, "Messaging is disabled", n3.unsubscribefromchannel, "CHANNEL!")
        assert_raises_rpc_error(-20, "Messaging is disabled", n3.clearmessages)

    def test_help_without_activation(self):
        self.log.info("Testing help text is visible regardless of activation...")
        n0 = self.nodes[0]
        help_text = n0.help()
        assert_contains("viewallmessages", help_text)
        assert_contains("getmessaginginfo", help_text)
        assert_contains("unsubscribefromchannel", help_text)
        assert_contains("clearmessages", help_text)
        assert_contains("subscribetochannel", help_text)

    def test_messaging(self):
        self.log.info("Testing messaging!")
        n0, n1 = self.nodes[0], self.nodes[1]

        spam_name = "SPAM"
        asset_name = "MESSAGING"
        owner_name = "MESSAGING!"
        channel_one = "MESSAGING~ONE"
        channel_two = "MESSAGING~TWO"
        ipfs_hash = "QmZPGfJojdTzaqCWJu2m3krark38X1rqEHBo4SjeqHKB26"

        # need ownership before channels can be created
        assert_raises_rpc_error(-32600, "Wallet doesn't have asset: " + owner_name,
            n0.issue, channel_one)

        n0.issue(asset_name, 100)
        n0.issue(channel_one)
        n0.issue(channel_two)
        n0.issue(spam_name, 100)

        n0.generate(1)
        self.sync_all()

        # you're auto-subscribed to your own channels
        n0_channels = n0.viewallmessagechannels()
        assert_contains(owner_name, n0_channels)
        assert_contains(channel_one, n0_channels)
        assert_contains(channel_two, n0_channels)

        # n1 subscribes to owner and channel one
        assert_equal([], n1.viewallmessagechannels())
        n1.subscribetochannel(owner_name)
        n1.subscribetochannel(channel_one)
        n1_channels = n1.viewallmessagechannels()
        assert_contains(owner_name, n1_channels)
        assert_contains(channel_one, n1_channels)
        assert_does_not_contain(channel_two, n1_channels)

        # n0 sends a message on owner
        n0.sendmessage(owner_name, ipfs_hash)
        n0.generate(1)
        self.sync_all()

        # n1 views then clears messages
        n1_messages = n1.viewallmessages()
        assert_equal(1, len(n1_messages))
        message = n1_messages[0]
        assert_contains_pair("Asset Name", owner_name, message)
        assert_contains_pair("Message", ipfs_hash, message)
        n1.clearmessages()
        n1_messages = n1.viewallmessages()
        assert_equal(0, len(n1_messages))

        # n0 sends more messages on channels one and two
        n0.sendmessage(channel_one, ipfs_hash)
        n0.sendmessage(channel_two, ipfs_hash)
        n0.generate(1)
        self.sync_all()

        # n1 views then clears messages
        n1_messages = n1.viewallmessages()
        assert_equal(1, len(n1_messages))
        message = n1_messages[0]
        assert_contains_pair("Asset Name", channel_one, message)
        assert_contains_pair("Message", ipfs_hash, message)
        n1.clearmessages()
        n1_messages = n1.viewallmessages()
        assert_equal(0, len(n1_messages))

        # n1 unsubscribes
        n1.unsubscribefromchannel(owner_name)
        n1.unsubscribefromchannel(channel_one)
        assert_equal(0, len(n1.viewallmessagechannels()))

        # auto-subscribe / spam protection (first address use only)
        addr1 = n1.getnewaddress()
        n0.transfer(asset_name, 10, addr1)
        n0.generate(1)
        self.sync_all()
        n0.transfer(spam_name, 10, addr1)
        n1_channels = n1.viewallmessagechannels()
        assert_equal(1, len(n1_channels))
        assert_contains(owner_name, n1_channels)
        assert_does_not_contain(spam_name, n1_channels)
        n1.unsubscribefromchannel(owner_name)

        # pre-existing messages (don't see w/o rescan)
        assert_equal(0, len(n1.viewallmessages()))
        n0.sendmessage(channel_two, ipfs_hash)
        n0.generate(1)
        self.sync_all()
        assert_equal(0, len(n1.viewallmessages()))
        n1.subscribetochannel(channel_two)
        assert_equal(0, len(n1.viewallmessages()))
        n0.sendmessage(channel_two, ipfs_hash)
        n0.generate(1)
        self.sync_all()
        assert_equal(1, len(n1.viewallmessages()))
        assert_contains_pair("Asset Name", channel_two, n1.viewallmessages()[0])
        n1.clearmessages()
        n1.unsubscribefromchannel(channel_two)


    def run_test(self):
        self.test_help_without_activation()
        self.activate_messaging()
        self.test_getmessaginginfo()
        self.test_disabled_messaging_node()
        self.test_messaging()


if __name__ == '__main__':
    MessagingTest().main()
